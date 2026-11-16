// Doses: schedules that fall due, the doses waiting for a button, and the buttons themselves (one per profile)
#pragma once
#include "state.h"
#include "motion.h"

static Schedule *findSchedule(uint16_t id) {
  for (auto &s : schedules) if (s.used && s.id == id) return &s;
  return nullptr;
}

static void dueRemove(uint16_t id) {
  for (int i = 0; i < MAX_DUE; i++) if (dueList[i] == id) dueList[i] = 0;
}

// Queue the items of a schedule as a job of its profile and take it off the due list. Returns 0 if the job queue is
// full, or if a slot of the schedule has been reserved for the other profile since: then nothing is dispensed and
// 'why' says so.
static uint32_t runSchedule(Schedule &s, const char *source, String *why = nullptr) {
  Lock l;
  dueRemove(s.id);
  uint8_t p = s.profile;
  String e = itemsProfile(s.items, s.n, p);
  if (e.length()) {
    if (why) {
      char msg[72];
      snprintf(msg, sizeof(msg), "schedule %u not run: %s", (unsigned)s.id, e.c_str());
      *why = msg;
    }
    return 0;
  }
  uint32_t id = motion::enqueueJob(s.items, s.n, source, "", nullptr, s.profile);
  if (!id && why) *why = "job queue full";
  return id;
}

// A schedule's time has come. Confirm doses wait for their profile's button. So does an auto dose if the pills that
// went into the cup last belong to the other profile: the cup may still hold them.
static void fireSchedule(Schedule &s, uint32_t now) {
  Lock l;
  char msg[72];
  bool hold = !s.confirm && cupMayHoldOther(s.profile);
  if (s.confirm || hold) {
    int at = -1;
    for (int i = 0; i < MAX_DUE && at < 0; i++) if (!dueList[i]) at = i;
    if (at < 0) {
      snprintf(msg, sizeof(msg), "schedule %u: too many doses waiting", (unsigned)s.id);
      addEvent("dose_missed", msg, -1, 0, s.profile);
      return;
    }
    dueList[at] = s.id;
    dueSince[at] = now;
    if (hold) snprintf(msg, sizeof(msg), "schedule %u %s, waits: cup may hold other pills", (unsigned)s.id, s.label);
    else snprintf(msg, sizeof(msg), "schedule %u %s", (unsigned)s.id, s.label);
    addEvent("dose_due", msg, -1, 0, s.profile);
    return;
  }
  String why;
  uint32_t id = runSchedule(s, "schedule", &why);
  snprintf(msg, sizeof(msg), "schedule %u %s", (unsigned)s.id, s.label);
  addEvent(id ? "dose_started" : "error", id ? msg : why.c_str(), -1, id, s.profile);
}

// doses nobody confirmed in time
static void expireDue(uint32_t now) {
  Lock l;
  for (int i = 0; i < MAX_DUE; i++) {
    if (dueList[i] && now - dueSince[i] > (uint32_t)cfg.due_timeout_min * 60UL) {
      Schedule *s = findSchedule(dueList[i]);
      char msg[40];
      snprintf(msg, sizeof(msg), "schedule %u not confirmed", (unsigned)dueList[i]);
      dueList[i] = 0;
      addEvent("dose_missed", msg, -1, 0, s ? s->profile : 0);
    }
  }
}

// The button of profile p was pressed (p = 0: the only button, it serves both profiles): dispense the dose of that
// profile that has waited longest. One dose per press.
static uint32_t pressButton(uint8_t p) {
  Lock l;
  int best = -1;
  for (int i = 0; i < MAX_DUE; i++) {
    Schedule *s = dueList[i] ? findSchedule(dueList[i]) : nullptr;
    if (!s || (p && s->profile != p)) continue;
    if (best < 0 || dueSince[i] < dueSince[best]) best = i;
  }
  if (best < 0) {
    addEvent("button", "pressed, nothing due", -1, 0, p);
    return 0;
  }
  Schedule &s = *findSchedule(dueList[best]);
  String why;
  uint32_t id = runSchedule(s, "button", &why);
  if (!id) addEvent("error", why.c_str(), -1, 0, s.profile);
  return id;
}
