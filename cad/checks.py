"""Fit / collision checks. Every number printed is an interference volume in mm^3 (should be ~0)."""
import math, time
import cadquery as cq
import dispenser as d
from build import build_parts, up, vol, log

P = build_parts(); log("built")
Z = d.Z_PLATE_TOP
mod = up(P["module_body"]); car = up(P["carousel"]); lid = up(P["module_lid"], Z + d.HOPPER_TOP)
pin = P["pinion"].translate((-d.GEAR_C, 0, d.MOTOR_M + 1.5))
static = {"tub": P["base_tub"], "tower": P["tower"], "chute": P["chute"], "cup": P["cup"],
          "dog": P["drive_dog"], "stepper": d.stepper_dummy(), "servo": d.servo_dummy(), "axle": d.axle_dummy()}
static.update(d.sensor_dummies())

print("\n1) roller + gate in the module, through the stroke")
for k in ("XL_soft", "L_cap0", "S_tab6"):
    gt = d.gate_in_module(k)
    print("   %-8s gate x module %.3f" % (k, vol(P["module_body"], gt)))
    for a in (0, 22, 45, 90, 120, 141, 150, -5):
        ro = d.roller_in_module(k, a)
        print("   %-8s %4d deg: roller x module %.3f   roller x gate %.3f" % (k, a, vol(P["module_body"], ro), vol(gt, ro)))
print("   gate while it is pushed in (5 / 15 / 30 mm out):", " ".join(
    "%.3f" % vol(P["module_body"], d.gate_in_module("S_tab6").translate((dx, 0, 0))) for dx in (5, 15, 30)))

print("\n2) module on the carousel: locked / while sliding in / neighbours / lid")
for dx in (0.0, 3.5, 7.0):
    print("   slide %.1f: %.3f" % (dx, vol(mod.translate((dx, 0, 0.0 if dx == 0 else 0.65)), car)))
print("   neighbour modules: %.3f" % vol(mod, d.rot_z(mod, d.SLOT_ANG)))
print("   lid vs body: %.3f" % vol(mod, lid))

print("\n3) static parts against each other")
names = list(static)
for i in range(len(names)):
    for j in range(i + 1, len(names)):
        v = vol(static[names[i]], static[names[j]])
        if v > 1e-3 or v != v: print("   %s x %s: %.3f" % (names[i], names[j], v))
print("   pinion x stepper: %.3f   pinion x tub: %.3f" % (vol(pin, static["stepper"]), vol(pin, static["tub"])))
print("   gear mesh (carousel x pinion) at 0 deg: %.3f" % vol(car, pin))

print("\n4) carousel + modules sweeping past everything that stands still")
worst = {}
def track(tag, part, ang):
    r = d.rot_z(part, ang)
    for n, s in static.items():
        v = vol(r, s)
        key = tag + " x " + n
        if v != v or v > worst.get(key, (0, 0))[0]: worst[key] = (v, ang)
for step in range(0, 25):
    track("carousel", car, step * 1.5)
log("carousel swept")
rol0 = up(d.roller_in_module("XL_soft", 0)); gat0 = up(d.gate_in_module("XL_soft"))
for step in range(0, 120):
    track("module", mod, step * 3.0); track("roller", rol0, step * 3.0); track("gate", gat0, step * 3.0)
for step in range(-30, 31):          # fine steps while a module passes the station
    track("module", mod, step * 0.75); track("roller", rol0, step * 0.75); track("gate", gat0, step * 0.75)
log("module swept")
for k in sorted(worst): print("   %-20s worst %.3f at %.1f deg" % (k, *worst[k]))
if not worst: print("   no contact anywhere")
# the home flag is the only feature that is not periodic with the slot pitch
flag = up(d.home_flag()); wb = 0
for a in range(0, 360, 2):
    b = d.rot_z(flag, a)
    wb = max([wb, vol(b, pin)] + [vol(b, static[n]) for n in ("tub", "tower", "chute", "stepper", "axle", "home_rx", "home_tx")])
print("   home flag, full turn: %.3f" % wb)

print("\n5) dispense stroke at the station (dog + roller turning together)")
for a in (0, 22, 45, 90, 141, 165, 175):
    dog = P["drive_dog"].rotate((0, 0, d.Z_AXIS), (1, 0, d.Z_AXIS), a)
    rol = up(d.roller_in_module("XL_soft", a))
    print("   %3d deg: dog x module %.3f  dog x roller %.3f  dog x tower %.3f  roller x tower %.3f  dog x gate %.3f" % (
        a, vol(dog, mod), vol(dog, rol), vol(dog, P["tower"]), vol(rol, P["tower"]), vol(dog, gat0)))

print("\n6) clearances that matter")
print("   prong top to drive flat: %.2f mm" % (d.TOOTH_FLAT - d.PRONG_HI))
print("   prong tip to roller cone: %.2f mm" % (d.PRONG_TIP - d.WEB_SPH))
print("   backlash angle of the coupling: +-%.1f deg" % math.degrees(math.atan((d.TOOTH_FLAT - d.PRONG_HI) / (d.PRONG_Y1 - 1.2))))
so = math.sin(math.radians(d.OUT_ANG))
for sx, sy in d.STUDS[1:2]:
    print("   stud head to slot boundary rib: %.2f mm" % (sx * math.tan(d.HALF) - 1.2 - (sy + d.STUD_HEAD_D / 2)))
    print("   keyhole to drop window: %.2f mm" % (sy - d.STUD_HEAD_D / 2 - 0.45 - (d.BORE_R * so + 0.5)))
