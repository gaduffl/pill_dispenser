// Pill dispenser firmware - build-time configuration
// Board: Seeed Studio XIAO ESP32-C6 (also fine: XIAO ESP32-S3). Arduino-ESP32 core 3.x.
#pragma once
#include <Arduino.h>

#define FW_VERSION "0.2.0"

// ---------------------------------------------------------------- pins
// Dx names are defined by the XIAO board variants, so this file works unchanged on
// XIAO ESP32-C6 and XIAO ESP32-S3. (Do not use the XIAO ESP32-C3 with this map:
// D8/D9 are boot strapping pins there and the ULN2003 inputs would pull them low.)
#define PIN_BEAM    D0    // drop light barrier in the chute: signal wire of the receiver (internal pull-up)
#define PIN_HOME    D1    // home light barrier under the carousel: signal wire of the receiver (internal pull-up)
#define PIN_BUTTON  D2    // optional push button to GND ("dispense what is due")
#define PIN_SERVO   D3    // MG90S signal
#define PIN_BEAM_LED D4   // supply wire of the drop barrier's emitter (about 10 mA): switched by the firmware to test the sensor
#define PIN_BUZZER  D6    // optional active buzzer / LED, set to -1 if unused
#define PIN_IN1     D7    // ULN2003 IN1..IN4 -> 28BYJ-48 carousel stepper
#define PIN_IN2     D8
#define PIN_IN3     D9
#define PIN_IN4     D10

// ---------------------------------------------------------------- mechanics
#define N_SLOTS            10
#define MOTOR_HALFSTEPS    4075.7728f          // 28BYJ-48: 64 half-steps x 63.684 gearbox
#define GEAR_RATIO         (60.0f / 15.0f)     // carousel gear / pinion
#define STEPS_PER_REV      (MOTOR_HALFSTEPS * GEAR_RATIO)
#define STEPS_PER_SLOT     (STEPS_PER_REV / N_SLOTS)

// ---------------------------------------------------------------- limits
#define MAX_ITEMS          8      // lines per dispense request
#define MAX_JOBS           12     // job history kept in RAM (queued + finished)
#define MAX_SCHEDULES      16
#define MAX_EVENTS         48
#define MAX_DUE            4

#define ENABLE_OTA         1

// ---------------------------------------------------------------- defaults (all changeable at runtime, PUT /api/v1/settings)
#define DEF_HOSTNAME        "pilldispenser"
#define DEF_TZ              "CET-1CEST,M3.5.0,M10.5.0/3"   // Europe/Berlin
#define DEF_STEP_US_CRUISE  1400      // ~10 rpm at the motor, ~2.6 rpm at the carousel
#define DEF_STEP_US_START   2600
#define DEF_ACCEL_STEPS     250
#define DEF_BACKLASH_STEPS  180       // ~4 deg at the carousel, final approach is always in + direction
#define DEF_HOME_OFFSET     -317      // position (in steps) of the home edge relative to "slot 1 at the station" (flag = +-7 deg)
#define DEF_SERVO_PARK_US   610       // roller pocket up, prongs horizontal
#define DEF_SERVO_END_US    2280      // ~150 deg further (145 deg stroke + coupling play): pocket over the outlet
#define DEF_SERVO_OVER_US   55        // overshoot when returning, takes up the coupling backlash (~5 deg)
#define DEF_SERVO_DPS       300       // sweep speed, degrees per second
#define DEF_ROCK_DEG        22        // short forward turn before every stroke: lines pills up in front of the pocket (0 = off)
#define DEF_RETRIES         4         // strokes per pill before the slot is reported empty / jammed
#define DEF_MAX_PER_ITEM    12        // upper bound for "count" in one request line
#define DEF_REFILL_SLOTS    2         // POST /present parks the slot this many positions away from the station
#define DEF_DUE_TIMEOUT_MIN 120
#define DEF_REHOME_JOBS     8         // re-home if the home mark was not seen during this many jobs (0 = never)
