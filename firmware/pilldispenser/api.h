// REST API (JSON over HTTP, port 80). Full description: docs/openapi.yaml
#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include "state.h"
#include "motion.h"

static WebServer server(80);
static bool rebootPending = false;
static uint32_t rebootAt = 0;

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Pill dispenser</title>
<style>body{font:15px system-ui,sans-serif;margin:16px;max-width:640px}button{font:inherit;padding:6px 10px;margin:2px}
table{border-collapse:collapse;width:100%}td,th{border-bottom:1px solid #ddd;padding:6px 4px;text-align:left}
pre{background:#f4f4f4;padding:8px;overflow:auto}input{font:inherit;padding:4px;width:14em}</style></head><body>
<h2>Pill dispenser</h2><p>API key <input id="k" placeholder="only if one is set"> <button onclick="save()">use</button></p>
<p id="st">...</p><table id="t"></table>
<p><button onclick="post('home')">Home</button><button onclick="post('stop')">Stop</button>
<a href="/setup">Wi-Fi setup</a></p><h3>Last events</h3><pre id="ev"></pre>
<script>
let key=localStorage.getItem('pdkey')||'';document.getElementById('k').value=key;
function save(){key=document.getElementById('k').value;localStorage.setItem('pdkey',key);load()}
function api(m,p,b){return fetch('/api/v1/'+p,{method:m,headers:Object.assign({'Content-Type':'application/json'},key?{'Authorization':'Bearer '+key}:{}),body:b?JSON.stringify(b):undefined}).then(r=>r.json())}
function post(p,b){api('POST',p,b||{}).then(load)}
function load(){api('GET','status').then(s=>{document.getElementById('st').textContent=s.error&&!s.state?s.error:
'State: '+s.state+(s.homed?'':' (not homed)')+' | slot at station: '+s.slot_at_station+' | '+(s.error||'ok')});
api('GET','slots').then(d=>{let h='<tr><th>#</th><th>Name</th><th>Left</th><th></th></tr>';(d.slots||[]).forEach(s=>{
h+='<tr><td>'+s.slot+'</td><td>'+(s.name||'-')+'</td><td>'+(s.count<0?'?':s.count)+(s.low_stock?' (low)':'')+'</td><td>'+
'<button onclick="post(\'dispense\',{items:[{slot:'+s.slot+',count:1}]})">1 pill</button>'+
'<button onclick="post(\'present\',{slot:'+s.slot+'})">refill pos.</button></td></tr>'});document.getElementById('t').innerHTML=h});
api('GET','events?limit=12').then(d=>{document.getElementById('ev').textContent=(d.events||[]).reverse().map(e=>e.seq+' '+e.type+' '+(e.slot?'slot '+e.slot+' ':'')+e.message).join('\n')})}
load();setInterval(load,3000);
</script></body></html>)HTML";

static const char SETUP_HTML[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Wi-Fi setup</title>
<style>body{font:16px system-ui,sans-serif;margin:20px;max-width:420px}input,button{font:inherit;padding:8px;width:100%;margin:6px 0;box-sizing:border-box}</style>
</head><body><h2>Pill dispenser Wi-Fi</h2><form method="post" action="/setup">
<label>Network name (SSID)<input name="ssid" required></label>
<label>Password<input name="password" type="password"></label>
<button>Save and restart</button></form></body></html>)HTML";

// ---------------------------------------------------------------- helpers
static void cors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
}

static void sendJson(int code, JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  cors();
  server.send(code, "application/json", out);
}

static void sendErr(int code, const char *msg) {
  JsonDocument d;
  d["error"] = msg;
  sendJson(code, d);
}

static void sendOk(const char *msg = "ok") {
  JsonDocument d;
  d["result"] = msg;
  sendJson(200, d);
}

static bool authed() {
  if (!cfg.api_key[0]) return true;
  String h = server.header("Authorization");
  if (h.startsWith("Bearer ") && h.substring(7) == cfg.api_key) return true;
  return server.header("X-API-Key") == cfg.api_key;
}

static bool readBody(JsonDocument &doc) {
  if (server.hasArg("plain")) return !deserializeJson(doc, server.arg("plain"));
  // body sent without "Content-Type: application/json" (plain curl -d): the server has parsed it as a
  // form, the JSON text then arrives as the name of the first argument. Best effort, simple bodies only.
  if (server.args() > 0 && server.argName(0).startsWith("{")) return !deserializeJson(doc, server.argName(0));
  return false;
}

static Job *findJob(uint32_t id) {
  for (auto &j : jobs) if (j.state != JOB_FREE && j.id == id) return &j;
  return nullptr;
}

static Schedule *findSchedule(uint16_t id) {
  for (auto &s : schedules) if (s.used && s.id == id) return &s;
  return nullptr;
}

static void dueRemove(uint16_t id) {
  for (int i = 0; i < MAX_DUE; i++) if (dueList[i] == id) dueList[i] = 0;
}

// queue the items of a schedule as a job
static uint32_t runSchedule(Schedule &s, const char *source) {
  dueRemove(s.id);
  return motion::enqueueJob(s.items, s.n, source, "");
}

// ---------------------------------------------------------------- handlers
static void hStatus() {
  JsonDocument d;
  d["device"] = cfg.hostname;
  d["firmware"] = FW_VERSION;
  d["state"] = motion::MSTATE_NAME[motion::state];
  d["homed"] = (bool)motion::homed;
  d["slot_at_station"] = motion::slotAtStation() + 1;
  d["current_job"] = (uint32_t)motion::currentJob;
  d["error"] = motion::state == motion::M_ERROR ? motion::lastError : "";
  d["slots"] = N_SLOTS;
  d["time"] = nowUnix();
  d["uptime_s"] = millis() / 1000;
  d["last_event_seq"] = eventSeq;
  JsonArray due = d["due"].to<JsonArray>();
  for (int i = 0; i < MAX_DUE; i++) if (dueList[i]) due.add(dueList[i]);
  JsonObject w = d["wifi"].to<JsonObject>();
  w["ssid"] = WiFi.SSID();
  w["ip"] = WiFi.localIP().toString();
  w["rssi"] = WiFi.RSSI();
  d["free_heap"] = ESP.getFreeHeap();
  sendJson(200, d);
}

static void hSlots(const String &sub, HTTPMethod m) {
  Lock l;
  JsonDocument d;
  if (sub.length() == 0) {
    JsonArray a = d["slots"].to<JsonArray>();
    for (int i = 0; i < N_SLOTS; i++) slotToJson(i, a.add<JsonObject>());
    return sendJson(200, d);
  }
  int n = sub.toInt();
  if (n < 1 || n > N_SLOTS) return sendErr(404, "no such slot");
  if (m == HTTP_PUT || m == HTTP_POST) {
    JsonDocument b;
    if (!readBody(b)) return sendErr(400, "invalid JSON");
    slotFromJson(n - 1, b.as<JsonObjectConst>());
    saveConfig();
  }
  slotToJson(n - 1, d.to<JsonObject>());
  sendJson(200, d);
}

static void hDispense() {
  JsonDocument b;
  if (!readBody(b)) return sendErr(400, "invalid JSON");
  Item items[MAX_ITEMS];
  uint8_t n = 0;
  String e = itemsFromJson(b["items"].as<JsonArrayConst>(), items, n);
  if (e.length()) return sendErr(400, e.c_str());
  bool existing = false;
  uint32_t id = motion::enqueueJob(items, n, "api", b["request_id"] | "", &existing);
  if (!id) return sendErr(503, "job queue full");
  Lock l;
  JsonDocument d;
  Job *j = findJob(id);
  if (j) jobToJson(*j, d.to<JsonObject>());
  sendJson(existing ? 200 : 202, d);
}

static void hJobs(const String &sub, const String &action, HTTPMethod m) {
  Lock l;
  JsonDocument d;
  if (sub.length() == 0) {
    JsonArray a = d["jobs"].to<JsonArray>();
    for (uint32_t id = jobSeq; id > 0 && a.size() < MAX_JOBS; id--) {
      Job *j = findJob(id);
      if (j) jobToJson(*j, a.add<JsonObject>());
    }
    return sendJson(200, d);
  }
  Job *j = findJob(strtoul(sub.c_str(), nullptr, 10));
  if (!j) return sendErr(404, "no such job");
  if (action == "cancel" && m == HTTP_POST) {
    if (j->state != JOB_QUEUED) return sendErr(409, "job is not queued (use /stop for the running job)");
    j->state = JOB_CANCELLED;
    j->t_done = nowUnix();
  }
  jobToJson(*j, d.to<JsonObject>());
  sendJson(200, d);
}

static void hStop() {
  {
    Lock l;
    for (auto &j : jobs) if (j.state == JOB_QUEUED) { j.state = JOB_CANCELLED; j.t_done = nowUnix(); }
  }
  if (motion::busy()) motion::abortFlag = true;
  addEvent("stop", "stop requested");
  sendOk("stopping");
}

static void hEvents() {
  Lock l;
  uint32_t since = server.hasArg("since") ? strtoul(server.arg("since").c_str(), nullptr, 10) : 0;
  int limit = server.hasArg("limit") ? server.arg("limit").toInt() : MAX_EVENTS;
  if (limit < 1 || limit > MAX_EVENTS) limit = MAX_EVENTS;
  uint32_t first = eventSeq > (uint32_t)MAX_EVENTS ? eventSeq - MAX_EVENTS + 1 : 1;
  if (!server.hasArg("since") && eventSeq >= (uint32_t)limit) since = eventSeq - limit;   // newest "limit" events
  if (since + 1 > first) first = since + 1;
  JsonDocument d;
  JsonArray a = d["events"].to<JsonArray>();
  for (uint32_t s = first; s <= eventSeq && (int)a.size() < limit; s++) {
    const Event &e = events[(s - 1) % MAX_EVENTS];
    if (e.seq == s) eventToJson(e, a.add<JsonObject>());
  }
  d["last_seq"] = eventSeq;
  sendJson(200, d);
}

static void hSchedules(const String &sub, const String &action, HTTPMethod m) {
  Lock l;
  JsonDocument d;
  if (sub.length() == 0) {
    if (m == HTTP_POST) {
      JsonDocument b;
      if (!readBody(b)) return sendErr(400, "invalid JSON");
      Schedule *s = nullptr;
      for (auto &x : schedules) if (!x.used) { s = &x; break; }
      if (!s) return sendErr(507, "schedule table full");
      *s = Schedule();
      String e = scheduleFromJson(b.as<JsonObjectConst>(), *s, false);
      if (e.length()) return sendErr(400, e.c_str());
      s->used = true;
      s->id = ++scheduleSeq;
      saveSchedules();
      scheduleToJson(*s, d.to<JsonObject>());
      return sendJson(201, d);
    }
    JsonArray a = d["schedules"].to<JsonArray>();
    for (auto &s : schedules) if (s.used) scheduleToJson(s, a.add<JsonObject>());
    return sendJson(200, d);
  }
  Schedule *s = findSchedule((uint16_t)sub.toInt());
  if (!s) return sendErr(404, "no such schedule");
  if (action == "run" && m == HTTP_POST) {
    uint32_t id = runSchedule(*s, "api");
    if (!id) return sendErr(503, "job queue full");
    Job *j = findJob(id);
    if (j) jobToJson(*j, d.to<JsonObject>());
    return sendJson(202, d);
  }
  if (m == HTTP_DELETE) {
    dueRemove(s->id);
    s->used = false;
    saveSchedules();
    return sendOk("deleted");
  }
  if (m == HTTP_PUT) {
    JsonDocument b;
    if (!readBody(b)) return sendErr(400, "invalid JSON");
    Schedule tmp = *s;
    String e = scheduleFromJson(b.as<JsonObjectConst>(), tmp, true);
    if (e.length()) return sendErr(400, e.c_str());
    *s = tmp;
    saveSchedules();
  }
  scheduleToJson(*s, d.to<JsonObject>());
  sendJson(200, d);
}

static void hSettings(HTTPMethod m) {
  Lock l;
  if (m == HTTP_PUT || m == HTTP_POST) {
    JsonDocument b;
    if (!readBody(b)) return sendErr(400, "invalid JSON");
    settingsFromJson(b.as<JsonObjectConst>());
    saveConfig();
    configTzTime(cfg.tz, "pool.ntp.org", "time.cloudflare.com");
  }
  JsonDocument d;
  settingsToJson(d.to<JsonObject>(), false);
  sendJson(200, d);
}

static void hMove(const String &what) {
  JsonDocument b;
  readBody(b);
  if (what == "home") return motion::send(motion::CMD_HOME) ? sendOk("homing") : sendErr(503, "busy");
  if (what == "park") return motion::send(motion::CMD_PARK) ? sendOk("parking") : sendErr(503, "busy");
  if (what == "present" || what == "goto") {
    long slot = b["slot"] | 0L;
    if (slot < 1 || slot > N_SLOTS) return sendErr(400, "slot out of range");
    bool ok = motion::send(what == "present" ? motion::CMD_PRESENT : motion::CMD_GOTO, slot - 1);
    return ok ? sendOk("moving") : sendErr(503, "busy");
  }
  if (what == "jog") {
    long steps = b["steps"] | 0L;
    if (steps == 0 || labs(steps) > 20000) return sendErr(400, "steps must be -20000..20000, not 0");
    return motion::send(motion::CMD_JOG, steps) ? sendOk("jogging") : sendErr(503, "busy");
  }
  if (what == "servo") {
    long us = b["us"] | 0L;
    const char *pos = b["position"] | "";
    if (!strcmp(pos, "park")) us = cfg.servo_park_us;
    if (!strcmp(pos, "end")) us = cfg.servo_end_us;
    if (us < 450 || us > 2550) return sendErr(400, "give us (450..2550) or position park|end");
    return motion::send(motion::CMD_SERVO, us) ? sendOk("servo moving") : sendErr(503, "busy");
  }
  sendErr(404, "not found");
}

static void hCalibrate(const String &what) {
  if (motion::busy()) return sendErr(409, "busy");
  if (what == "slot1") {                       // "slot 1 is exactly at the station right now"
    Lock l;
    cfg.home_offset = cfg.home_offset - motion::st_pos;
    motion::st_pos = 0;
    saveConfig();
    JsonDocument d;
    d["home_offset"] = cfg.home_offset;
    return sendJson(200, d);
  }
  sendErr(404, "not found");
}

static void hDiag() {
  JsonDocument d;
  if (!motion::busy()) { motion::beamTest(); digitalWrite(PIN_BEAM_LED, LOW); }   // emitter off / on, receiver must follow
  d["beam_ok"] = motion::bm_ok;
  d["beam_level_lit"] = (int)motion::bm_lit;
  d["beam_level_dark"] = (int)motion::bm_dark;
  d["beam_last_count"] = (uint32_t)motion::bm_events;
  d["home_level"] = digitalRead(PIN_HOME);
  d["home_active"] = motion::homeActive();
  d["servo_us"] = (int)motion::sv_us;
  d["position_steps"] = (int32_t)motion::st_pos;
  d["steps_per_slot"] = STEPS_PER_SLOT;
  sendJson(200, d);
}

static void hWifi() {
  JsonDocument b;
  String ssid, pass;
  if (readBody(b)) { ssid = b["ssid"] | ""; pass = b["password"] | ""; }
  else { ssid = server.arg("ssid"); pass = server.arg("password"); }
  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64) return sendErr(400, "ssid/password invalid");
  copyStr(cfg.wifi_ssid, sizeof(cfg.wifi_ssid), ssid.c_str());
  copyStr(cfg.wifi_pass, sizeof(cfg.wifi_pass), pass.c_str());
  saveConfig();
  rebootPending = true;
  rebootAt = millis() + 1500;
  sendOk("saved, restarting");
}

// ---------------------------------------------------------------- router
static void dispatch() {
  String uri = server.uri();
  HTTPMethod m = server.method();
  if (m == HTTP_OPTIONS) { cors(); server.send(204); return; }
  if (uri == "/") { server.send_P(200, "text/html", INDEX_HTML); return; }
  if (uri == "/setup") {
    if (m == HTTP_POST) {
      // the setup page may only be used while the device is in setup (access point) mode or without an API key
      if (cfg.api_key[0] && WiFi.getMode() == WIFI_STA) return sendErr(403, "use POST /api/v1/wifi with the API key");
      return hWifi();
    }
    server.send_P(200, "text/html", SETUP_HTML);
    return;
  }
  if (!uri.startsWith("/api/v1/")) return sendErr(404, "not found");
  if (!authed()) return sendErr(401, "missing or wrong API key");
  String p = uri.substring(8), seg[3];
  int ns = 0, start = 0;
  while (ns < 3) {
    int i = p.indexOf('/', start);
    if (i < 0) { seg[ns++] = p.substring(start); break; }
    seg[ns++] = p.substring(start, i);
    start = i + 1;
  }
  const String &a = seg[0];
  if (a == "status" && m == HTTP_GET) return hStatus();
  if (a == "slots") return hSlots(seg[1], m);
  if (a == "dispense" && m == HTTP_POST) return hDispense();
  if (a == "jobs") return hJobs(seg[1], seg[2], m);
  if (a == "stop" && m == HTTP_POST) return hStop();
  if (a == "events" && m == HTTP_GET) return hEvents();
  if (a == "schedules") return hSchedules(seg[1], seg[2], m);
  if (a == "settings") return hSettings(m);
  if (a == "calibrate" && m == HTTP_POST) return hCalibrate(seg[1]);
  if (a == "diag" && m == HTTP_GET) return hDiag();
  if (a == "wifi" && m == HTTP_POST) return hWifi();
  if (a == "reboot" && m == HTTP_POST) { rebootPending = true; rebootAt = millis() + 800; return sendOk("restarting"); }
  if (m == HTTP_POST && (a == "home" || a == "park" || a == "present" || a == "goto" || a == "jog" || a == "servo")) return hMove(a);
  sendErr(404, "not found");
}

static void apiBegin() {
  static const char *hdrs[] = {"Authorization", "X-API-Key"};
  server.collectHeaders(hdrs, 2);
  server.onNotFound(dispatch);
  server.begin();
}
