// Host simulation of the carousel logic with ten index flags (slot 1's twice as wide): homing, checked slot moves,
// gear backlash, lost steps, a carousel turned by hand, a slipping pinion, broken or see-through flags, and the jobs.
// Every roller stroke is checked against the module that physically stands at the station: a stroke at a different
// module than the firmware believes is a "wrong-module stroke" and must never happen.
#include "Arduino.h"
#include "esp_timer.h"
#include <vector>
#include <random>
#include <algorithm>
SerialT Serial; EspT ESP;
#include "config.h"
static uint32_t fakeMs = 0;
uint32_t millis() { return fakeMs; } uint32_t micros() { return fakeMs * 1000; }
void delay(uint32_t ms) { fakeMs += ms; } void delayMicroseconds(uint32_t) {}
static void tickPeriodic(uint32_t ms);
void vTaskDelay(TickType_t t) { tickPeriodic(t); }
void configTzTime(const char *, const char *, const char *) {}
bool ledcAttach(uint8_t, uint32_t, uint8_t) { return true; }
int analogRead(int) { return 0; } void analogReadResolution(int) {}
void pinMode(int, int) {}

// ---------------------------------------------------------------- simulated mechanics
static const int NS = N_SLOTS;
static const double REV = 4075.7728 * 4.0, SLOT = REV / NS, DEG = REV / 360.0;
static const double P0 = 777.0;           // physical carousel angle (half-steps) at which slot 1 stands at the station
static double phys = 1234.0;              // physical carousel angle; + = forward = the next slot comes to the station
static int gap = 0;                       // backlash: 0 = teeth pressed forward ... BL = pressed backward
static int BL = 70;                       // real backlash (motor gearbox + carousel gear) in half-steps
static long motorSteps = 0;               // every half-step the firmware makes
static long stallFrom = -1, stallTo = -1; // the half-steps in this range do not move anything (jam)
static double slipP = 0;                  // probability that a half-step slips (pinion loose on the shaft)
static long abortAt = -1;                 // press "stop" at this half-step
static void (*onAbort)() = nullptr;
static std::mt19937 rng(7);
// index flags: slot i's flag starts LEAD before slot i stands at the station
static double LEAD = 7.0 * DEG;
static const double W = 12.0 * DEG, WHOME = 24.0 * DEG;
static int missingFlag = -1;              // this flag broke off
static bool transparent = false;          // flags printed in a filament that lets the infrared through
static bool homeInv = false;              // receiver output: false = LOW while blocked (Adafruit 2167), true = HIGH
static int homeStuck = -1;                // receiver unplugged or dead: always this level
static double edgeNoise = 0;              // the receiver flickers within this many half-steps of a flag edge
static int pins[16] = {0}, lastPhase = -1;
static const int SEQ[8] = {0x1, 0x3, 0x2, 0x6, 0x4, 0xC, 0x8, 0x9};

static void motorStep(int d) {            // the rotor follows one half-step of the coils
  long n = motorSteps++;
  if (n == abortAt && onAbort) onAbort();
  if (n >= stallFrom && n < stallTo) return;                // jammed: the step is lost
  if (d > 0 && gap > 0) { gap--; return; }                  // taking up the backlash, the carousel does not move yet
  if (d < 0 && gap < BL) { gap++; return; }
  if (slipP > 0 && std::uniform_real_distribution<double>(0, 1)(rng) < slipP) return;
  phys += d;
}
static void turnByHand(double steps) {    // the motor holds still; beyond the backlash it is turned backwards
  phys += steps;
  gap = std::min(std::max(gap + (int)lround(steps), 0), BL);
}
static bool flagInBeam() {
  bool in = false, near = false;
  for (int i = 0; i < NS; i++) {
    if (i == missingFlag) continue;
    double rel = fmod(phys - (P0 + i * SLOT - LEAD), REV);
    if (rel < 0) rel += REV;
    double w = i == 0 ? WHOME : W;
    if (rel < w) in = true;
    if (rel < edgeNoise || REV - rel < edgeNoise || fabs(rel - w) < edgeNoise) near = true;
  }
  if (near) return rng() & 1;
  return in;
}
static int physSlot() { return (int)(((lround((phys - P0) / SLOT) % NS) + NS) % NS); }
static double physOffDeg() { double x = (phys - P0) / SLOT; return (x - (double)lround(x)) * SLOT / DEG; }

// ---------------------------------------------------------------- simulated drop light barrier, pills, servo
static bool rxUnplugged = false, txUnplugged = false, dropInv = false;
static int pillsPerStroke = 0, strokes = 0, servoEndUs = 2280, servoParkUs = 610;
static int wrongStrokes = 0, offStrokes = 0, handAfterStroke = -1;
static double handSteps = 0, worstOff = 0;
static int (*beliefSlot)() = nullptr;
static uint32_t blockFrom[4], blockTo[4];
static int nBlock = 0;
bool ledcWrite(uint8_t, uint32_t duty) {
  uint32_t us = duty * 20000UL / 16384UL;
  static bool fromPark = true;
  if (us <= (uint32_t)servoParkUs + 30) fromPark = true;
  if (fromPark && us >= (uint32_t)servoEndUs - 8) {          // roller arrives over the outlet, coming from the hopper
    fromPark = false;
    strokes++;
    if (beliefSlot && physSlot() != beliefSlot()) {
      wrongStrokes++;
      printf("   !! stroke at module %d, firmware believes %d\n", physSlot() + 1, beliefSlot() + 1);
    }
    if (fabs(physOffDeg()) > 1.0) offStrokes++;
    worstOff = std::max(worstOff, fabs(physOffDeg()));
    nBlock = 0;
    for (int i = 0; i < pillsPerStroke && i < 4; i++) { blockFrom[nBlock] = fakeMs + 70 + 45 * i; blockTo[nBlock] = blockFrom[nBlock] + 12; nBlock++; }
    if (strokes == handAfterStroke) { turnByHand(handSteps); handAfterStroke = -1; }
  }
  return true;
}
void digitalWrite(int pin, int v) {
  pins[pin] = v;
  if (pin != D10) return;                // coils() writes IN1..IN4 in order: evaluate once the pattern is complete
  int m = pins[D7] | (pins[D8] << 1) | (pins[D9] << 2) | (pins[D10] << 3);
  for (int i = 0; i < 8; i++) if (SEQ[i] == m) {
    if (lastPhase >= 0 && i != lastPhase) { int d = (i - lastPhase + 8) % 8; if (d == 1) motorStep(1); else if (d == 7) motorStep(-1); }
    lastPhase = i;
  }
}
int digitalRead(int pin) {
  if (pin == D1) {                       // index light barrier
    if (homeStuck >= 0) return homeStuck;
    bool blocked = !transparent && flagInBeam();
    return (blocked != homeInv) ? LOW : HIGH;
  }
  if (pin == D0) {                       // drop light barrier: receiver follows the emitter unless a pill is in the way
    if (rxUnplugged) return HIGH;        // open input, pulled up
    bool pill = false;
    for (int i = 0; i < nBlock; i++) if (fakeMs >= blockFrom[i] && fakeMs < blockTo[i]) pill = true;
    bool light = pins[PIN_BEAM_LED] == HIGH && !txUnplugged && !pill;
    return (light != dropInv) ? HIGH : LOW;
  }
  return HIGH;
}

// ---------------------------------------------------------------- timers / semaphores / tasks
struct Tm { void (*cb)(void *); bool pending; };
static std::vector<Tm *> timers;
static bool semFlag[8];
static int nSem = 0;
static Tm *periodic = nullptr;
int esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *h) { Tm *t = new Tm{a->callback, false}; timers.push_back(t); *h = t; return 0; }
int esp_timer_start_once(esp_timer_handle_t h, uint64_t) { ((Tm *)h)->pending = true; return 0; }
int esp_timer_start_periodic(esp_timer_handle_t h, uint64_t) { periodic = (Tm *)h; return 0; }
int esp_timer_stop(esp_timer_handle_t h) { ((Tm *)h)->pending = false; if (periodic == h) periodic = nullptr; return 0; }
SemaphoreHandle_t xSemaphoreCreateBinary() { semFlag[nSem] = false; return &semFlag[nSem++]; }
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return xSemaphoreCreateBinary(); }
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t, TickType_t) { return 1; }
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t) { return 1; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { *(bool *)s = true; return 1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait) {
  bool *f = (bool *)s;
  for (long guard = 0; !*f && wait && guard < 5000000; guard++) {
    bool any = false;
    for (auto t : timers) if (t->pending) { t->pending = false; any = true; t->cb(nullptr); }
    if (!any) break;
  }
  bool r = *f;
  *f = false;
  return r;
}
QueueHandle_t xQueueCreate(int, int) { return nullptr; }
BaseType_t xQueueReceive(QueueHandle_t, void *, TickType_t) { return 0; }
BaseType_t xQueueSend(QueueHandle_t, const void *, TickType_t) { return 1; }
BaseType_t xTaskCreate(void (*)(void *), const char *, int, void *, int, void *) { return 1; }
static void tickPeriodic(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) {
    fakeMs++;
    if (periodic) for (int k = 0; k < 2; k++) periodic->cb(nullptr);
  }
}

#include "state.h"
#include "motion.h"
#include "doses.h"

// ---------------------------------------------------------------- test helpers
static int bad = 0;
static void expect(bool ok, const char *what) { if (!ok) { bad++; printf("   !! FAILED: %s\n", what); } }
static uint32_t mark() { return eventSeq; }
static int eventsSince(uint32_t seq, const char *type, const char *text = nullptr) {
  int n = 0;
  for (uint32_t s = seq + 1; s <= eventSeq; s++) {
    const Event &e = events[(s - 1) % MAX_EVENTS];
    if (e.seq == s && !strcmp(e.type, type) && (!text || strstr(e.msg, text))) n++;
  }
  return n;
}
static int eventsFor(uint32_t seq, const char *type, uint8_t profile, const char *text = nullptr) {
  int n = 0;
  for (uint32_t s = seq + 1; s <= eventSeq; s++) {
    const Event &e = events[(s - 1) % MAX_EVENTS];
    if (e.seq == s && !strcmp(e.type, type) && e.profile == profile && (!text || strstr(e.msg, text))) n++;
  }
  return n;
}
static void printEventsSince(uint32_t seq) {
  for (uint32_t s = seq + 1; s <= eventSeq; s++) {
    const Event &e = events[(s - 1) % MAX_EVENTS];
    if (e.seq == s) printf("        event %-8s %s\n", e.type, e.msg);
  }
}
// goto + report: confirmed by the firmware, the right module physically there, and how well aligned
static bool gotoChecked(int sl, const char *label, bool quiet = false) {
  bool ok = motion::gotoSlot(sl);
  bool right = physSlot() == sl && motion::slotAtStation() == sl;
  if (!quiet || !ok || !right || fabs(physOffDeg()) > 1.0)
    printf("  %-44s goto slot %2d: %s, module %2d at the station, %+.2f deg%s\n", label, sl + 1, ok ? "confirmed" : "REFUSED",
           physSlot() + 1, physOffDeg(), ok && !right ? "  <-- WRONG MODULE" : "");
  return ok && right;
}
static void idleFor(int ms) { for (int t = 0; t < ms; t += 20) { fakeMs += 20; motion::idleCheck(); } }
// turned by hand at degPerS while the firmware idles (it polls the barrier every 20 ms). Back-driving the 1:256
// gearing by hand is slow; 90 deg/s is already very fast for it.
static void turnByHandIdle(double steps, double degPerS = 90) {
  double perPoll = degPerS * DEG * 0.020, done = 0;
  while (fabs(done) < fabs(steps)) {
    double d = std::min(perPoll, fabs(steps) - fabs(done)) * (steps < 0 ? -1 : 1);
    turnByHand(d);
    done += d;
    fakeMs += 20;
    motion::idleCheck();
  }
}
static void reset() {
  stallFrom = stallTo = -1; slipP = 0; abortAt = -1; missingFlag = -1; transparent = false; homeStuck = -1;
  edgeNoise = 0; homeInv = false; LEAD = 7.0 * DEG; BL = 70; motion::abortFlag = false;
}
static Job *runJob(Item *it, int n, const char *req) {
  motion::state = motion::M_IDLE;
  motion::enqueueJob(it, n, "api", req);
  Job *j = motion::nextQueued();
  motion::runJob(*j);
  return j;
}

int main() {
  stateMtx = xSemaphoreCreateRecursiveMutex();
  motion::begin();
  servoEndUs = cfg.servo_end_us;
  servoParkUs = cfg.servo_park_us;
  beliefSlot = [] { return motion::slotAtStation(); };
  onAbort = [] { motion::abortFlag = true; };
  uint32_t m0;

  printf("A) homing: both receiver polarities, start in the wide flag / in a normal flag / in a gap / next to an edge,\n"
         "   gear teeth pressed forward or backward, receiver flickering at the edges\n");
  {
    const double starts[] = {1234.0, P0 + 3.0, P0 + 4 * SLOT + 100, P0 - LEAD - 3, P0 + 6 * SLOT - LEAD + W + 2, P0 + 9000.0};
    int i = 0;
    for (int inv = 0; inv < 2; inv++)
      for (double st : starts) {
        reset();
        homeInv = inv;
        phys = st;
        gap = (i % 2) ? BL : 0;
        edgeNoise = (i % 3 == 2) ? 3 : 0;
        lastPhase = -1;
        motion::homed = false;
        bool ok = motion::homeAndPark();
        bool right = physSlot() == motion::slotAtStation();
        printf("   receiver %s while blocked, start %8.0f, teeth %s%s: %s, module %2d, %+.2f deg\n", inv ? "HIGH" : "LOW ", st,
               gap == BL ? "back" : "fwd ", edgeNoise > 0 ? ", flicker" : "         ", ok ? "homed" : "FAILED", physSlot() + 1, physOffDeg());
        expect(ok && right && fabs(physOffDeg()) < 1.0, "homing");
        i++;
      }
    reset();
    printf("   flag widths measured (deg):");
    for (int k = 0; k < NS; k++) printf(" %.1f", motion::flagW[k] / STEPS_PER_DEG);
    printf("\n");
  }

  printf("B) slot moves (real backlash 70 and 250 steps, set: %d)\n", (int)cfg.backlash);
  for (int bl : {70, 250}) {
    reset();
    BL = bl;
    motion::homeAndPark();
    m0 = mark();
    int seq[] = {0, 1, 9, 3, 4, 4, 0, 5, 2, 7, 1, 6, 8, 3};
    bool all = true;
    double worst = 0;
    for (int sl : seq) { all &= gotoChecked(sl, "", true); worst = std::max(worst, fabs(physOffDeg())); }
    printf("   backlash %3d: %zu moves %s, worst alignment %.2f deg, homed again %d times, corrections %d\n", bl, sizeof(seq) / sizeof(seq[0]),
           all ? "all confirmed at the right module" : "FAILED", worst, eventsSince(m0, "homed"), eventsSince(m0, "resync"));
    printEventsSince(m0);
    expect(all && eventsSince(m0, "homed") == 0, "slot sequence");
  }

  printf("C) lost steps during moves\n");
  reset();
  gotoChecked(2, "start", true);
  m0 = mark(); stallFrom = motorSteps + 1500; stallTo = stallFrom + 600;
  expect(gotoChecked(6, "600 steps lost in the middle of a move"), "600 lost"); printEventsSince(m0);
  m0 = mark(); stallFrom = motorSteps + 300; stallTo = stallFrom + 2000;
  expect(gotoChecked(1, "2000 steps lost (more than a slot)"), "2000 lost"); printEventsSince(m0);
  m0 = mark(); stallFrom = motorSteps + 1100; stallTo = stallFrom + 100;
  expect(gotoChecked(2, "100 lost just before the flag edge"), "100 lost before the edge"); printEventsSince(m0);
  expect(eventsSince(m0, "homed") == 0, "small loss is corrected without homing");
  m0 = mark(); stallFrom = motorSteps + 1400; stallTo = stallFrom + 150;
  motion::gotoSlot(3);
  printf("  %-44s goto slot  4: module %2d at the station, %+.2f deg (not measurable: right module, a bit short)\n",
         "150 lost after the flag edge (last 7 deg)", physSlot() + 1, physOffDeg());
  expect(physSlot() == 3, "loss after the edge keeps the right module");
  expect(gotoChecked(4, "next move"), "next move corrects it"); printEventsSince(m0);
  m0 = mark(); stallFrom = motorSteps; stallTo = stallFrom + 100000;
  bool ok = motion::gotoSlot(8);
  printf("  %-44s goto slot  9: %s, '%s'\n", "carousel jammed for good", ok ? "confirmed??" : "refused", motion::lastError);
  expect(!ok, "permanent jam must be refused");
  reset();
  expect(motion::homeAndPark(), "home after the jam");

  printf("D) carousel turned by hand while idle\n");
  for (double h : {-600.0, SLOT, 3 * SLOT + 100, 200.0}) {
    reset();
    gotoChecked(4, "", true);
    idleFor(200);
    m0 = mark();
    turnByHandIdle(h);
    idleFor(200);
    bool noticed = eventsSince(m0, "moved") > 0;
    char lbl[64];
    snprintf(lbl, sizeof(lbl), "turned %+.0f steps, idle watch: %s", h, noticed ? "noticed" : "not visible");
    expect(gotoChecked(7, lbl), "after a hand turn");
    printEventsSince(m0);
    if (fabs(h) > 300) expect(noticed, "idle watch");
  }

  printf("E) stop pressed in the middle of a move, then a new move\n");
  for (long at : {300L, 900L, 3000L}) {
    reset();
    gotoChecked(5, "", true);
    abortAt = motorSteps + at;
    bool r = motion::gotoSlot(at < 1000 ? 5 : 1);
    motion::abortFlag = false;
    char lbl[64];
    snprintf(lbl, sizeof(lbl), "stopped after %ld steps (%s)", at, r ? "move finished??" : "aborted");
    expect(!r && gotoChecked(8, lbl), "move after abort");
  }

  printf("F) raw jog (calibration), then a slot move\n");
  for (int32_t j : {-500, -1700, 700, 260}) {
    reset();
    gotoChecked(3, "", true);
    motion::runCmd(motion::Cmd{motion::CMD_JOG, j});
    char lbl[64];
    snprintf(lbl, sizeof(lbl), "jog %+d steps", (int)j);
    expect(gotoChecked(6, lbl), "move after jog");
  }

  printf("G) calibration: flags really 9 deg ahead of their slots, firmware set to 7\n");
  reset();
  LEAD = 9.0 * DEG;
  motion::homeAndPark();
  gotoChecked(0, "before calibration");
  int32_t off = (int32_t)lround(-physOffDeg() * DEG);
  motion::runCmd(motion::Cmd{motion::CMD_JOG, off});
  printf("   jogged %+d steps: slot 1 now %+.2f deg\n", (int)off, physOffDeg());
  bool cal = motion::calibrateSlot1();
  printf("   calibrate: %s, home_offset %d\n", cal ? "accepted" : "refused", (int)cfg.home_offset);
  expect(cal && fabs(cfg.home_offset + 9.0 * STEPS_PER_DEG) < 3, "calibration");
  expect(gotoChecked(6, "after calibration") && gotoChecked(0, "after calibration"), "moves after calibration");
  motion::runCmd(motion::Cmd{motion::CMD_JOG, (int32_t)SLOT});
  bool cal2 = motion::calibrateSlot1();
  printf("   slot 2 at the station, calibrate as slot 1: %s\n", cal2 ? "accepted??" : "refused");
  expect(!cal2, "calibrating at the wrong slot is refused");
  motion::runCmd(motion::Cmd{motion::CMD_JOG, (int32_t)(-SLOT - 4 * DEG)});
  bool cal3 = motion::calibrateSlot1();
  printf("   slot 1 4 deg before the station, calibrate: %s\n", cal3 ? "accepted" : "refused");
  cfg.home_offset = DEF_HOME_OFFSET;
  reset();
  motion::homeAndPark();

  printf("H) faults that must stop the machine before a stroke\n");
  Item two[2] = {{2, 1, 0, 0}, {5, 1, 0, 0}};
  slots[2].count = 30; slots[5].count = 30;
  pillsPerStroke = 1;
  struct Fault { const char *name; void (*set)(); };
  Fault faults[] = {
      {"pinion slips on the motor shaft (30%)", [] { slipP = 0.3; }},
      {"one flag broken off", [] { missingFlag = 4; }},
      {"flags let the infrared through", [] { transparent = true; }},
      {"receiver unplugged (reads HIGH)", [] { homeStuck = HIGH; }},
      {"emitter unplugged (reads LOW)", [] { homeStuck = LOW; }},
      {"carousel jammed", [] { stallFrom = motorSteps; stallTo = motorSteps + 10000000; }},
  };
  for (auto &f : faults) {
    reset();
    motion::homeAndPark();
    f.set();
    motion::homed = false;                     // e.g. after a restart
    strokes = 0;
    Job *j = runJob(two, 2, "");
    printf("   %-40s job %s, %d strokes, '%s'\n", f.name, JOB_STATE_NAME[j->state], strokes, j->error);
    expect(j->state == JOB_FAILED && strokes == 0, f.name);
  }
  reset();
  slipP = 0.02;
  motion::homed = false;
  strokes = 0;
  Job *js = runJob(two, 2, "");
  printf("   %-40s job %s, %d strokes\n", "pinion slips a little (2%)", JOB_STATE_NAME[js->state], strokes);
  expect(js->state == JOB_DONE, "small slip is corrected at every slot");

  printf("I) carousel turned by hand between two strokes of a job\n");
  reset();
  motion::homeAndPark();
  Item many[1] = {{6, 3, 0, 0}};
  slots[6].count = 30;
  strokes = 0;
  handAfterStroke = 1;
  handSteps = 1.5 * SLOT;
  m0 = mark();
  Job *jh = runJob(many, 1, "");
  printf("   job %s, %d of 3 pills, %d strokes, module at the station %d\n", JOB_STATE_NAME[jh->state], jh->items[0].dispensed, strokes, physSlot() + 1);
  printEventsSince(m0);
  expect(jh->state == JOB_DONE && eventsSince(m0, "moved") == 1 && physSlot() == 6, "hand turn during a job");

  printf("J) step count wrong by a whole slot (cannot happen by itself): slot 1's wide flag must catch it\n");
  reset();
  motion::homeAndPark();
  gotoChecked(3, "", true);
  motion::st_pos += (int32_t)lround(SLOT);
  m0 = mark();
  int moves = 0;
  for (int sl : {5, 7, 9, 1, 3}) { motion::gotoSlot(sl); moves++; if (eventsSince(m0, "resync", "looks")) break; }
  printf("   caught after %d moves: %s\n", moves, eventsSince(m0, "resync", "looks") ? "yes" : "NO");
  printEventsSince(m0);
  expect(eventsSince(m0, "resync", "looks") == 1 && physSlot() == motion::slotAtStation(), "wide flag check");

  printf("K) jobs that never pass slot 1 home again every %d jobs\n", (int)cfg.rehome_jobs);
  reset();
  motion::homeAndPark();
  m0 = mark();
  strokes = 0;
  int nj = 2 * (int)cfg.rehome_jobs + 2;
  for (int n = 0; n < nj; n++) { Item it1[1] = {{(uint8_t)(3 + n % 2), 1, 0, 0}}; slots[3 + n % 2].count = 30; runJob(it1, 1, ""); }
  printf("   %d jobs between slots 4 and 5, %d strokes, homed %d times\n", nj, strokes, eventsSince(m0, "homed"));
  expect(eventsSince(m0, "homed") == 2, "periodic homing");

  printf("L) jobs\n");
  reset();
  motion::homeAndPark();
  Item it[2] = {{2, 2, 0, 0}, {5, 1, 0, 0}};
  slots[2].count = 30; slots[5].count = 4; slots[5].low = 3;
  pillsPerStroke = 1; strokes = 0;
  Job *j = runJob(it, 2, "req-1");
  printf("   1) pills come: state=%s dispensed=%d/%d,%d/%d strokes=%d stock %d,%d\n", JOB_STATE_NAME[j->state], j->items[0].dispensed, j->items[0].count, j->items[1].dispensed, j->items[1].count, strokes, (int)slots[2].count, (int)slots[5].count);
  expect(j->state == JOB_DONE && strokes == 3 && slots[2].count == 28 && slots[5].count == 3, "normal job");
  bool ex = false;
  uint32_t id2 = motion::enqueueJob(it, 2, "api", "req-1", &ex);
  printf("   2) same request id again: job %u, existing=%d\n", (unsigned)id2, ex);
  expect(ex && id2 == j->id, "idempotent request");
  pillsPerStroke = 0; strokes = 0; j = runJob(it, 2, "req-2");
  printf("   3) module empty: state=%s dispensed=%d,%d strokes=%d error='%s'\n", JOB_STATE_NAME[j->state], j->items[0].dispensed, j->items[1].dispensed, strokes, j->error);
  expect(j->state == JOB_PARTIAL && strokes == 2 * cfg.retries, "empty module");
  pillsPerStroke = 1; rxUnplugged = true; strokes = 0; j = runJob(it, 2, "req-3");
  printf("   4) drop receiver unplugged: state=%s strokes=%d error='%s'\n", JOB_STATE_NAME[j->state], strokes, j->error);
  expect(j->state == JOB_FAILED && strokes == 0, "dead receiver must stop before any stroke");
  rxUnplugged = false; txUnplugged = true; strokes = 0; j = runJob(it, 2, "req-4");
  printf("   5) drop emitter unplugged / chute blocked: state=%s strokes=%d\n", JOB_STATE_NAME[j->state], strokes);
  expect(j->state == JOB_FAILED && strokes == 0, "dark sensor must stop before any stroke");
  txUnplugged = false; dropInv = true; strokes = 0; slots[2].count = 30; j = runJob(it, 2, "req-5");
  printf("   6) drop barrier with the opposite output polarity: state=%s dispensed=%d,%d strokes=%d\n", JOB_STATE_NAME[j->state], j->items[0].dispensed, j->items[1].dispensed, strokes);
  expect(j->state == JOB_DONE && strokes == 3, "inverted polarity");
  dropInv = false; pillsPerStroke = 2; strokes = 0;
  Item one[1] = {{2, 2, 0, 0}};
  j = runJob(one, 1, "req-6");
  printf("   7) two pills in one stroke: state=%s dispensed=%d of %d asked, strokes=%d\n", JOB_STATE_NAME[j->state], j->items[0].dispensed, j->items[0].count, strokes);
  expect(j->items[0].dispensed == 2 && strokes == 1, "double is counted, no further stroke");
  pillsPerStroke = 1; rxUnplugged = true; cfg.beam_required = false; strokes = 0; j = runJob(it, 2, "req-7");
  printf("   8) blind mode switched on by the user: state=%s dispensed=%d,%d strokes=%d\n", JOB_STATE_NAME[j->state], j->items[0].dispensed, j->items[1].dispensed, strokes);
  expect(j->state == JOB_DONE && strokes == 3, "blind mode");
  rxUnplugged = false; cfg.beam_required = true;

  printf("M) two people: reserved slots, one button each, never one person's pills on the other's\n");
  {
    reset();
    motion::homeAndPark();
    for (auto &sl : slots) { sl.profile = 0; sl.count = 50; sl.enabled = true; }
    slots[0].profile = 1; slots[1].profile = 2;             // slot 1 reserved for profile 1, slot 2 for profile 2, 3 shared
    Item a1[1] = {{0, 1, 0, 0}}, b2[1] = {{1, 1, 0, 0}}, s3[1] = {{2, 1, 0, 0}}, mix[2] = {{0, 1, 0, 0}, {1, 1, 0, 0}};
    uint8_t p = 0;
    bool okA = itemsProfile(a1, 1, p).length() == 0 && p == 1;
    p = 0; bool okB = itemsProfile(b2, 1, p).length() == 0 && p == 2;
    p = 0; bool okS = itemsProfile(s3, 1, p).length() == 0 && p == 1;
    p = 2; bool okS2 = itemsProfile(s3, 1, p).length() == 0 && p == 2;
    p = 1; String eOther = itemsProfile(b2, 1, p);
    p = 0; String eMix = itemsProfile(mix, 2, p);
    printf("   dose from slot 1 -> profile %s, slot 2 -> %s, shared slot 3 -> %s / %s for profile 2\n", okA ? "1" : "??",
           okB ? "2" : "??", okS ? "1" : "??", okS2 ? "2" : "??");
    printf("   profile 1 asking for slot 2: '%s'; one dose from slots 1 and 2: '%s'\n", eOther.c_str(), eMix.c_str());
    expect(okA && okB && okS && okS2, "profile taken from reserved / shared slots");
    expect(eOther.length() > 0 && eMix.length() > 0, "a reserved slot is refused to the other profile");

    for (auto &sc : schedules) sc = Schedule();
    for (auto &dl : dueList) dl = 0;
    Schedule &A = schedules[0], &B = schedules[1], &C = schedules[2];
    A.used = B.used = C.used = true;
    A.id = 1; B.id = 2; C.id = 3;
    A.profile = 1; B.profile = 2; C.profile = 2;
    C.confirm = true;                                        // A and B are auto doses
    A.n = B.n = C.n = 1;
    A.items[0] = a1[0]; B.items[0] = b2[0]; C.items[0] = s3[0];
    cupProfile = 0;
    pillsPerStroke = 1;
    motion::state = motion::M_IDLE;
    uint32_t t = 1800000000;
    auto runNext = []() -> Job * { Job *j = motion::nextQueued(); if (j) motion::runJob(*j); return j; };
    auto isDue = [](uint16_t id) { for (auto x : dueList) if (x == id) return true; return false; };

    m0 = mark();
    fireSchedule(A, t);
    Job *j = runNext();
    printf("   1) auto dose of profile 1, cup empty: %s, cup now holds profile %u's pills\n",
           j ? JOB_STATE_NAME[j->state] : "nothing queued", (unsigned)cupProfile);
    expect(j && j->profile == 1 && j->state == JOB_DONE && cupProfile == 1 && eventsFor(m0, "dose_started", 1) == 1, "auto dose 1");

    m0 = mark();
    fireSchedule(B, t += 60);
    j = runNext();
    printf("   2) auto dose of profile 2 right after: %s, waits for button 2: %s\n", j ? "DISPENSED" : "not dispensed",
           isDue(2) ? "yes" : "no");
    expect(!j && isDue(2) && eventsFor(m0, "dose_due", 2, "cup") == 1, "auto dose of the other profile waits");

    m0 = mark();
    uint32_t id1 = pressButton(1);
    printf("   3) button 1 pressed: %s\n", id1 ? "DISPENSED" : "nothing due for profile 1");
    expect(!id1 && eventsFor(m0, "button", 1) == 1 && isDue(2), "button 1 does not release profile 2's dose");

    uint32_t id2 = pressButton(2);
    j = runNext();
    printf("   4) button 2 pressed: job %s for profile %u, cup now holds profile %u's pills\n", j ? JOB_STATE_NAME[j->state] : "-",
           j ? (unsigned)j->profile : 0, (unsigned)cupProfile);
    expect(id2 && j && j->id == id2 && j->profile == 2 && j->state == JOB_DONE && cupProfile == 2 && !isDue(2), "button 2");

    fireSchedule(A, t += 60);
    uint32_t id0 = pressButton(0);
    j = runNext();
    printf("   5) only one button fitted: profile 1's held dose released by it: %s\n", j && j->id == id0 && j->profile == 1 ? "yes" : "NO");
    expect(j && j->id == id0 && j->profile == 1 && cupProfile == 1, "single button serves both profiles");

    fireSchedule(C, t += 60);                                // confirm dose of profile 2, then its held auto dose
    fireSchedule(B, t += 60);
    uint32_t first = pressButton(2);
    j = runNext();
    bool oldestFirst = j && j->id == first && j->items[0].slot == 2;
    uint32_t second = pressButton(2);
    j = runNext();
    printf("   6) two doses waiting for profile 2: one per press, oldest first: %s\n", oldestFirst && j && j->id == second && j->items[0].slot == 1 ? "yes" : "NO");
    expect(oldestFirst && j && j->id == second && j->items[0].slot == 1, "one dose per press, oldest first");

    m0 = mark();
    int before = strokes;
    slots[1].profile = 1;                                    // slot 2 reserved for profile 1 after B was planned
    fireSchedule(B, t += 60);
    j = runNext();
    printf("   7) slot reserved for the other profile after the plan was made: %s, '%s'\n", j ? "DISPENSED" : "refused",
           eventsFor(m0, "error", 2) ? events[(eventSeq - 1) % MAX_EVENTS].msg : "");
    expect(!j && strokes == before && eventsFor(m0, "error", 2, "reserved") == 1, "plan using a slot now reserved for the other profile");
    slots[1].profile = 2;

    m0 = mark();
    fireSchedule(C, t += 60);
    expireDue(t + (uint32_t)cfg.due_timeout_min * 60 + 1);
    printf("   8) dose not confirmed in time: %s\n", eventsFor(m0, "dose_missed", 2) == 1 && !isDue(3) ? "dose_missed for profile 2" : "NOT REPORTED");
    expect(eventsFor(m0, "dose_missed", 2) == 1 && !isDue(3), "missed dose names its profile");
    for (auto &sl : slots) sl.profile = 0;
  }

  printf("\nwrong-module strokes in all tests: %d\n", wrongStrokes);
  printf("strokes more than 1 deg off: %d (worst %.2f deg)\n", offStrokes, worstOff);
  expect(wrongStrokes == 0, "no stroke at a wrong module");
  printf(bad ? "RESULT: %d problems\n" : "RESULT: ok\n", bad);
  return bad;
}
