// Motion: carousel stepper (28BYJ-48 + ULN2003), dispense servo, two IR light barriers (home, drop), job runner
#pragma once
#include "state.h"
#include "esp_timer.h"

namespace motion {

enum MState : uint8_t { M_BOOT, M_IDLE, M_HOMING, M_MOVING, M_DISPENSING, M_ERROR };
static const char *MSTATE_NAME[] = {"boot", "idle", "homing", "moving", "dispensing", "error"};

enum CmdType : uint8_t { CMD_HOME, CMD_PRESENT, CMD_JOG, CMD_SERVO, CMD_PARK, CMD_GOTO };
struct Cmd { CmdType type; int32_t arg; };

static volatile MState state = M_BOOT;
static volatile bool homed = false;
static volatile bool abortFlag = false;
static volatile uint32_t currentJob = 0;
static uint32_t jobsSinceMark = 0;        // jobs since the home mark was last seen (homing or passing check)
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
static volatile bool st_watch = false, st_stopOnHome = false, st_homePrev = false, st_homeSeen = false;
static volatile int32_t st_homePos = 0;
static volatile int st_homeLvl = LOW;        // level of the home input while the flag is in the beam (found by home())
// homing scan: the flag covers 14 deg of the turn, so the short stretch between two level changes is the flag
static volatile bool st_scan = false, st_scanFound = false;
static volatile int st_scanLvl = LOW, st_flagLvl = LOW;
static volatile int32_t st_scanEdge = 0, st_flagEdge = 0;
static volatile uint8_t st_scanEdges = 0, st_scanCnt = 0;

static inline void coils(uint8_t m) {
  digitalWrite(PIN_IN1, m & 1);
  digitalWrite(PIN_IN2, (m >> 1) & 1);
  digitalWrite(PIN_IN3, (m >> 2) & 1);
  digitalWrite(PIN_IN4, (m >> 3) & 1);
}

static inline bool homeActive() { return digitalRead(PIN_HOME) == st_homeLvl; }

static void stepCb(void *) {
  if (abortFlag || st_left <= 0) { xSemaphoreGive(stepSem); return; }
  st_phase = (uint8_t)((st_phase + st_phaseDir) & 7);
  coils(HALFSTEP[st_phase]);
  st_pos += st_dir;
  st_left--;
  st_done++;
  if (st_scan) {
    if (digitalRead(PIN_HOME) == st_scanLvl) st_scanCnt = 0;
    else if (++st_scanCnt >= 6) {                    // a change counts once it has lasted 6 steps
      int32_t edge = st_pos - 5 * st_dir;
      if (st_scanEdges > 0 && abs(edge - st_scanEdge) < (int32_t)(STEPS_PER_REV / 5)) {
        st_flagLvl = st_scanLvl;                     // the stretch that just ended was short: that was the flag
        st_flagEdge = st_scanEdge;
        st_scanFound = true;
        st_left = 0;
      }
      st_scanLvl = !st_scanLvl;
      st_scanEdge = edge;
      st_scanCnt = 0;
      if (st_scanEdges < 250) st_scanEdges++;
    }
  }
  if (st_watch) {
    bool on = homeActive();
    if (on && !st_homePrev && st_dir > 0) {          // leading edge of the flag, seen while moving forward
      st_homeSeen = true;
      st_homePos = st_pos;
      if (st_stopOnHome) st_left = 0;
    }
    st_homePrev = on;
  }
  int32_t k = st_done < st_left ? st_done : st_left;   // distance to the nearer end of the move -> trapezoid ramp
  uint32_t us = (k >= (int32_t)st_accel) ? st_cruise : st_start - (st_start - st_cruise) * (uint32_t)k / st_accel;
  esp_timer_start_once(stepTimer, us);
}

static bool runSteps(int32_t n, uint32_t cruise, uint32_t start, uint32_t accel, bool watch, bool stopOnHome) {
  if (n == 0) return true;
  st_dir = n > 0 ? 1 : -1;
  st_phaseDir = st_dir * cfg.car_dir;
  st_left = n > 0 ? n : -n;
  st_done = 0;
  st_cruise = cruise;
  st_start = start > cruise ? start : cruise;
  st_accel = accel ? accel : 1;
  st_watch = watch;
  st_stopOnHome = stopOnHome;
  st_homeSeen = false;
  st_homePrev = homeActive();
  xSemaphoreTake(stepSem, 0);
  coils(HALFSTEP[st_phase]);
  esp_timer_start_once(stepTimer, st_start);
  uint32_t timeoutMs = (uint32_t)(((uint64_t)st_left * st_start) / 1000ULL) + 3000;
  bool ok = xSemaphoreTake(stepSem, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
  esp_timer_stop(stepTimer);
  vTaskDelay(pdMS_TO_TICKS(40));
  coils(0);                                   // gearbox holds the carousel; no need to heat the coils
  return ok && !abortFlag;
}

static void fail(const char *msg) {
  copyStr(lastError, sizeof(lastError), msg);
  addEvent("error", msg);
}

// home mark seen during a normal move: compare with where it should be, correct silently lost steps
static bool resyncCheck() {
  if (!st_homeSeen) return false;
  st_homeSeen = false;
  jobsSinceMark = 0;
  double rev = STEPS_PER_REV;
  double e = fmod((double)st_homePos - (double)cfg.home_offset, rev);
  if (e > rev / 2) e -= rev;
  if (e < -rev / 2) e += rev;
  if (fabs(e) > STEPS_PER_SLOT * 0.04) {      // more than ~1.8 deg off
    st_pos -= (int32_t)lround(e);
    char buf[64];
    snprintf(buf, sizeof(buf), "position corrected by %ld steps at home mark", (long)lround(e));
    addEvent("resync", buf);
    return true;
  }
  return false;
}

// slotF: which slot index (0-based, may be fractional) should stand at the station
static bool gotoSlot(double slotF) {
  for (int pass = 0; pass < 2; pass++) {
    double cur = (double)st_pos / STEPS_PER_SLOT;
    double t = slotF + N_SLOTS * round((cur - slotF) / N_SLOTS);
    int32_t delta = (int32_t)lround(t * STEPS_PER_SLOT) - st_pos;
    if (delta == 0) return true;
    bool ok;
    if (delta < 0) {                          // always finish in + direction: gear backlash ends up on the same side
      ok = runSteps(delta - cfg.backlash, cfg.step_us_cruise, cfg.step_us_start, cfg.accel_steps, false, false);
      if (ok) ok = runSteps(cfg.backlash, cfg.step_us_cruise, cfg.step_us_start, cfg.accel_steps, true, false);
    } else {
      ok = runSteps(delta, cfg.step_us_cruise, cfg.step_us_start, cfg.accel_steps, true, false);
    }
    if (!ok) return false;
    if (!resyncCheck()) return true;          // position was right, done
  }
  return true;
}

static bool home() {
  state = M_HOMING;
  uint32_t fast = cfg.step_us_cruise, slow = cfg.step_us_cruise * 2;
  // 1) Turn forward until the flag has passed the light barrier once completely. Which level means "blocked"
  //    is not assumed: the flag is simply the short stretch, so either output polarity works.
  st_scanLvl = digitalRead(PIN_HOME);
  st_scanEdges = 0;
  st_scanCnt = 0;
  st_scanFound = false;
  st_scan = true;
  runSteps((int32_t)(STEPS_PER_REV * 1.25), fast, cfg.step_us_start, cfg.accel_steps, false, false);
  st_scan = false;
  if (abortFlag) { fail("homing aborted"); return false; }
  if (!st_scanFound) {
    homed = false;
    fail(st_scanEdges ? "home flag not recognised (stray light? loose plug?)"
                      : st_scanLvl == HIGH ? "home light barrier stuck HIGH - check plugs and stepper"
                                           : "home light barrier stuck LOW - check plugs, alignment");
    return false;
  }
  st_homeLvl = st_flagLvl;
  // 2) back to just before its leading edge, then a slow approach for a repeatable edge
  int32_t back = st_pos - st_flagEdge + 400;
  bool ok = runSteps(-back, fast, cfg.step_us_start, 100, false, false) && runSteps(900, slow, slow, 1, true, true);
  if (!ok || !st_homeSeen) {
    homed = false;
    fail("home mark lost on second approach");
    return false;
  }
  st_homeSeen = false;
  st_pos = cfg.home_offset;
  homed = true;
  jobsSinceMark = 0;
  addEvent("homed", st_homeLvl == LOW ? "carousel homed (flag reads LOW)" : "carousel homed (flag reads HIGH)");
  return true;
}

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
  // lost steps are only noticed when the home mark passes the sensor; if that has not happened
  // for a while, home again before dispensing
  if (cfg.rehome_jobs > 0 && jobsSinceMark >= (uint32_t)cfg.rehome_jobs) homed = false;
  jobsSinceMark++;
  if (!homed && !home()) fatal = true;
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
    if (!gotoSlot(it.slot)) { if (!abortFlag) { fail("carousel move failed"); fatal = true; } break; }
    state = M_DISPENSING;
    while (it.dispensed < it.count && !abortFlag) {
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
      state = home() ? M_IDLE : M_ERROR;
      break;
    case CMD_PRESENT:                          // bring a slot to the refill position
    case CMD_GOTO: {
      servoPark();
      if (!homed && !home()) { state = M_ERROR; break; }
      state = M_MOVING;
      double target = c.arg + (c.type == CMD_PRESENT ? cfg.refill_slots : 0);
      gotoSlot(target);
      state = M_IDLE;
      break;
    }
    case CMD_JOG:                              // raw steps, for calibration
      servoPark();
      state = M_MOVING;
      runSteps(c.arg, cfg.step_us_cruise * 2, cfg.step_us_cruise * 2, 1, false, false);
      state = M_IDLE;
      break;
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
  if (PIN_BUZZER >= 0) { pinMode(PIN_BUZZER, OUTPUT); digitalWrite(PIN_BUZZER, LOW); }
  ledcAttach(PIN_SERVO, 50, 14);
  servoWrite(cfg.servo_park_us);
  vTaskDelay(pdMS_TO_TICKS(700));
  state = home() ? M_IDLE : M_ERROR;
  for (;;) {
    Cmd c;
    if (xQueueReceive(cmdQueue, &c, pdMS_TO_TICKS(100)) == pdTRUE) {
      runCmd(c);
      continue;
    }
    Job *j = nextQueued();
    if (j) { runJob(*j); continue; }
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

static int slotAtStation() {
  long s = lround((double)st_pos / STEPS_PER_SLOT) % N_SLOTS;
  if (s < 0) s += N_SLOTS;
  return (int)s;
}

}  // namespace motion
