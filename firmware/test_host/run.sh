#!/bin/sh
# Host-side checks, no ESP32 needed:
#  1. syntax / name check of the whole sketch against stand-in headers (stubs/)
#  2. simulation of the motion logic: homing, slot moves, lost-step recovery, job handling
# The stand-in headers do not prove that the real Arduino-ESP32 or ArduinoJson APIs are used correctly.
set -e
cd "$(dirname "$0")"
g++ -std=gnu++17 -fsyntax-only -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-type-limits \
    -x c++ -Istubs -I../pilldispenser ../pilldispenser/pilldispenser.ino
echo "syntax ok"
g++ -std=gnu++17 -O1 -Istubs -I../pilldispenser sim.cpp -o /tmp/pilldispenser_sim
/tmp/pilldispenser_sim
