// Host simulation of the carousel logic: homing, slot moves, lost-step recovery.
#include "Arduino.h"
#include "esp_timer.h"
#include <vector>
SerialT Serial; EspT ESP;
#include "config.h"
#define N_SLOTS_SIM N_SLOTS
static uint32_t fakeMs = 0;
static uint32_t fakeMsNow();
uint32_t millis() { return fakeMs; } uint32_t micros() { return fakeMs * 1000; }
void delay(uint32_t ms) { fakeMs += ms; } void delayMicroseconds(uint32_t) {}
struct Tm; static void tickPeriodic(uint32_t ms);
void vTaskDelay(TickType_t t) { tickPeriodic(t); }
void configTzTime(const char *, const char *, const char *) {}
bool ledcAttach(uint8_t, uint32_t, uint8_t) { return true; }
int analogRead(int) { return 0; } void analogReadResolution(int) {}
void pinMode(int, int) {}
// ---- simulated mechanics
static const int NS = N_SLOTS_SIM;
static const double REV = 4075.7728 * 4.0, SLOT = REV / NS;
static double phys = 1234.0;          // physical carousel angle in half-steps
static const double P0 = 777.0;       // physical angle at which slot 1 stands at the station
static const int FLAG0 = -317, FLAGW = 634;   // the home flag covers 14 degrees, centred on "slot 1 at the station"
static bool homeInv = false, homeDead = false;
static int pins[16] = {0}; static int lastPhase = -1;
static const int SEQ[8] = {0x1, 0x3, 0x2, 0x6, 0x4, 0xC, 0x8, 0x9};
// ---- simulated drop light barrier + pills
static bool rxUnplugged = false, txUnplugged = false, dropInv = false;
static int pillsPerStroke = 0, strokes = 0, servoEndUs = 2280, servoParkUs = 610;
static uint32_t prevUs = 0, blockFrom[4], blockTo[4]; static int nBlock = 0;
static uint32_t fakeMsNow();
bool ledcWrite(uint8_t, uint32_t duty) {
  uint32_t us = duty * 20000UL / 16384UL;
  static bool fromPark = true;
  if (us <= (uint32_t)servoParkUs + 30) fromPark = true;
  if (fromPark && us >= (uint32_t)servoEndUs - 8) {      // roller arrives over the outlet, coming from the hopper
    fromPark = false;
    strokes++;
    nBlock = 0;
    for (int i = 0; i < pillsPerStroke && i < 4; i++) { blockFrom[nBlock] = fakeMsNow() + 70 + 45 * i; blockTo[nBlock] = blockFrom[nBlock] + 12; nBlock++; }
  }
  prevUs = us;
  return true;
}
void digitalWrite(int pin, int v) {
  pins[pin] = v;
  if (pin != D10) return;            // coils() writes IN1..IN4 in order: evaluate once the pattern is complete
  int m = pins[D7] | (pins[D8] << 1) | (pins[D9] << 2) | (pins[D10] << 3);
  for (int i = 0; i < 8; i++) if (SEQ[i] == m) {
    if (lastPhase >= 0 && i != lastPhase) { int d = (i - lastPhase + 8) % 8; if (d == 1) phys += 1; else if (d == 7) phys -= 1; }
    lastPhase = i;
  }
}
int digitalRead(int pin) {
  if (pin == D1) {                   // home light barrier
    if (homeDead) return HIGH;
    double rel = fmod(phys - P0 - FLAG0, REV); if (rel < 0) rel += REV;
    bool blocked = rel < FLAGW;
    return (blocked != homeInv) ? LOW : HIGH;
  }
  if (pin == D0) {                   // drop light barrier: receiver follows the emitter unless a pill is in the way
    if (rxUnplugged) return HIGH;    // open input, pulled up
    bool pill = false; uint32_t t = fakeMsNow();
    for (int i = 0; i < nBlock; i++) if (t >= blockFrom[i] && t < blockTo[i]) pill = true;
    bool light = pins[D4] == HIGH && !txUnplugged && !pill;
    return (light != dropInv) ? HIGH : LOW;
  }
  return HIGH;
}
// ---- timers / semaphores
struct Tm { void (*cb)(void *); bool pending; };
static std::vector<Tm *> timers; static bool semFlag[8]; static int nSem = 0;
int esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *h) { Tm *t = new Tm{a->callback, false}; timers.push_back(t); *h = t; return 0; }
int esp_timer_start_once(esp_timer_handle_t h, uint64_t) { ((Tm *)h)->pending = true; return 0; }
static Tm *periodic = nullptr;
int esp_timer_start_periodic(esp_timer_handle_t h, uint64_t) { periodic = (Tm *)h; return 0; } int esp_timer_stop(esp_timer_handle_t h) { ((Tm *)h)->pending = false; if (periodic == h) periodic = nullptr; return 0; }
SemaphoreHandle_t xSemaphoreCreateBinary() { semFlag[nSem] = false; return &semFlag[nSem++]; }
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return xSemaphoreCreateBinary(); }
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t, TickType_t) { return 1; } BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t) { return 1; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { *(bool *)s = true; return 1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait) {
  bool *f = (bool *)s;
  for (long guard = 0; !*f && wait && guard < 5000000; guard++) {
    bool any = false;
    for (auto t : timers) if (t->pending) { t->pending = false; any = true; t->cb(nullptr); }
    if (!any) break;
  }
  bool r = *f; *f = false; return r;
}
QueueHandle_t xQueueCreate(int, int) { return nullptr; } BaseType_t xQueueReceive(QueueHandle_t, void *, TickType_t) { return 0; }
BaseType_t xQueueSend(QueueHandle_t, const void *, TickType_t) { return 1; }
BaseType_t xTaskCreate(void (*)(void *), const char *, int, void *, int, void *) { return 1; }
static void tickPeriodic(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) {
    fakeMs++;
    if (periodic) for (int k = 0; k < 2; k++) periodic->cb(nullptr);
  }
}
static uint32_t fakeMsNow() { return fakeMs; }
#include "state.h"
#include "motion.h"
static double slotAtStation() { double x = fmod((phys - P0) / SLOT, (double)NS); if (x < 0) x += NS; return x; }
static double slotErr(int want) { double e = slotAtStation() - want; if (e > NS / 2) e -= NS; if (e < -NS / 2) e += NS; return e * SLOT; }
static int bad = 0;
static void expect(bool ok, const char *what) { if (!ok) { bad++; printf("   !! FAILED: %s\n", what); } }
static Job *run(Item *it, int n, const char *req) { motion::state = motion::M_IDLE; motion::enqueueJob(it, n, "api", req); Job *j = motion::nextQueued(); motion::runJob(*j); return j; }
int main() {
  stateMtx = xSemaphoreCreateRecursiveMutex();
  motion::begin();
  servoEndUs = cfg.servo_end_us; servoParkUs = cfg.servo_park_us;
  // ---- homing with either output polarity of the light barrier, from three starting positions
  for (int inv = 0; inv < 2; inv++) for (double start : {1234.0, P0 + 3.0, P0 + 9000.0}) {
    homeInv = inv; phys = start; lastPhase = -1; motion::homed = false;
    bool ok = motion::home();
    motion::gotoSlot(0);
    printf("home, barrier reads %s when blocked, start %6.0f: %s, slot 1 off by %.1f steps\n", inv ? "HIGH" : "LOW ", start, ok ? "ok" : "FAILED", slotErr(0));
    expect(ok && fabs(slotErr(0)) < 8, "homing");
  }
  homeInv = false; expect(motion::home(), "home"); motion::gotoSlot(0);
  int seq[] = {0, 1, 9, 3, 4, 4, 0, 5, 2, 7, 1};
  for (int sl : seq) {
    double before = phys;
    motion::gotoSlot(sl);
    printf("  goto slot %2d: moved %+7.0f steps, error %.2f steps\n", sl + 1, phys - before, slotErr(sl));
    expect(fabs(slotErr(sl)) < 8, "slot move");
  }
  phys -= 600; printf("  -- carousel blocked by hand: 600 steps lost --\n");
  for (int sl : {3, 8, 1, 0}) { motion::gotoSlot(sl); printf("  goto slot %2d: error %.1f steps\n", sl + 1, slotErr(sl)); }
  expect(fabs(slotErr(0)) < 8, "recovery after lost steps");
  homeDead = true; motion::homed = false;
  bool hk = motion::home(); printf("home barrier unplugged: home() %s, error '%s'\n", hk ? "ok" : "refused", motion::lastError); expect(!hk, "dead home sensor must fail");
  homeDead = false; phys = 4000; expect(motion::home(), "home again");
  // ---- jobs
  Item it[2] = {{2, 2, 0, 0}, {5, 1, 0, 0}};
  slots[2].count = 30; slots[5].count = 4; slots[5].low = 3;
  pillsPerStroke = 1; strokes = 0;
  Job *j = run(it, 2, "req-1");
  printf("1) pills come: state=%s dispensed=%d/%d,%d/%d strokes=%d stock %d,%d\n", JOB_STATE_NAME[j->state], j->items[0].dispensed, j->items[0].count, j->items[1].dispensed, j->items[1].count, strokes, (int)slots[2].count, (int)slots[5].count);
  expect(j->state == JOB_DONE && strokes == 3 && slots[2].count == 28 && slots[5].count == 3, "normal job");
  bool ex = false; uint32_t id2 = motion::enqueueJob(it, 2, "api", "req-1", &ex);
  printf("2) same request id again: job %u, existing=%d\n", (unsigned)id2, ex); expect(ex && id2 == j->id, "idempotent request");
  pillsPerStroke = 0; strokes = 0; j = run(it, 2, "req-2");
  printf("3) module empty: state=%s dispensed=%d,%d strokes=%d error='%s'\n", JOB_STATE_NAME[j->state], j->items[0].dispensed, j->items[1].dispensed, strokes, j->error);
  expect(j->state == JOB_PARTIAL && strokes == 2 * cfg.retries, "empty module");
  pillsPerStroke = 1; rxUnplugged = true; strokes = 0; j = run(it, 2, "req-3");
  printf("4) receiver unplugged: state=%s strokes=%d error='%s'\n", JOB_STATE_NAME[j->state], strokes, j->error); expect(j->state == JOB_FAILED && strokes == 0, "dead receiver must stop before any stroke");
  rxUnplugged = false; txUnplugged = true; strokes = 0; j = run(it, 2, "req-4");
  printf("5) emitter unplugged / chute blocked: state=%s strokes=%d\n", JOB_STATE_NAME[j->state], strokes); expect(j->state == JOB_FAILED && strokes == 0, "dark sensor must stop before any stroke");
  txUnplugged = false; dropInv = true; strokes = 0; slots[2].count = 30; j = run(it, 2, "req-5");
  printf("6) light barrier with the opposite output polarity: state=%s dispensed=%d,%d strokes=%d\n", JOB_STATE_NAME[j->state], j->items[0].dispensed, j->items[1].dispensed, strokes);
  expect(j->state == JOB_DONE && strokes == 3, "inverted polarity");
  dropInv = false; pillsPerStroke = 2; strokes = 0; Item one[1] = {{2, 2, 0, 0}}; j = run(one, 1, "req-6");
  printf("7) two pills in one stroke: state=%s dispensed=%d of %d asked, strokes=%d\n", JOB_STATE_NAME[j->state], j->items[0].dispensed, j->items[0].count, strokes);
  expect(j->items[0].dispensed == 2 && strokes == 1, "double is counted, no further stroke");
  pillsPerStroke = 1; rxUnplugged = true; cfg.beam_required = false; strokes = 0; j = run(it, 2, "req-7");
  printf("8) blind mode switched on by the user: state=%s dispensed=%d,%d strokes=%d\n", JOB_STATE_NAME[j->state], j->items[0].dispensed, j->items[1].dispensed, strokes);
  expect(j->state == JOB_DONE && strokes == 3, "blind mode");
  printf(bad ? "RESULT: %d problems\n" : "RESULT: ok\n", bad);
  return bad;
}
