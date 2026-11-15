# Assembly and wiring guide

Design revision 0.3. This guide goes from printed parts to the first dispensed pill. Plan an afternoon:
about an hour for the mechanics, an hour for the wiring, and an hour for firmware and calibration.

**Directions.** *Front* is the side with the tower and the cup. *Left* and *right* are as seen standing in front
of the dispenser. *Slot 1* is the place marked 1 on top of the carousel.

**Status.** Nothing has been built yet (see the [README](../README.md)). All positions and clearances in this
guide come from the CAD model and were checked there. Where real parts are likely to differ, the guide says what
to adjust.

1. [Before you print](#1-before-you-print)
2. [Tools and parts](#2-tools-and-parts)
3. [Carousel](#3-carousel)
4. [Base: axle and stepper](#4-base-axle-and-stepper)
5. [Tower and servo](#5-tower-and-servo)
6. [Light barriers](#6-light-barriers)
7. [Electronics in the base](#7-electronics-in-the-base)
8. [Wiring, hole by hole](#8-wiring-hole-by-hole)
9. [Check before power](#9-check-before-power)
10. [Carousel on](#10-carousel-on)
11. [Firmware](#11-firmware)
12. [First start and calibration](#12-first-start-and-calibration)
13. [Modules](#13-modules)
14. [If something is wrong](#14-if-something-is-wrong)

## 1. Before you print

- **Print the carousel and the chute in a dark, opaque filament** (black or dark grey). The ten index flags under
  the carousel and the walls of the chute must block infrared light. Light-coloured or translucent PETG can let
  enough infrared through that a light barrier never reads "blocked". All other parts can be any colour.
- **Check one light barrier half against the model.** The cradles in the base and the pockets in the chute are
  made for housings of 20 x 10 x 8 mm with the lens in the middle of a small end face (Adafruit 2167). Clones can
  differ.
- Files, quantities and print settings: [README, Parts to print](../README.md#parts-to-print). No part needs
  supports.

Under the carousel, in a ring around the hub, hang ten short curved walls: the **index flags**, one per slot.
They tell the firmware which module stands at the station, so the servo never turns the wrong roller. Slot 1's
flag is twice as wide as the others (24 instead of 12 degrees). After printing, check that all ten are there and
unbroken. Do not trim or sand them.

## 2. Tools and parts

Tools: 2.5 mm hex key (M3 screws), 13 mm spanner or socket (M8 nut), small Phillips screwdriver (servo screws),
side cutters, double-sided foam tape, and tape or stick-on cable clips for the wires.

| Parts (see the [shopping list](shopping-list.md)) | Step |
|---|---|
| 2 ball bearings 608, hex bolt M8 x 80, washer M8, self-locking nut M8 | 3, 4, 10 |
| 28BYJ-48 stepper with ULN2003 driver board, 2 screws M3 x 10 | 4, 7 |
| MG90S servo, 4 screws M3 x 12, servo extension lead 15 cm | 5, 8 |
| 2 IR break-beam sensor pairs (Adafruit 2167 type, 23 cm wires with male pins) | 6, 8 |
| XIAO ESP32-C6 with pre-soldered headers, 170-point mini breadboard | 7, 8 |
| Electrolytic capacitor 470 uF, 16 V or more | 8 |
| Jumper wires: 6 short male-male (links on the board), 6 female-male (ULN2003), 3 male-male (servo) | 8 |
| USB-C power supply 5 V / 3 A | 11 |
| Optional: 12 mm panel push button with leads, active 3 V buzzer (20 mA or less), 1 short link and 2 to 4 jumpers | 8 |

## 3. Carousel

1. Press one 608 bearing into each end of the hub.
2. Check the ten index flags (section 1).

## 4. Base: axle and stepper

![The base with the electronics in place](img/base.png)

1. Push the M8 x 80 bolt up through the floor of the base. Its head sits in the hex pocket underneath and cannot
   turn.
2. The stepper hangs between two posts at the back right. Lower it in from above, shaft up. The shaft is off
   centre on the motor: turn the motor so that the shaft is towards the centre of the base and the cable outlet
   towards the wall. Fix it with 2 x M3 x 10 through its two ears into the posts.
3. Press the pinion onto the shaft, flat to flat, until it stops.

## 5. Tower and servo

1. Slide the servo into the tower from the front, output shaft first, so that the shaft points towards the
   carousel. Fix it with its two screws.
2. Lead the servo cable to the narrow slot in the tower's back plate, just right of the servo block (seen from
   the front). Push the plug through it standing on edge; the slot is 3 mm wide. From there the cable drops over
   the front wall into the base. Plug the 15 cm extension lead onto the servo's plug: the servo cable alone does
   not reach the breadboard.
3. Fix the tower to the front of the base with 4 x M3 x 12, screwed from inside the base into the tower. The holes
   in the base are vertical slots: leave the screws slightly loose, the height is set during calibration.
4. Press the servo cable down along the inside of the front wall to the floor and tape it there. The carousel's
   ribs pass just above the wall.

## 6. Light barriers

Each light barrier has two halves: an **emitter** with two wires (red, black) and a **receiver** with three
(red, black, white). The lenses of a pair face each other. A half fits its cradle or pocket only one way round.
If one sits loose, wrap a strip of tape around it.

**Index light barrier, under the carousel.** Left of the centre pillar stand two cradles in a line. Push the
receiver into the inner cradle (the one nearer the pillar) and the emitter into the outer one, each with its lens
towards the gap between them. The index flags pass through that gap. The wires leave at the far ends of the
cradles: take them straight down to the floor and tape them down there. Nothing may stick out into the ring
where the flags pass (33 to 35 mm from the axle, from 36 mm above the floor upwards).

**Drop light barrier, in the chute.** The chute has a pocket in each side wall. Push one half into each, lens
first. Which half goes left does not matter. Set the chute into its seat behind the cup bay, sloping down towards
the cup. Lead the wires down to the floor and along the floor to the back (see the layout drawing below).

The sensor wires are 23 cm long. The half on the right side of the chute needs about 21 cm of that to reach the
breadboard, so take the short way past the centre pillar, not along the wall. A female-male jumper extends a
wire that falls short.

## 7. Electronics in the base

![Layout of the base, seen from above](img/layout.svg)

1. **XIAO onto the breadboard.** Hold the breadboard as in the [breadboard drawing](#8-wiring-hole-by-hole): the
   end with column 1 away from you (it goes against the back wall). Push the XIAO in with its USB-C socket at the
   column 1 end, its pins D0 to D6 in the third hole from the left edge (row c) and 5V to D7 in the second hole
   right of the centre channel (row g). Two holes stay free beside it on the left, three on the right. Your
   board's printed letters may run the other way: go by the drawing.
2. **Breadboard into its pocket.** At the back of the base is a rectangle with a low rim, open towards the wall.
   Peel the backing off the breadboard and set it in with the XIAO end hard against the wall, so that the USB-C
   socket looks into the opening in the back wall. Check from outside that the USB-C plug goes in fully. Plugs
   with a moulded grip of up to 15 x 11 mm fit through the opening.
3. **ULN2003 board.** Its pocket is on the floor to the right, in front of the stepper: a low rim of 38 x 35 mm.
   Smaller boards sit in the middle of it. Fix the board with a pad of foam tape, components up, the white motor
   socket facing the stepper. Plug the stepper in and coil the spare cable flat on the floor.

## 8. Wiring, hole by hole

Everything runs from the 5 V of the USB-C cable. Nothing is soldered.

![Breadboard wiring, seen from above](img/wiring.svg)

**Hole names.** Columns are counted from the back wall, 1 to 17. Rows are a to e on the left half and f to j on
the right half, seen from the front. In each column, a to e are connected to each other, and so are f to j. The
two halves are not connected.

Work in this order. Use short jumpers on the board and lay them flat.

**Links on the board** (short male-male jumpers). They turn three columns into supply rails and give the two
chute wires a landing place they can reach:

| From | To | Result |
|---|---|---|
| 1i | 9f | column 9, right half: 5 V |
| 2i | 10g | column 10, right half: GND |
| 10e | 10f | column 10, left half: GND as well |
| 3i | 11f | column 11, right half: 3.3 V |
| 12b | 1a | column 12, left half: D0 |
| 13b | 5a | column 13, left half: D4 |

**Capacitor**, 470 uF: long leg (+) into 1j, short leg (-, the side with the stripe) into 2j. Bend it so that
the body lies flat along the outside of the breadboard, pointing towards the front and not towards the wall. It
keeps the servo's starting current from resetting the XIAO.

**Stepper driver**, 6 female-male jumpers from the pins of the ULN2003 board. If the board has a jumper next to
its + and - pins, leave it in place.

| ULN2003 pin | Hole | Connects to |
|---|---|---|
| IN1 | 7j | D7 |
| IN2 | 6j | D8 |
| IN3 | 5j | D9 |
| IN4 | 4j | D10 |
| + | 9g | 5 V |
| - | 10h | GND |

**Servo**, through the extension lead and 3 male-male jumpers in its free end. Some servos have yellow, red and
black wires instead.

| Servo wire | Hole | Connects to |
|---|---|---|
| orange (signal) | 4a | D3 |
| red | 9h | 5 V |
| brown | 10i | GND |

**Index light barrier.** The wires plug straight in.

| Wire | Hole | Connects to |
|---|---|---|
| receiver, white | 2a | D1 |
| receiver, red | 11g | 3.3 V |
| receiver, black | 10a | GND |
| emitter, red | 11h | 3.3 V |
| emitter, black | 10b | GND |

**Drop light barrier.**

| Wire | Hole | Connects to |
|---|---|---|
| receiver, white | 12a | D0, through the link 12b-1a |
| receiver, red | 11i | 3.3 V |
| receiver, black | 10c | GND |
| emitter, red | 13a | D4, through the link 13b-5a |
| emitter, black | 10d | GND |

The emitter in the chute runs from pin D4 on purpose. Before every stroke the firmware switches it off and on and
checks that the receiver follows. A loose plug, two halves that do not face each other, or a pill lying in the
chute are noticed before a pill is released. The receivers use the pull-ups inside the XIAO, so no resistors are
needed.

**Optional: button and buzzer.** First one more link, 10j to 14f: column 14, right half, becomes GND.

| Part | Hole | Connects to |
|---|---|---|
| button, one wire | 3a | D2 |
| button, other wire | 14g | GND |
| buzzer + | 6a | D5 |
| buzzer - | 14h | GND |

The button goes into the 12 mm hole in the right-hand side wall, nut inside. The buzzer is driven straight from
a pin, so use an active buzzer for 3 V that draws 20 mA or less. Leave D6 (7a, 7b) free: it is the serial
transmit pin and chatters while the XIAO boots.

**Lay the wires** flat on the floor, as in the layout drawing: the servo, stepper driver and button wires on
the right, the light barrier wires on the left. Wires may pass under the stepper. Tape them down every few
centimetres. Near the centre the index flags come down to 36 mm above the floor and elsewhere the carousel's
ribs to about 68 mm, so no loop of wire may stand up.

Every hole in use, for checking:

| Column | Left half (a-e) | Right half (f-j) |
|---|---|---|
| 1 | a: link from 12b; c: XIAO D0 | g: XIAO 5V; i: link to 9f; j: capacitor + |
| 2 | a: index receiver white; c: XIAO D1 | g: XIAO GND; i: link to 10g; j: capacitor - |
| 3 | a: button (opt.); c: XIAO D2 | g: XIAO 3V3; i: link to 11f |
| 4 | a: servo orange; c: XIAO D3 | g: XIAO D10; j: ULN2003 IN4 |
| 5 | a: link from 13b; c: XIAO D4 | g: XIAO D9; j: ULN2003 IN3 |
| 6 | a: buzzer + (opt.); c: XIAO D5 | g: XIAO D8; j: ULN2003 IN2 |
| 7 | c: XIAO D6, not used | g: XIAO D7; j: ULN2003 IN1 |
| 9 | - | f: link from 1i; g: ULN2003 +; h: servo red |
| 10 | a to d: black wires of the four light barrier halves; e: link to 10f | f: link from 10e; g: link from 2i; h: ULN2003 -; i: servo brown; j: link to 14f (opt.) |
| 11 | - | f: link from 3i; g, h, i: red wires of the index receiver, index emitter, drop receiver |
| 12 | a: drop receiver white; b: link to 1a | - |
| 13 | a: drop emitter red; b: link to 5a | - |
| 14 | - | f: link from 10j; g: button; h: buzzer - (all optional) |

The holes right next to the XIAO (rows b and h, columns 1 to 8) touch the edge of its board. The plan leaves
them free.

## 9. Check before power

- 5 V and GND not swapped: ULN2003 + in 9g and - in 10h, servo red in 9h and brown in 10i, the capacitor's stripe
  in 2j.
- The red wires of the light barriers are in column 11 (3.3 V) or, for the chute's emitter, in 13a: none is in
  column 9 (5 V).
- Every wire lies flat on the floor and nothing reaches into the ring around the centre pillar.
- The USB-C cable is still unplugged.

## 10. Carousel on

1. Lower the carousel onto the bolt. The gear under its plate has to mesh with the pinion: turn the carousel
   slowly by hand until the teeth find each other and it drops into place.
2. Add the washer and the self-locking nut. Tighten until the plate has no wobble but still turns without
   binding. Push the hub cap onto the nut.
3. Push the cup into its bay from the front.

Leave the drive dog off the servo for now, and the modules off the carousel.

## 11. Firmware

Folder `firmware/`. Either:

- **Arduino IDE:** install the board package "esp32" by Espressif (version 3.x) and the library "ArduinoJson"
  (7.x), select the board "XIAO_ESP32C6", open `firmware/pilldispenser/pilldispenser.ino` and upload, or
- **PlatformIO:** run `pio run -t upload` in `firmware/`.

Connect the USB-C cable at the back of the base to your computer. Firmware 0.3.0 has been tested in a simulation
on a PC but has not yet been compiled for the ESP32. If the compiler reports an error, the fix is most likely
small.

## 12. First start and calibration

After every power-up the dispenser homes: the carousel turns a little more than once while the firmware reads the
ten flags, then it stops at the nearest slot.

**Wi-Fi.** On the first start the dispenser opens the Wi-Fi network `PillDispenser-xxxx`. Join it, open
`http://192.168.4.1/setup` and enter your Wi-Fi. After the restart it answers at `http://pilldispenser.local/`
with a small test page. Set an API key before you rely on it:

```
curl -X PUT http://pilldispenser.local/api/v1/settings -H 'Content-Type: application/json' \
     -d '{"api_key":"a-long-random-string"}'
```

Then send the key with every request (`-H 'Authorization: Bearer a-long-random-string'`). The examples below
leave it out for brevity, and assume `H=http://pilldispenser.local/api/v1` and `J='Content-Type: application/json'`.

**Calibration, once:**

1. **Direction.** While it homes, the carousel must turn clockwise seen from above: the slots pass the station
   from right to left, and slot 2 arrives after slot 1. If it turns the other way:
   `curl -X PUT $H/settings -H "$J" -d '{"car_dir":-1}'`, then `curl -X POST $H/home`.
2. **Sensors.** `curl $H/diag` should show:
   - `"beam_ok": true`. The drop light barrier works. If not, the two halves in the chute do not face each other,
     a plug is wrong (receiver white in 12a, emitter red in 13a), or something lies in the chute.
   - `"index_flags_deg"`: ten widths, slot 1 first, about 24 to 27 for slot 1 and 12 to 15 for the others. They
     read a little wider than the flags are because the beam has a width.
   - `"position_confirmed": true`.

   If homing failed, `curl $H/status` shows why; see section 14.
3. **Servo and drive dog.** `curl -X POST $H/servo -H "$J" -d '{"position":"park"}'`, then press the drive dog
   onto the spline with the prongs horizontal and fix it with the servo's horn screw. Fine-tune with
   `servo_park_us`. `{"position":"end"}` should turn the dog about 150 degrees, the upper side of the roller moving
   to the left when you look at the tower from the front. Adjust `servo_end_us`. If the servo runs the wrong way,
   swap the two values' roles: park near 2280, end near 610.
4. **Slot 1.** Put a module with its roller and gate on slot 1 (section 13); it needs no pills yet. Then
   `curl -X POST $H/goto -H "$J" -d '{"slot":1}'`, and nudge with `curl -X POST $H/jog -H "$J" -d '{"steps":40}'`
   until the module stands exactly in front of the tower with its roller centred on the drive dog. Negative values go back, and 45 steps are about 1 degree.
   `curl -X POST $H/calibrate/slot1` stores the position. It answers 409 if slot 1's flag is not in the light
   barrier at that position. Then either the slot at the station is not slot 1, or the module stands more than
   about 5 degrees from where the flags expect it: check the slot number and the flag.
5. **Tower height.** With a module at the station, the prongs should sit 0.5 to 1 mm below the flat of the
   roller's tooth and must not touch the module. Shift the tower in its slots, then tighten the 4 screws.
6. **Try it.** Put a few pills into module 1, then
   `curl -X POST $H/dispense -H "$J" -d '{"items":[{"slot":1,"count":1}]}'`.

## 13. Modules

1. Lay the gate onto its roller: the tab drops into the groove near the tooth. Rollers and gates carry the same
   name and are used as a pair.
2. Push both into the module together, and slide a 60 mm piece of filament through the small hole under the
   roller to keep them in.
3. Set the module's three studs into the wide ends of the keyholes on the carousel and slide it towards the centre
   until the bump clicks into its hole. Fill it, close the lid.

To remove a module, turn it away from the station (`POST /present`), lift its outer edge slightly and pull it
outwards.

## 14. If something is wrong

Homing errors (shown by `GET /status`, the dispenser stops):

| Message | Meaning, what to do |
|---|---|
| `index barrier never changed (HIGH): jammed? see-through flags?` | The barrier never saw a flag. Either the carousel did not turn (stepper plug, ULN2003 wires in 4j to 7j, gear not meshing) or the flags let infrared through: cover them with black tape or reprint them in a dark colour. |
| `index barrier never changed (LOW): jammed? emitter unplugged?` | The barrier read "blocked" all the time: emitter not powered (red in 11h, black in 10b), halves not facing each other, or a wire in the beam. |
| `only N of 20 flag edges in a turn: jam, slip or broken flag?` | The carousel stopped or slipped during the turn, or a flag is broken. |
| `index flags: a gap of N deg - flag broken off or see-through?` | One flag is missing or lets light through. |
| `homing: a turn took N% of the expected steps - slipping?` | The pinion slips on the shaft or the gears skip. Check the pinion, and that the carousel turns freely (nut not too tight). |
| `index flags not recognised (...)` | The flag pattern is irregular: a wire or a loose sensor half in the beam, or a damaged flag. |

During operation (`GET /events`):

| Event or error | Meaning |
|---|---|
| `resync` | A slot was reached a few steps off and corrected, or the flags disagreed with the step count and the carousel homed again. Now and then is normal. Often means friction or a slipping pinion. |
| `moved` | The carousel was turned while it stood still, by hand or by a slipping gear. It homes before the next move. |
| job error `carousel position not confirmed by the index flags` | Four tries in a row did not confirm the slot. The dispenser stops rather than dispense from the wrong module. Check the index light barrier and the flags. |
| `drop light barrier failed its test - plugs? chute blocked?` | The chute's receiver did not follow its emitter: plugs in 12a and 13a, the links 12b-1a and 13b-5a, or something in the chute. |
| The XIAO restarts when the servo starts | Capacitor missing or the wrong way round, or a weak supply or cable. Use a 5 V / 3 A supply with a fixed cable. |

Steps lost in the last few degrees of a move cannot be measured, because the flag's edge has already passed. The
right module is still at the station and the next move corrects the position.
