// Modular carousel pill dispenser - firmware
// Board: Seeed Studio XIAO ESP32-C6 (or XIAO ESP32-S3), Arduino-ESP32 core 3.x
// Library: ArduinoJson 7.x
//
// First start: the device opens the Wi-Fi network "PillDispenser-xxxx". Join it, open
// http://192.168.4.1/setup and enter your Wi-Fi. Afterwards: http://pilldispenser.local/
// (Or put credentials into secrets.h, see below.)

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <time.h>
#include "config.h"
#if ENABLE_OTA
#include <ArduinoOTA.h>
#endif
#if __has_include("secrets.h")
#include "secrets.h"          // optional: #define WIFI_SSID "..." / #define WIFI_PASS "..." / #define API_KEY "..."
#endif
#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "This firmware needs the Arduino-ESP32 core 3.x (LEDC API, XIAO ESP32-C6 support)"
#endif

#include "state.h"
#include "motion.h"
#include "doses.h"
#include "api.h"

static bool apMode = false;
static bool netServices = false;

// ------------------------------------------------------------------ Wi-Fi
static void startAP() {
  char name[32];
  snprintf(name, sizeof(name), "PillDispenser-%04X", (unsigned)(ESP.getEfuseMac() >> 32) & 0xFFFF);
  WiFi.mode(cfg.wifi_ssid[0] ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(name);
  apMode = true;
  Serial.printf("Setup access point \"%s\" - open http://192.168.4.1/setup\n", name);
}

static void startNetServices() {
  if (netServices) return;
  netServices = true;
  if (MDNS.begin(cfg.hostname)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("pilldispenser", "tcp", 80);
    MDNS.addServiceTxt("pilldispenser", "tcp", "api", "/api/v1");
    MDNS.addServiceTxt("pilldispenser", "tcp", "slots", String(N_SLOTS));
  }
  configTzTime(cfg.tz, "pool.ntp.org", "time.cloudflare.com");
#if ENABLE_OTA
  ArduinoOTA.setHostname(cfg.hostname);
  if (cfg.api_key[0]) ArduinoOTA.setPassword(cfg.api_key);
  ArduinoOTA.begin();
#endif
  Serial.printf("Online: http://%s.local/  (%s)\n", cfg.hostname, WiFi.localIP().toString().c_str());
}

static void wifiBegin() {
  WiFi.persistent(false);
  WiFi.setHostname(cfg.hostname);
  WiFi.setAutoReconnect(true);
  if (cfg.wifi_ssid[0]) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
    for (int i = 0; i < 80 && WiFi.status() != WL_CONNECTED; i++) delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) startNetServices();
  else startAP();
}

static void tickWifi() {
  static uint32_t last = 0;
  if (millis() - last < 2000) return;
  last = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (apMode) { WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); apMode = false; }
    startNetServices();
  }
}

// ------------------------------------------------------------------ schedules
static void tickSchedules() {
  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  uint32_t now = nowUnix();
  if (!now) return;                              // clock not set yet: never dispense on a guessed time
  time_t t = now;
  struct tm tmv;
  localtime_r(&t, &tmv);
  uint32_t ymd = (tmv.tm_year + 1900) * 10000UL + (tmv.tm_mon + 1) * 100UL + tmv.tm_mday;
  uint8_t dayBit = 1 << ((tmv.tm_wday + 6) % 7);  // Monday = bit 0
  Lock l;
  bool dirty = false;
  for (auto &s : schedules) {
    if (!s.used || !s.enabled || !(s.days & dayBit)) continue;
    if (s.hh != tmv.tm_hour || s.mm != tmv.tm_min || s.last_ymd == ymd) continue;
    s.last_ymd = ymd;
    dirty = true;
    fireSchedule(s, now);
  }
  expireDue(now);
  if (dirty) saveSchedules();
}

// ------------------------------------------------------------------ buttons: dispense what is due, one per profile
static void tickButtons() {
  static const int pins[N_PROFILES] = {PIN_BUTTON, PIN_BUTTON2};
  static bool was[N_PROFILES] = {};
  static uint32_t downAt[N_PROFILES] = {};
  for (int k = 0; k < N_PROFILES; k++) {
    if (pins[k] < 0) continue;
    bool down = digitalRead(pins[k]) == LOW;
    if (down && !was[k]) downAt[k] = millis();
    if (!down && was[k] && millis() - downAt[k] > 40) pressButton(PIN_BUTTON2 < 0 ? 0 : k + 1);   // one button: both
    was[k] = down;
  }
}

// ------------------------------------------------------------------ webhook: push events to the app backend
static bool webhookWanted(const char *type) {
  static const char *types[] = {"job_done", "job_partial", "job_failed", "low_stock", "slot_empty", "dose_due",
                                "dose_missed", "double", "chute_blocked", "error"};
  for (auto t : types) if (!strcmp(t, type)) return true;
  return false;
}

static void tickWebhook() {
  static uint32_t pushed = 0, retryAt = 0;
  static uint8_t fails = 0;
  if (!cfg.webhook_url[0] || WiFi.status() != WL_CONNECTED) { pushed = eventSeq; return; }
  if (pushed >= eventSeq || millis() < retryAt) return;
  if (eventSeq - pushed > MAX_EVENTS) pushed = eventSeq - MAX_EVENTS;
  String body;
  {
    Lock l;
    const Event &e = events[pushed % MAX_EVENTS];   // event with seq == pushed + 1
    if (e.seq != pushed + 1 || !webhookWanted(e.type)) { pushed++; return; }
    JsonDocument d;
    d["device"] = cfg.hostname;
    eventToJson(e, d["event"].to<JsonObject>());
    if (e.job) { Job *j = findJob(e.job); if (j) jobToJson(*j, d["job"].to<JsonObject>()); }
    serializeJson(d, body);
  }
  HTTPClient http;
  http.setTimeout(4000);
  int code = -1;
  if (http.begin(cfg.webhook_url)) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(body);
    http.end();
  }
  if (code >= 200 && code < 300) { pushed++; fails = 0; }
  else if (++fails >= 5) { pushed++; fails = 0; }        // give up on this one after 5 tries
  else retryAt = millis() + 15000;
}

// ------------------------------------------------------------------ Arduino entry points
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nPill dispenser firmware " FW_VERSION);
  stateMtx = xSemaphoreCreateRecursiveMutex();
  loadAll();
#ifdef WIFI_SSID
  if (!cfg.wifi_ssid[0]) { copyStr(cfg.wifi_ssid, sizeof(cfg.wifi_ssid), WIFI_SSID); copyStr(cfg.wifi_pass, sizeof(cfg.wifi_pass), WIFI_PASS); }
#endif
#ifdef API_KEY
  if (!cfg.api_key[0]) copyStr(cfg.api_key, sizeof(cfg.api_key), API_KEY);
#endif
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  if (PIN_BUTTON2 >= 0) pinMode(PIN_BUTTON2, INPUT_PULLUP);
  motion::begin();                               // servo to park, carousel homes itself
  wifiBegin();
  apiBegin();
  addEvent("boot", "firmware " FW_VERSION);
}

void loop() {
  server.handleClient();
#if ENABLE_OTA
  if (netServices) ArduinoOTA.handle();
#endif
  tickWifi();
  tickSchedules();
  tickButtons();
  tickWebhook();
  if (rebootPending && millis() > rebootAt) ESP.restart();
  delay(2);
}
