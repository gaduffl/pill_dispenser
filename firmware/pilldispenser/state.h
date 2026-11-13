// Shared state: settings, slots, schedules, jobs, events + persistence in NVS
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "config.h"

struct Settings {
  char hostname[32] = DEF_HOSTNAME;
  char api_key[48] = "";                 // empty = API is open on the LAN
  char tz[48] = DEF_TZ;
  char webhook_url[128] = "";
  char wifi_ssid[33] = "";
  char wifi_pass[65] = "";
  int8_t car_dir = 1;                    // flip if the carousel turns the wrong way
  int32_t home_offset = DEF_HOME_OFFSET;
  int32_t backlash = DEF_BACKLASH_STEPS;
  uint32_t step_us_cruise = DEF_STEP_US_CRUISE;
  uint32_t step_us_start = DEF_STEP_US_START;
  uint32_t accel_steps = DEF_ACCEL_STEPS;
  int32_t servo_park_us = DEF_SERVO_PARK_US;
  int32_t servo_end_us = DEF_SERVO_END_US;
  int32_t servo_over_us = DEF_SERVO_OVER_US;
  int32_t servo_dps = DEF_SERVO_DPS;
  int32_t rock_deg = DEF_ROCK_DEG;
  bool beam_required = true;             // false = count strokes blindly if the drop sensor is unusable
  int32_t retries = DEF_RETRIES;
  int32_t max_per_item = DEF_MAX_PER_ITEM;
  int32_t refill_slots = DEF_REFILL_SLOTS;
  int32_t due_timeout_min = DEF_DUE_TIMEOUT_MIN;
  int32_t rehome_jobs = DEF_REHOME_JOBS;
  bool buzzer = true;
};

struct Slot {
  char name[32] = "";
  char roller[16] = "";                  // free text, e.g. "XL_soft"
  int32_t count = -1;                    // pills left, -1 = not tracked
  int32_t low = 10;                      // low-stock threshold
  bool enabled = true;
};

struct Item { uint8_t slot; uint8_t count; uint8_t dispensed; uint8_t missed; };

struct Schedule {
  bool used = false;
  uint16_t id = 0;
  uint8_t hh = 8, mm = 0;
  uint8_t days = 0x7F;                   // bit0 = Monday ... bit6 = Sunday
  bool enabled = true;
  bool confirm = false;                  // false: dispense at the time. true: announce, dispense on button / API
  uint8_t n = 0;
  Item items[MAX_ITEMS];
  uint32_t last_ymd = 0;                 // yyyymmdd of the last run (prevents double runs)
  char label[24] = "";
};

enum JobState : uint8_t { JOB_FREE, JOB_QUEUED, JOB_RUNNING, JOB_DONE, JOB_PARTIAL, JOB_FAILED, JOB_CANCELLED };
static const char *JOB_STATE_NAME[] = {"free", "queued", "running", "done", "partial", "failed", "cancelled"};

struct Job {
  JobState state = JOB_FREE;
  uint32_t id = 0;
  char request_id[40] = "";
  char source[12] = "";                  // api | schedule | button
  uint8_t n = 0;
  Item items[MAX_ITEMS];
  char error[64] = "";
  uint32_t t_created = 0, t_done = 0;    // unix time (0 if the clock is not set)
};

struct Event {
  uint32_t seq = 0;
  uint32_t t = 0;
  char type[20] = "";
  char msg[72] = "";
  int16_t slot = -1;
  uint32_t job = 0;
};

// ---------------------------------------------------------------- globals
static Settings cfg;
static Slot slots[N_SLOTS];
static Schedule schedules[MAX_SCHEDULES];
static Job jobs[MAX_JOBS];
static Event events[MAX_EVENTS];
static uint32_t eventSeq = 0;            // last sequence number handed out
static uint32_t jobSeq = 0;
static uint16_t scheduleSeq = 0;
static uint16_t dueList[MAX_DUE];        // schedules waiting for confirmation
static uint32_t dueSince[MAX_DUE];
static SemaphoreHandle_t stateMtx;
static Preferences prefs;

struct Lock {
  Lock() { xSemaphoreTakeRecursive(stateMtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(stateMtx); }
};

static inline uint32_t nowUnix() {
  time_t t = time(nullptr);
  return t > 1700000000 ? (uint32_t)t : 0;
}

static void copyStr(char *dst, size_t n, const char *src) {
  if (!src) src = "";
  strncpy(dst, src, n - 1);
  dst[n - 1] = 0;
}

// ---------------------------------------------------------------- events
static void addEvent(const char *type, const char *msg, int slot = -1, uint32_t job = 0) {
  Lock l;
  Event &e = events[eventSeq % MAX_EVENTS];
  e.seq = ++eventSeq;
  e.t = nowUnix();
  copyStr(e.type, sizeof(e.type), type);
  copyStr(e.msg, sizeof(e.msg), msg);
  e.slot = slot;
  e.job = job;
  Serial.printf("[event %lu] %s: %s (slot %d, job %lu)\n", (unsigned long)e.seq, type, msg, slot, (unsigned long)job);
}

static void eventToJson(const Event &e, JsonObject o) {
  o["seq"] = e.seq;
  o["time"] = e.t;
  o["type"] = e.type;
  o["message"] = e.msg;
  if (e.slot >= 0) o["slot"] = e.slot;
  if (e.job) o["job_id"] = e.job;
}

// ---------------------------------------------------------------- JSON <-> structs
static void settingsToJson(JsonObject o, bool withSecrets) {
  o["hostname"] = cfg.hostname;
  o["api_key_set"] = cfg.api_key[0] != 0;
  if (withSecrets) { o["api_key"] = cfg.api_key; o["wifi_pass"] = cfg.wifi_pass; }
  o["wifi_ssid"] = cfg.wifi_ssid;
  o["tz"] = cfg.tz;
  o["webhook_url"] = cfg.webhook_url;
  o["car_dir"] = cfg.car_dir;
  o["home_offset"] = cfg.home_offset;
  o["backlash"] = cfg.backlash;
  o["step_us_cruise"] = cfg.step_us_cruise;
  o["step_us_start"] = cfg.step_us_start;
  o["accel_steps"] = cfg.accel_steps;
  o["servo_park_us"] = cfg.servo_park_us;
  o["servo_end_us"] = cfg.servo_end_us;
  o["servo_over_us"] = cfg.servo_over_us;
  o["servo_dps"] = cfg.servo_dps;
  o["rock_deg"] = cfg.rock_deg;
  o["beam_required"] = cfg.beam_required;
  o["retries"] = cfg.retries;
  o["max_per_item"] = cfg.max_per_item;
  o["refill_slots"] = cfg.refill_slots;
  o["due_timeout_min"] = cfg.due_timeout_min;
  o["rehome_jobs"] = cfg.rehome_jobs;
  o["buzzer"] = cfg.buzzer;
}

template <typename T> static void rd(JsonVariantConst v, T &dst, long lo, long hi) {
  if (v.is<long>()) { long x = v.as<long>(); if (x < lo) x = lo; if (x > hi) x = hi; dst = (T)x; }
}
static void rdStr(JsonVariantConst v, char *dst, size_t n) {
  if (v.is<const char *>()) copyStr(dst, n, v.as<const char *>());
}
static void rdBool(JsonVariantConst v, bool &dst) {
  if (v.is<bool>()) dst = v.as<bool>();
}

static void settingsFromJson(JsonObjectConst o) {
  rdStr(o["hostname"], cfg.hostname, sizeof(cfg.hostname));
  rdStr(o["api_key"], cfg.api_key, sizeof(cfg.api_key));
  rdStr(o["tz"], cfg.tz, sizeof(cfg.tz));
  rdStr(o["webhook_url"], cfg.webhook_url, sizeof(cfg.webhook_url));
  rdStr(o["wifi_ssid"], cfg.wifi_ssid, sizeof(cfg.wifi_ssid));
  rdStr(o["wifi_pass"], cfg.wifi_pass, sizeof(cfg.wifi_pass));
  if (o["car_dir"].is<long>()) cfg.car_dir = o["car_dir"].as<long>() < 0 ? -1 : 1;
  rd(o["home_offset"], cfg.home_offset, -20000, 20000);
  rd(o["backlash"], cfg.backlash, 0, 1000);
  rd(o["step_us_cruise"], cfg.step_us_cruise, 900, 10000);
  rd(o["step_us_start"], cfg.step_us_start, 900, 10000);
  rd(o["accel_steps"], cfg.accel_steps, 1, 2000);
  rd(o["servo_park_us"], cfg.servo_park_us, 450, 2550);
  rd(o["servo_end_us"], cfg.servo_end_us, 450, 2550);
  rd(o["servo_over_us"], cfg.servo_over_us, 0, 200);
  rd(o["servo_dps"], cfg.servo_dps, 30, 600);
  rd(o["rock_deg"], cfg.rock_deg, 0, 35);
  rdBool(o["beam_required"], cfg.beam_required);
  rd(o["retries"], cfg.retries, 1, 10);
  rd(o["max_per_item"], cfg.max_per_item, 1, 50);
  rd(o["refill_slots"], cfg.refill_slots, 1, N_SLOTS - 1);
  rd(o["due_timeout_min"], cfg.due_timeout_min, 1, 1440);
  rd(o["rehome_jobs"], cfg.rehome_jobs, 0, 1000);
  rdBool(o["buzzer"], cfg.buzzer);
}

static void slotToJson(int i, JsonObject o) {
  const Slot &s = slots[i];
  o["slot"] = i + 1;
  o["name"] = s.name;
  o["roller"] = s.roller;
  o["count"] = s.count;
  o["low"] = s.low;
  o["enabled"] = s.enabled;
  o["low_stock"] = s.count >= 0 && s.count <= s.low;
}

static void slotFromJson(int i, JsonObjectConst o) {
  Slot &s = slots[i];
  rdStr(o["name"], s.name, sizeof(s.name));
  rdStr(o["roller"], s.roller, sizeof(s.roller));
  rd(o["count"], s.count, -1, 100000);
  rd(o["low"], s.low, 0, 100000);
  rdBool(o["enabled"], s.enabled);
}

static void itemsToJson(const Item *it, uint8_t n, JsonArray a, bool progress) {
  for (uint8_t i = 0; i < n; i++) {
    JsonObject o = a.add<JsonObject>();
    o["slot"] = it[i].slot + 1;
    o["count"] = it[i].count;
    if (progress) { o["dispensed"] = it[i].dispensed; o["missed"] = it[i].missed; }
  }
}

// returns "" or an error text
static String itemsFromJson(JsonArrayConst a, Item *it, uint8_t &n) {
  n = 0;
  if (a.isNull() || a.size() == 0) return "items missing";
  if (a.size() > MAX_ITEMS) return "too many items";
  for (JsonObjectConst o : a) {
    long slot = o["slot"] | 0L, count = o["count"] | 1L;
    if (slot < 1 || slot > N_SLOTS) return "slot out of range";
    if (count < 1 || count > cfg.max_per_item) return "count out of range";
    if (!slots[slot - 1].enabled) return "slot disabled";
    it[n].slot = slot - 1; it[n].count = count; it[n].dispensed = 0; it[n].missed = 0;
    n++;
  }
  return "";
}

static void scheduleToJson(const Schedule &s, JsonObject o) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", s.hh, s.mm);
  o["id"] = s.id;
  o["time"] = buf;
  JsonArray d = o["days"].to<JsonArray>();
  for (int i = 0; i < 7; i++) if (s.days & (1 << i)) d.add(i + 1);        // 1 = Monday ... 7 = Sunday
  o["enabled"] = s.enabled;
  o["mode"] = s.confirm ? "confirm" : "auto";
  o["label"] = s.label;
  itemsToJson(s.items, s.n, o["items"].to<JsonArray>(), false);
  o["last_run"] = s.last_ymd;
}

static String scheduleFromJson(JsonObjectConst o, Schedule &s, bool partial) {
  if (o["time"].is<const char *>()) {
    int h = -1, m = -1;
    if (sscanf(o["time"].as<const char *>(), "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return "time must be HH:MM";
    s.hh = h; s.mm = m;
  } else if (!partial) return "time missing";
  if (o["days"].is<JsonArrayConst>()) {
    uint8_t d = 0;
    for (JsonVariantConst v : o["days"].as<JsonArrayConst>()) { long k = v | 0L; if (k >= 1 && k <= 7) d |= 1 << (k - 1); }
    s.days = d ? d : 0x7F;
  }
  rdBool(o["enabled"], s.enabled);
  if (o["mode"].is<const char *>()) s.confirm = strcmp(o["mode"].as<const char *>(), "confirm") == 0;
  rdStr(o["label"], s.label, sizeof(s.label));
  if (o["items"].is<JsonArrayConst>()) {
    String e = itemsFromJson(o["items"].as<JsonArrayConst>(), s.items, s.n);
    if (e.length()) return e;
  } else if (!partial) return "items missing";
  return "";
}

static void jobToJson(const Job &j, JsonObject o) {
  o["id"] = j.id;
  o["state"] = JOB_STATE_NAME[j.state];
  o["source"] = j.source;
  if (j.request_id[0]) o["request_id"] = j.request_id;
  itemsToJson(j.items, j.n, o["items"].to<JsonArray>(), true);
  if (j.error[0]) o["error"] = j.error;
  o["created"] = j.t_created;
  o["finished"] = j.t_done;
}

// ---------------------------------------------------------------- persistence
static void saveBlob(const char *key, JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  prefs.putBytes(key, out.c_str(), out.length());
}

static bool loadBlob(const char *key, JsonDocument &doc) {
  size_t n = prefs.getBytesLength(key);
  if (n == 0 || n > 16000) return false;
  char *buf = (char *)malloc(n + 1);
  if (!buf) return false;
  prefs.getBytes(key, buf, n);
  buf[n] = 0;
  DeserializationError err = deserializeJson(doc, buf);
  free(buf);
  return !err;
}

static void saveConfig() {
  Lock l;
  JsonDocument doc;
  settingsToJson(doc["settings"].to<JsonObject>(), true);
  JsonArray a = doc["slots"].to<JsonArray>();
  for (int i = 0; i < N_SLOTS; i++) slotToJson(i, a.add<JsonObject>());
  saveBlob("cfg", doc);
}

static void saveSchedules() {
  Lock l;
  JsonDocument doc;
  JsonArray a = doc["schedules"].to<JsonArray>();
  for (auto &s : schedules) if (s.used) scheduleToJson(s, a.add<JsonObject>());
  doc["seq"] = scheduleSeq;
  saveBlob("sched", doc);
}

static void loadAll() {
  prefs.begin("pilldisp", false);
  JsonDocument doc;
  if (loadBlob("cfg", doc)) {
    settingsFromJson(doc["settings"].as<JsonObjectConst>());
    int i = 0;
    for (JsonObjectConst o : doc["slots"].as<JsonArrayConst>()) if (i < N_SLOTS) slotFromJson(i++, o);
  }
  JsonDocument d2;
  if (loadBlob("sched", d2)) {
    scheduleSeq = d2["seq"] | 0;
    int i = 0;
    for (JsonObjectConst o : d2["schedules"].as<JsonArrayConst>()) {
      if (i >= MAX_SCHEDULES) break;
      Schedule &s = schedules[i];
      // stored items may reference slots that were disabled later: load them anyway
      s = Schedule();
      s.used = true;
      s.id = o["id"] | 0;
      s.last_ymd = o["last_run"] | 0UL;
      bool en[N_SLOTS];
      for (int k = 0; k < N_SLOTS; k++) { en[k] = slots[k].enabled; slots[k].enabled = true; }
      String e = scheduleFromJson(o, s, false);
      for (int k = 0; k < N_SLOTS; k++) slots[k].enabled = en[k];
      if (e.length() == 0) i++; else s.used = false;
    }
  }
}
