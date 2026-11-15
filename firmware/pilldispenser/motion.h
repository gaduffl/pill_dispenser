// Motion: carousel stepper (28BYJ-48 + ULN2003), dispense servo, two IR light barriers (index, drop), job runner
#pragma once
#include "state.h"
#include "esp_timer.h"
#ifdef ARDUINO_ARCH_ESP32
#include "driver/gpio.h"
#endif

namespace motion {

enum MState : uint8_t { M_BOOT, M_IDLE, M_HOMING, M_MOVING, M_DISPENSING, M_ERROR };
static const char *MSTATE_NAME[] = {"boot", "idle", "homing", "moving", "dispensing", "error"};

enum CmdType : uint8_t { CMD_HOME, CMD_PRESENT, CMD_JOG, CMD_SERVO, CMD_PARK, CMD_GOTO };
struct Cmd { CmdType type; int32_t arg; };

static volatile MState state = M_BOOT;
static volatile bool homed = false;
static volatile bool abortFlag = false;
static volatile uint32_t currentJob = 0;
static uint32_t jobsSinceMark = 0;        // jobs since slot 1's wide flag was last recognised (homing or passing it)
static char lastError[64] = "";
static QueueHandle_t cmdQueue;

// ------------------------------------------------------------------ stepper
static const uint8_t HALFSTEP[8] = {0x1, 0x3, 0x2, 0x6, 0x4, 0xC, 0x8, 0x9};
static esp_timer_handle_t stepTimer;
static SemaphoreHandle_t stepSem;
static volatile int32_t st_pos = 0;          // logical position in half-steps, 0 = slot 1 at the station
static volatile int32_t st_left = 0, st_done = 0;
static volatile int8_t st_dir = 1, st_phaseDir = 1;
static volatile uint8_t st_phase = 0;
static volatile uint32_t st_cruise = 1400, st_start = 2600, st_accel = 250;

// ------------------------------------------------------------------ index light barrier
// Ten flags under the carousel hub pass the light barrier next to the axle, one per slot. Turning forward, a flag
// reaches the beam FLAG_LEAD_DEG before its slot stands at the station and still covers the beam there; slot 1's
// flag is twice as wide (home mark). The step timer records every change of the barrier with the position where it
// happened. After each move the firmware compares these changes with where the flags must be: the carousel has
// to arrive exactly where the step count says, at the right slot, or it is corrected or homed again.
static const int EDGE_DEB = 6;               // a change counts once it has lasted this many steps
static const int MAX_EDGES = 48;
static volatile int32_t ed_pos[MAX_EDGES];   // position of the change (first step of the new level)
static volatile int8_t ed_lvl[MAX_EDGES];    // input level after the change
static volatile int8_t ed_dir[MAX_EDGES];    // direction of travel
static volatile uint8_t ed_n = 0;
static volatile bool ed_overflow = false;
static volatile int ed_level = HIGH;         // debounced input level
static volatile uint8_t ed_cnt = 0;
static volatile uint8_t ed_stopAfter = 0;    // end the move once this many changes are recorded (0 = never)
static int ed_startLvl = HIGH;               // barrier level when the record was started
static volatile int st_flagLvl = LOW;        // input level while a flag is in the beam (found by home())
static int32_t regFlagW = (int32_t)(FLAG_DEG * STEPS_PER_DEG);        // width of a normal flag in steps (measured by home())
static int32_t homeFlagW = (int32_t)(HOME_FLAG_DEG * STEPS_PER_DEG);  // width of slot 1's flag (measured by home())
static int32_t flagW[N_SLOTS];               // flag widths measured by the last homing, [0] = slot 1 (for /diag)
static bool parked = false;                  // standing at a slot that the flags confirmed
static int restLvl = HIGH;                   // barrier level when the carousel last stopped (idle watch)
static uint8_t idleOdd = 0;
static const int32_t POS_TOL = (int32_t)(1.0f * STEPS_PER_DEG);  // arrival accepted within +-1 deg (about 1 mm at the module)

static inline void coils(uint8_t m) {
  digitalWrite(PIN_IN1, m & 1);
  digitalWrite(PIN_IN2, (m >> 1) & 1);
  digitalWrite(PIN_IN3, (m >> 2) & 1);
  digitalWrite(PIN_IN4, (m >> 3) & 1);
}

static int stableHome() {                    // majority of 5 reads, 1 ms in all
  int n = 0;
  for (int i = 0; i < 5; i++) { n += digitalRead(PIN_HOME) == HIGH; delayMicroseconds(200); }
  return n >= 3 ? HIGH : LOW;
}

static inline bool homeActive() { return digitalRead(PIN_HOME) == st_flagLvl; }

static void stepCb(void *) {
  if (abortFlag || st_left <= 0) { xSemaphoreGive(stepSem); return; }
  st_phase = (uint8_t)((st_phase + st_phaseDir) & 7);
  coils(HALFSTEP[st_phase]);
  st_pos += st_dir;
  st_left--;
  st_done++;
  int raw = digitalRead(PIN_HOME);
  if (raw == ed_level) ed_cnt = 0;
  else if (++ed_cnt >= EDGE_DEB) {
    ed_level = raw;
    ed_cnt = 0;
    if (ed_n < MAX_EDGES) {
      ed_pos[ed_n] = st_pos - (EDGE_DEB - 1) * st_dir;
      ed_lvl[ed_n] = (int8_t)raw;
      ed_dir[ed_n] = st_dir;
      ed_n++;
    } else {
      ed_overflow = true;
    }
    if (ed_stopAfter && ed_n >= ed_stopAfter) st_left = 0;
  }
  int32_t k = st_done < st_left ? st_done : st_left;   // distance to the nearer end of the move -> trapezoid ramp
  uint32_t us = (k >= (int32_t)st_accel) ? st_cruise : st_start - (st_start - st_cruise) * (uint32_t)k / st_accel;
  esp_timer_start_once(stepTimer, us);
}

// start a new record of barrier changes (the moves that follow append to it)
static void edgesBegin() {
  ed_n = 0;
  ed_overflow = false;
  ed_cnt = 0;
  ed_stopAfter = 0;
  ed_level = stableHome();
  ed_startLvl = ed_level;
}

// a move has ended: a change that happened in its last few steps has not been confirmed by the debounce yet.
// The carousel stands still now, so read the barrier properly and record the change with this move's direction.
static void edgesFlush() {
  int lvl = stableHome();
  ed_cnt = 0;
  if (lvl == ed_level) return;
  ed_level = lvl;
  if (ed_n < MAX_EDGES) {
    ed_pos[ed_n] = st_pos;
    ed_lvl[ed_n] = (int8_t)lvl;
    ed_dir[ed_n] = st_dir;
    ed_n++;
  } else {
    ed_overflow = true;
  }
}

static bool runSteps(int32_t n, uint32_t cruise, uint32_t start, uint32_t accel) {
  if (n == 0) return true;
  st_dir = n > 0 ? 1 : -1;
  st_phaseDir = st_dir * cfg.car_dir;
  st_left = n > 0 ? n : -n;
  st_done = 0;
  st_cruise = cruise;
  st_start = start > cruise ? start : cruise;
  st_accel = accel ? accel : 1;
  xSemaphoreTake(stepSem, 0);
  coils(HALFSTEP[st_phase]);
  esp_timer_start_once(stepTimer, st_start);
  uint32_t timeoutMs = (uint32_t)(((uint64_t)st_left * st_start) / 1000ULL) + 3000;
  bool ok = xSemaphoreTake(stepSem, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
  esp_timer_stop(stepTimer);
  vTaskDelay(pdMS_TO_TICKS(40));
  coils(0);                                   // gearbox holds the carousel; no need to heat the coils
  edgesFlush();
  return ok && !abortFlag;
}

static bool run(int32_t n) { return runSteps(n, cfg.step_us_cruise, cfg.step_us_start, cfg.accel_steps); }

static void fail(const char *msg) {
  copyStr(lastError, sizeof(lastError), msg);
  addEvent("error", msg);
}

// the carousel has come to rest: remember the barrier level, the idle watch compares against it
static void settle() {
  restLvl = stableHome();
  idleOdd = 0;
}

// ------------------------------------------------------------------ positions of the flags
static inline int32_t wrapRev(int32_t p) {    // into -REV/2 .. +REV/2
  double r = fmod((double)p, (double)STEPS_PER_REV);
  if (r >= STEPS_PER_REV / 2) r -= STEPS_PER_REV;
  if (r < -STEPS_PER_REV / 2) r += STEPS_PER_REV;
  return (int32_t)lround(r);
}
static inline long flagIndexAt(int32_t pos) {       // number of the last leading edge at or before pos (any integer)
  return (long)floor(((double)pos - cfg.home_offset) / STEPS_PER_SLOT);
}
static inline int32_t leadPos(long k) { return (int32_t)lround(cfg.home_offset + (double)k * STEPS_PER_SLOT); }
static inline bool isSlot1(long k) { return ((k % N_SLOTS) + N_SLOTS) % N_SLOTS == 0; }
// Number of the last leading edge passed at position pos, given whether the barrier sees a flag there. The step
// count alone can be off by the gear backlash (after a stop or a raw move backwards) or by a few lost steps; the
// barrier level settles that, because the flags and the gaps between them are much wider than this uncertainty.
static long flagCountAt(int32_t pos, bool inFlag) {
  long k = flagIndexAt(pos);
  int32_t into = pos - leadPos(k);                   // steps past that leading edge
  int32_t w = isSlot1(k) ? homeFlagW : regFlagW;
  if ((into < w) == inFlag) return k;
  if (inFlag) return (into - w) < ((int32_t)STEPS_PER_SLOT - into) ? k : k + 1;   // still in flag k, or already in the next
  return into < w - into ? k - 1 : k;                // not yet at flag k, or already past it
}
static int32_t leadToStation() {                   // steps from a flag's leading edge to its slot at the station
  double d = fmod(-(double)cfg.home_offset, (double)STEPS_PER_SLOT);
  if (d < 0) d += STEPS_PER_SLOT;
  return (int32_t)lround(d);
}
// the last run of every slot move: long enough to take up the gear backlash and then cross the leading edge of the
// target slot's flag, so that every arrival is measured against that edge
static int32_t approachSteps() { return cfg.backlash + leadToStation() + (int32_t)(2.5f * STEPS_PER_DEG); }

// "slot 1 stands at the station now": is that consistent with the flags? new_offset = where slot 1's leading edge
// would be. The station has to lie inside slot 1's flag, at least 2 deg from both of its edges (normal width).
static bool calibrationPlausible(int32_t newOffset) {
  int32_t lo = -(int32_t)((FLAG_DEG - 2.0f) * STEPS_PER_DEG), hi = -(int32_t)(2.0f * STEPS_PER_DEG);
  return newOffset >= lo && newOffset <= hi;
}

// "slot 1 stands exactly at the station now" (after jogging it there): take this position as the reference.
// Refused unless the carousel is homed and the station lies well inside slot 1's flag.
static bool calibrateSlot1() {
  if (!homed) return false;
  int32_t newOffset = wrapRev(cfg.home_offset - st_pos);
  if (!calibrationPlausible(newOffset) || stableHome() != st_flagLvl) return false;
  cfg.home_offset = newOffset;
  st_pos = 0;
  parked = false;
  return true;
}

// ------------------------------------------------------------------ homing
// Turn forward a little more than one revolution and record the light barrier. The flags are the shorter of the
// two levels (they cover a third of the turn), so the output polarity of the light barrier does not matter.
// Slot 1's flag is the one that is twice as wide. Its leading edge is at home_offset.
static bool home() {
  state = M_HOMING;
  homed = false;
  parked = false;
  edgesBegin();
  ed_stopAfter = 2 * N_SLOTS + 1;                 // 20 complete stretches after the first change = one revolution
  bool ran = run((int32_t)(STEPS_PER_REV * 1.15f));
  ed_stopAfter = 0;
  if (!ran) { fail(abortFlag ? "homing aborted" : "homing timed out"); return false; }
  char msg[64];
  int n = ed_n;
  if (n == 0) {
    fail(ed_level == HIGH ? "index barrier never changed (HIGH): jammed? see-through flags?"
                          : "index barrier never changed (LOW): jammed? emitter unplugged?");
    return false;
  }
  if (n < 2 * N_SLOTS + 1) {
    snprintf(msg, sizeof(msg), "only %d of 20 flag edges in a turn: jam, slip or broken flag?", n - 1);
    fail(msg);
    return false;
  }
  int32_t sum[2] = {0, 0};
  for (int i = 0; i < 2 * N_SLOTS; i++) sum[ed_lvl[i] == HIGH ? 1 : 0] += ed_pos[i + 1] - ed_pos[i];
  int32_t total = sum[0] + sum[1];
  int lvl = sum[1] < sum[0] ? HIGH : LOW;          // flags: the level that covers less of the turn
  for (int i = 0; i < 2 * N_SLOTS; i++) {           // a gap twice as long as normal: a flag is missing or lets light through
    int32_t len = ed_pos[i + 1] - ed_pos[i];
    if (ed_lvl[i] != lvl && len > 2 * (STEPS_PER_SLOT - FLAG_DEG * STEPS_PER_DEG)) {
      snprintf(msg, sizeof(msg), "index flags: a gap of %d deg - flag broken off or see-through?", (int)(len / STEPS_PER_DEG));
      fail(msg);
      return false;
    }
  }
  if (fabs(total - STEPS_PER_REV) > STEPS_PER_REV * 0.04f) {
    snprintf(msg, sizeof(msg), "homing: a turn took %d%% of the expected steps - slipping?", (int)(100.0f * total / STEPS_PER_REV));
    fail(msg);
    return false;
  }
  int32_t w[N_SLOTS], lead[N_SLOTS], sorted[N_SLOTS];
  int nf = 0;
  for (int i = 0; i < 2 * N_SLOTS; i++) {
    if (ed_lvl[i] != lvl) continue;
    if (nf < N_SLOTS) { w[nf] = ed_pos[i + 1] - ed_pos[i]; lead[nf] = ed_pos[i]; }
    nf++;
  }
  if (nf != N_SLOTS) { fail("index flags not recognised (pattern)"); return false; }
  int hi = 0;
  for (int i = 0; i < N_SLOTS; i++) { sorted[i] = w[i]; if (w[i] > w[hi]) hi = i; }
  for (int i = 1; i < N_SLOTS; i++)                 // insertion sort, ten values
    for (int j = i; j > 0 && sorted[j] < sorted[j - 1]; j--) { int32_t t = sorted[j]; sorted[j] = sorted[j - 1]; sorted[j - 1] = t; }
  int32_t med = sorted[N_SLOTS / 2 - 1];            // a normal flag (the wide one is the largest value)
  bool ok = med > 5.0f * STEPS_PER_DEG && med < 20.0f * STEPS_PER_DEG && w[hi] * 2 >= med * 3;
  for (int i = 0; i < N_SLOTS && ok; i++) {
    if (i != hi && (w[i] * 10 < med * 6 || w[i] * 10 > med * 14)) ok = false;
    int32_t gap = lead[(i + 1) % N_SLOTS] - lead[i] + (i == N_SLOTS - 1 ? total : 0);
    if (fabs(gap - STEPS_PER_SLOT) > STEPS_PER_SLOT * 0.1f) ok = false;
  }
  if (!ok) {
    snprintf(msg, sizeof(msg), "index flags not recognised (widest %d, normal %d steps)", (int)w[hi], (int)med);
    fail(msg);
    return false;
  }
  for (int i = 0; i < N_SLOTS; i++) flagW[(i - hi + N_SLOTS) % N_SLOTS] = w[i];
  regFlagW = med;
  homeFlagW = w[hi];
  st_flagLvl = lvl;
  st_pos = wrapRev(cfg.home_offset + (st_pos - lead[hi]));
  homed = true;
  jobsSinceMark = 0;
  settle();
  addEvent("homed", lvl == LOW ? "carousel homed (flags read LOW)" : "carousel homed (flags read HIGH)");
  return true;
}

// ------------------------------------------------------------------ slot moves, checked against the flags
enum Verdict : uint8_t { V_OK, V_SHIFTED, V_LOST };

// slot standing at the station according to the step count (0-based)
static int slotAtStation() {
  long s = lround((double)st_pos / STEPS_PER_SLOT) % N_SLOTS;
  if (s < 0) s += N_SLOTS;
  return (int)s;
}

// Go through the barrier changes recorded since edgesBegin(). Which flag an edge belongs to is counted from the start
// position (confirmed by the previous move or by homing, and guarded by the idle watch since): every leading edge
// passed forward adds one, every leading edge passed backward takes one away. A flag that passed completely forward
// must have the right width (slot 1's is wide), which checks the count itself. The last change must be the leading
// edge of the target slot's flag, passed forward, with the barrier still in that flag. err = how far that edge was
// from where the step count expected it (positive: the carousel lags behind the step count, steps were lost).
static Verdict checkArrival(int32_t start, int32_t target, int32_t &err, char *why, size_t whyLen) {
  err = 0;
  if (ed_overflow) { snprintf(why, whyLen, "too many light barrier changes"); return V_LOST; }
  long k = flagCountAt(start, ed_startLvl == st_flagLvl), kT = flagIndexAt(target), lastK = 0;
  int lastIdx = -1, n = ed_n;
  for (int i = 0; i < n; i++) {
    bool toFlag = ed_lvl[i] == st_flagLvl;
    if (ed_dir[i] > 0 && toFlag) {                         // leading edge, forward
      k++;
      lastK = k;
      lastIdx = i;
    } else if (ed_dir[i] > 0) {                            // trailing edge, forward: a whole flag went by
      if (lastIdx == i - 1 && lastIdx >= 0) {
        bool wide = (ed_pos[i] - ed_pos[lastIdx]) * 2 > regFlagW * 3;
        if (wide != isSlot1(lastK)) {
          snprintf(why, whyLen, "flag of slot %ld looks %s", ((lastK % N_SLOTS) + N_SLOTS) % N_SLOTS + 1, wide ? "wide" : "narrow");
          return V_LOST;
        }
        if (wide) jobsSinceMark = 0;                       // slot 1 recognised: the count is right
      }
    } else if (!toFlag) {                                  // leading edge, backward
      k--;
    }
  }
  if (lastIdx < 0) { snprintf(why, whyLen, "no flag reached the barrier - jammed?"); return V_LOST; }
  if (lastK < kT) { snprintf(why, whyLen, "carousel stopped short of the slot - jammed?"); return V_LOST; }
  if (lastK > kT || lastIdx != n - 1 || stableHome() != st_flagLvl) {
    snprintf(why, whyLen, "carousel went past the slot");
    return V_LOST;
  }
  err = ed_pos[lastIdx] - leadPos(lastK);
  if (labs(err) <= POS_TOL) return V_OK;
  snprintf(why, whyLen, "slot reached %ld steps off, corrected", (long)err);
  return V_SHIFTED;
}

// Bring slot `slot` (0-based) to the station and confirm it with its flag. Every call ends with a forward approach
// across the leading edge of that slot's flag, also when the slot is at the station already. A small offset is
// corrected; if the flags do not fit, the carousel homes again. Returns false only if the position cannot be confirmed.
static bool gotoSlot(int slot) {
  char why[56];
  for (int attempt = 0; attempt < 4; attempt++) {
    if (abortFlag) return false;
    if (!homed && !home()) return false;
    state = M_MOVING;
    double cur = (double)st_pos / STEPS_PER_SLOT;
    double t = slot + N_SLOTS * round((cur - slot) / N_SLOTS);
    int32_t target = (int32_t)lround(t * STEPS_PER_SLOT);
    parked = false;                                        // even if it is there already: back off and approach again
    int32_t start = st_pos, delta = target - st_pos, app = approachSteps();
    edgesBegin();
    bool ok = delta >= app ? run(delta) : (run(delta - app) && run(app));
    if (!ok) {
      if (!abortFlag) fail("carousel move timed out");
      return false;
    }
    int32_t err = 0;
    Verdict v = checkArrival(start, target, err, why, sizeof(why));
    if (v == V_OK) {
      st_pos -= err;                                       // fine correction, within POS_TOL
      parked = true;
      settle();
      return true;
    }
    if (v == V_SHIFTED) {
      st_pos -= err;
      addEvent("resync", why, slot + 1);
    } else {
      homed = false;
      char msg[72];
      snprintf(msg, sizeof(msg), "%s - homing again", why);
      addEvent("resync", msg, slot + 1);
    }
  }
  fail("carousel position not confirmed by the index flags");
  return false;
}

// Home, then go to the slot nearest to where homing ended: the carousel never rests right at a flag edge, and the
// homing is confirmed by a normal checked arrival.
static bool homeAndPark() {
  if (!home()) return false;
  return gotoSlot(slotAtStation());
}

// Called about every 20 ms while nothing moves. The carousel stands still, so the light barrier must not change.
// If it does, someone turned the carousel: its position is unknown until it has homed again.
static void idleCheck() {
  if (!homed) { idleOdd = 0; return; }
  if (stableHome() == restLvl) { idleOdd = 0; return; }
  if (++idleOdd < 3) return;
  idleOdd = 0;
  homed = false;
  parked = false;
  addEvent("moved", "carousel turned while idle - it homes before the next move");
}

// still standing where the last move confirmed it? (checked before every stroke)
static bool stillParked() { return homed && parked && stableHome() == restLvl; }

// ------------------------------------------------------------------ servo
static int32_t sv_us = 0;
static bool sv_on = false;
static uint32_t sv_lastMove = 0;
static const float US_PER_DEG = 2000.0f / 180.0f;

static void servoWrite(int32_t us) {
  if (us < 400) us = 400;
  if (us > 2600) us = 2600;
  ledcWrite(PIN_SERVO, (uint32_t)us * 16384UL / 20000UL);   // 14 bit @ 50 Hz
  sv_us = us;
  sv_on = true;
  sv_lastMove = millis();
}

static void servoOff() {
  ledcWrite(PIN_SERVO, 0);
  sv_on = false;
}

static void servoTo(int32_t us) {
  if (!sv_on || sv_us == 0) {                 // position unknown or relaxed: command it and give it time
    servoWrite(us);
    vTaskDelay(pdMS_TO_TICKS(450));
    return;
  }
  int32_t stepUs = (int32_t)(cfg.servo_dps * US_PER_DEG / 100.0f);   // per 10 ms
  if (stepUs < 1) stepUs = 1;
  while (sv_us != us) {
    int32_t d = us - sv_us;
    if (d > stepUs) d = stepUs;
    if (d < -stepUs) d = -stepUs;
    servoWrite(sv_us + d);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static inline int servoSign() { return cfg.servo_end_us >= cfg.servo_park_us ? 1 : -1; }

// back to "pocket up": overshoot a little, then return, so the roller is centred in the coupling play
static void servoPark() {
  servoTo(cfg.servo_park_us - servoSign() * cfg.servo_over_us);
  vTaskDelay(pdMS_TO_TICKS(60));
  servoTo(cfg.servo_park_us);
  vTaskDelay(pdMS_TO_TICKS(120));
}

// ------------------------------------------------------------------ drop sensor
static esp_timer_handle_t beamTimer;
static volatile bool bm_active = false, bm_blocked = false;
static volatile int bm_lit = HIGH, bm_dark = LOW;   // receiver level with the emitter on / off
static volatile uint8_t bm_cnt = 0;
static volatile uint32_t bm_events = 0, bm_clearAt = 0;
static bool bm_ok = false;

static void beamCb(void *) {
  if (!bm_active) return;
  bool blk = digitalRead(PIN_BEAM) != bm_lit;
  if (blk == bm_blocked) { bm_cnt = 0; return; }
  if (++bm_cnt < 2) return;                   // two samples in a row (1 ms) before a change counts
  bm_cnt = 0;
  uint32_t now = millis();
  if (blk) {
    if (now - bm_clearAt >= 8) bm_events++;   // re-blocked within 8 ms = same pill tumbling
    bm_blocked = true;
  } else {
    bm_blocked = false;
    bm_clearAt = now;
  }
}

static int beamLevel() {
  int n = 0;
  for (int i = 0; i < 9; i++) { n += digitalRead(PIN_BEAM) == HIGH; delayMicroseconds(200); }
  return n > 4 ? HIGH : LOW;
}

// Self-test: the emitter is switched off and on again, and the receiver has to follow. If it does not, a plug is
// loose, the two halves do not face each other, or something is lying in the chute. The two levels found here are
// then used as "blocked" and "free", so it does not matter which way round the light barrier switches.
static bool beamTest() {
  digitalWrite(PIN_BEAM_LED, LOW);
  vTaskDelay(pdMS_TO_TICKS(8));
  bm_dark = beamLevel();
  digitalWrite(PIN_BEAM_LED, HIGH);
  vTaskDelay(pdMS_TO_TICKS(8));
  bm_lit = beamLevel();
  bm_ok = bm_dark != bm_lit;
  return bm_ok;
}

static bool beamArm() {
  if (!beamTest()) { digitalWrite(PIN_BEAM_LED, LOW); return false; }
  bm_events = 0;
  bm_blocked = false;
  bm_cnt = 0;
  bm_clearAt = millis() - 100;
  bm_active = true;
  esp_timer_start_periodic(beamTimer, 500);   // 2 kHz
  return true;
}

static uint32_t beamDisarm() {
  bm_active = false;
  esp_timer_stop(beamTimer);
  digitalWrite(PIN_BEAM_LED, LOW);
  return bm_events;
}

// ------------------------------------------------------------------ one roller stroke
// returns pills seen by the sensor (0 = nothing came out), -1 = sensor unusable
static int stroke(int attempt) {
  int32_t park = cfg.servo_park_us, end = cfg.servo_end_us;
  int sgn = servoSign();
  bool armed = beamArm();
  if (!armed && cfg.beam_required) return -1;
  // Rock: a short turn forward and back. The pocket stays open to the hopper, the roller surface slides under
  // the waiting pills, pushes them square against the wall behind the pocket and lets them drift down the cone.
  if (cfg.rock_deg > 0) {
    int32_t rock = (int32_t)((cfg.rock_deg + 5) * US_PER_DEG);     // + coupling play
    for (int i = 0; i < (attempt == 0 ? 1 : 3); i++) {
      servoTo(park + sgn * rock);
      servoPark();
    }
  }
  vTaskDelay(pdMS_TO_TICKS(120));
  servoTo(end);                               // carry the pill round to the outlet
  vTaskDelay(pdMS_TO_TICKS(180));
  servoTo(end - sgn * (int32_t)(12 * US_PER_DEG));   // short shake in case it sticks
  servoTo(end);
  vTaskDelay(pdMS_TO_TICKS(220));
  servoPark();
  vTaskDelay(pdMS_TO_TICKS(150));             // time for a slow pill to reach the beam
  if (!armed) return 1;                       // blind mode: one stroke counts as one pill
  bool stuck = bm_blocked;
  int n = (int)beamDisarm();
  if (stuck) addEvent("chute_blocked", "drop sensor still covered after the stroke");
  return n;
}

// ------------------------------------------------------------------ jobs
static uint32_t enqueueJob(const Item *items, uint8_t n, const char *source, const char *requestId, bool *existing = nullptr) {
  Lock l;
  if (existing) *existing = false;
  if (requestId && requestId[0]) {
    for (auto &j : jobs)
      if (j.state != JOB_FREE && strcmp(j.request_id, requestId) == 0) { if (existing) *existing = true; return j.id; }
  }
  Job *slot = nullptr;
  for (auto &j : jobs) if (j.state == JOB_FREE) { slot = &j; break; }
  if (!slot) {                                 // recycle the oldest finished job
    for (auto &j : jobs)
      if (j.state >= JOB_DONE && (!slot || j.id < slot->id)) slot = &j;
  }
  if (!slot) return 0;                         // everything queued or running
  *slot = Job();
  slot->state = JOB_QUEUED;
  slot->id = ++jobSeq;
  slot->n = n;
  for (uint8_t i = 0; i < n; i++) { slot->items[i] = items[i]; slot->items[i].dispensed = 0; slot->items[i].missed = 0; }
  copyStr(slot->source, sizeof(slot->source), source);
  copyStr(slot->request_id, sizeof(slot->request_id), requestId);
  slot->t_created = nowUnix();
  return slot->id;
}

static Job *nextQueued() {
  Lock l;
  Job *best = nullptr;
  for (auto &j : jobs) if (j.state == JOB_QUEUED && (!best || j.id < best->id)) best = &j;
  if (best) best->state = JOB_RUNNING;
  return best;
}

static void runJob(Job &j) {
  currentJob = j.id;
  abortFlag = false;
  bool fatal = false;
  addEvent("job_started", j.source, -1, j.id);
  servoWrite(cfg.servo_park_us);               // make sure the prongs are level before the carousel moves
  vTaskDelay(pdMS_TO_TICKS(350));
  // every arrival is checked against its slot's flag; which flag it is, is counted. Slot 1's wide flag checks the
  // count itself: if it has not been recognised for a while, home again before dispensing
  if (cfg.rehome_jobs > 0 && jobsSinceMark >= (uint32_t)cfg.rehome_jobs) homed = false;
  jobsSinceMark++;
  bool done[MAX_ITEMS] = {false};
  for (uint8_t k = 0; k < j.n && !fatal && !abortFlag; k++) {
    // next line = the one whose slot is closest to the station right now
    int best = -1;
    double bestDist = 1e9, cur = (double)st_pos / STEPS_PER_SLOT;
    for (uint8_t i = 0; i < j.n; i++) {
      if (done[i]) continue;
      double d = fabs(remainder(cur - j.items[i].slot, (double)N_SLOTS));
      if (d < bestDist) { bestDist = d; best = i; }
    }
    if (best < 0) break;
    done[best] = true;
    Item &it = j.items[best];
    state = M_MOVING;
    if (!gotoSlot(it.slot)) { if (!abortFlag) fatal = true; break; }     // gotoSlot has reported why
    state = M_DISPENSING;
    while (it.dispensed < it.count && !abortFlag) {
      if (!stillParked()) {                    // the light barrier changed since the arrival: confirm the slot again
        homed = false;
        parked = false;
        addEvent("moved", "carousel moved at the station - homing again", it.slot + 1, j.id);
        state = M_MOVING;
        if (!gotoSlot(it.slot)) { if (!abortFlag) fatal = true; break; }
        state = M_DISPENSING;
      }
      int got = 0;
      for (int a = 0; a < cfg.retries && got == 0 && !abortFlag; a++) got = stroke(a);
      if (got < 0) { fail("drop light barrier failed its test - plugs? chute blocked?"); fatal = true; break; }
      if (got == 0) {
        it.missed = it.count - it.dispensed;
        addEvent("slot_empty", "no pill detected - module empty or jammed", it.slot + 1, j.id);
        break;
      }
      Lock l;
      it.dispensed += got;
      Slot &s = slots[it.slot];
      if (s.count >= 0) s.count = s.count > got ? s.count - got : 0;
      if (got > 1) addEvent("double", "more than one pill in a single stroke", it.slot + 1, j.id);
      if (s.count >= 0 && s.count <= s.low) addEvent("low_stock", s.name, it.slot + 1, j.id);
    }
  }
  servoPark();
  bool all = true, any = false;
  for (uint8_t i = 0; i < j.n; i++) {
    if (j.items[i].dispensed < j.items[i].count) all = false;
    if (j.items[i].dispensed) any = true;
  }
  {
    Lock l;
    if (fatal) { j.state = JOB_FAILED; copyStr(j.error, sizeof(j.error), lastError); }
    else if (all) j.state = JOB_DONE;
    else if (abortFlag && !any) { j.state = JOB_CANCELLED; copyStr(j.error, sizeof(j.error), "stopped"); }
    else { j.state = JOB_PARTIAL; copyStr(j.error, sizeof(j.error), abortFlag ? "stopped" : "one or more slots empty or jammed"); }
    j.t_done = nowUnix();
  }
  addEvent(j.state == JOB_DONE ? "job_done" : j.state == JOB_FAILED ? "job_failed" : "job_partial", j.error, -1, j.id);
  saveConfig();                                // remaining counts
  if (cfg.buzzer && PIN_BUZZER >= 0) {
    for (int i = 0; i < (j.state == JOB_DONE ? 1 : 3); i++) {
      digitalWrite(PIN_BUZZER, HIGH); vTaskDelay(pdMS_TO_TICKS(120));
      digitalWrite(PIN_BUZZER, LOW); vTaskDelay(pdMS_TO_TICKS(120));
    }
  }
  abortFlag = false;
  currentJob = 0;
  state = fatal ? M_ERROR : M_IDLE;
}

static void runCmd(const Cmd &c) {
  abortFlag = false;
  switch (c.type) {
    case CMD_HOME:
      servoPark();
      state = homeAndPark() || abortFlag ? M_IDLE : M_ERROR;
      break;
    case CMD_PRESENT:                          // bring a slot to the refill position
    case CMD_GOTO: {
      servoPark();
      int target = (int)((c.arg + (c.type == CMD_PRESENT ? cfg.refill_slots : 0)) % N_SLOTS);
      bool ok = gotoSlot(target);
      state = ok || abortFlag ? M_IDLE : M_ERROR;
      break;
    }
    case CMD_JOG: {                            // raw steps, for calibration (not checked against the flags)
      servoPark();
      state = M_MOVING;
      parked = false;
      edgesBegin();
      uint32_t us = cfg.step_us_cruise * 2;
      // backwards: go a little further and come back, so the jog ends like every slot move, gear teeth pressed forward
      if (runSteps(c.arg < 0 ? c.arg - cfg.backlash : c.arg, us, us, 1) && c.arg < 0) runSteps(cfg.backlash, us, us, 1);
      settle();
      state = M_IDLE;
      break;
    }
    case CMD_SERVO:                            // raw pulse width, for calibration
      servoTo(c.arg);
      break;
    case CMD_PARK:
      servoPark();
      break;
  }
  abortFlag = false;
}

static void task(void *) {
  pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT); pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);
  coils(0);
  pinMode(PIN_HOME, INPUT_PULLUP);
  pinMode(PIN_BEAM, INPUT_PULLUP);
  pinMode(PIN_BEAM_LED, OUTPUT);
  digitalWrite(PIN_BEAM_LED, LOW);
  if (PIN_BUZZER >= 0) {
    pinMode(PIN_BUZZER, OUTPUT); digitalWrite(PIN_BUZZER, LOW);
#ifdef ARDUINO_ARCH_ESP32
    // strongest drive, 40 mA instead of the default 20 mA: the active buzzer hangs on the pin directly
    gpio_set_drive_capability((gpio_num_t)PIN_BUZZER, GPIO_DRIVE_CAP_3);
#endif
  }
  ledcAttach(PIN_SERVO, 50, 14);
  servoWrite(cfg.servo_park_us);
  vTaskDelay(pdMS_TO_TICKS(700));
  state = homeAndPark() ? M_IDLE : M_ERROR;
  for (;;) {
    Cmd c;
    if (xQueueReceive(cmdQueue, &c, pdMS_TO_TICKS(20)) == pdTRUE) {
      runCmd(c);
      continue;
    }
    Job *j = nextQueued();
    if (j) { runJob(*j); continue; }
    idleCheck();                                               // nobody may turn the carousel unnoticed
    if (sv_on && millis() - sv_lastMove > 2500) servoOff();   // quiet and cool while idle
  }
}

static void begin() {
  stepSem = xSemaphoreCreateBinary();
  cmdQueue = xQueueCreate(8, sizeof(Cmd));
  esp_timer_create_args_t a = {};
  a.callback = &stepCb;
  a.name = "step";
  esp_timer_create(&a, &stepTimer);
  esp_timer_create_args_t b = {};
  b.callback = &beamCb;
  b.name = "beam";
  esp_timer_create(&b, &beamTimer);
  xTaskCreate(task, "motion", 8192, nullptr, 2, nullptr);
}

static bool busy() { return state != M_IDLE && state != M_ERROR; }

static bool send(CmdType t, int32_t arg = 0) {
  Cmd c{t, arg};
  return xQueueSend(cmdQueue, &c, 0) == pdTRUE;
}


}  // namespace motion
