"""Functional checks: does the coupling drive, is the roller retained, is exactly one pill metered, can it get out."""
import math
import cadquery as cq
import dispenser as d
from build import build_parts, up, vol, log
P = build_parts()
Z = d.Z_PLATE_TOP
mod = up(P["module_body"]); car = up(P["carousel"])

print("A) coupling: dog turned, roller held at 0 -> must collide once backlash is used up")
rol = up(d.roller_in_module("XL_soft", 0))
for a in (2, 5, 7, 9, 12, -5, -7, -9, -12):
    dog = P["drive_dog"].rotate((0, 0, d.Z_AXIS), (1, 0, d.Z_AXIS), a)
    print("   dog %+3d deg: overlap %.2f mm3" % (a, vol(dog, rol)))

print("B) keeper: roller pulled outwards must hit the pin, the gate must come along; rotation stays free")
pin = up(d.cyl_y(d.PIN_X, d.PIN_Z, 0.875, -40, 40))
gat = up(d.gate_in_module("XL_soft"))
for dx in (0.0, 0.5, 1.5, 3.0):
    print("   pull %.1f mm: pin overlap %.2f   roller x resting gate %.2f" % (dx, vol(pin, rol.translate((dx, 0, 0))), vol(gat, rol.translate((dx, 0, 0)))))
print("   gate alone pulled 1.5 mm against the roller: %.2f (must be > 0: it cannot leave without the roller)" % vol(gat.translate((1.5, 0, 0)), rol))
print("   pin vs roller at 0/90/145: %.3f %.3f %.3f" % tuple(vol(pin, up(d.roller_in_module("XL_soft", a))) for a in (0, 90, 145)))

def capsule(L, D):
    r = D / 2
    return (d.cyl_x(0, 0, r, -L / 2 + r, L / 2 - r).union(cq.Workplane("XY").sphere(r).translate((-L / 2 + r, 0, 0)))
            .union(cq.Workplane("XY").sphere(r).translate((L / 2 - r, 0, 0))))
def tablet(D, T): return d.cyl_z(0, 0, D / 2, -T / 2, T / 2)
SOL = {"XL_soft": capsule(30, 12.5), "L_cap0": capsule(21.7, 7.7), "S_tab6": tablet(6.0, 3.0), "M_tab10": tablet(10.0, 5.0)}

print("C) one pill in the pocket, carried through the stroke")
for pk in ("XL_soft", "L_cap0", "S_tab6"):
    g = d.pocket_geo(pk); pill = SOL[pk]
    print("   %s: pill top %.2f mm below the roller surface; gate leaves %.1f mm; a pill needs %.1f mm to stand in the pocket, has %.1f -> %s" % (
        pk, g["top_clear"], g["gap"], g["L"], g["gap"] + g["depth"],
        "CAN STAND" if g["can_stand"] else "cannot stand"))
    gt = up(d.gate_in_module(pk))
    for a in (0, 22, 45, 70, 100, 125):
        pl = up(d.pill_in_pocket(pk, pill, a)); ro = up(d.roller_in_module(pk, a))
        print("     %3d deg: pill x roller %.3f   pill x housing %.3f   pill x gate %.3f" % (a, vol(pl, ro), vol(pl, mod), vol(pl, gt)))
    # the next pill waits on top of the full pocket, under the gate
    lift0 = g["depth"]
    for extra, what in ((0.2, "lying on the roller"), (1.0, "lifted 1 mm"), (2.5, "lifted 2.5 mm")):
        p2 = up(d.pill_in_pocket(pk, pill, 0, lift=lift0 + extra))
        print("     second pill %-20s x gate %.3f  x housing %.3f" % (what, vol(p2, gt), vol(p2, mod)))
    # end of the stroke: slide the pill out along the pocket, then let it drop straight down
    for a in (137, 141, 145):
        ro = up(d.roller_in_module(pk, a))
        found = None
        for sl in range(0, 24):
            p1 = up(d.pill_in_pocket(pk, pill, a, lift=sl))
            if vol(p1, mod) > 1e-3 or vol(p1, car) > 1e-3 or vol(p1, ro) > 1e-3: break
            okay, landed = True, None
            for dz in range(1, 80, 1):
                p2 = p1.translate((0, 0, -dz))
                if max(vol(p2, mod), vol(p2, car), vol(p2, ro), vol(p2, P["tower"]), vol(p2, P["base_tub"])) > 1e-3:
                    okay = False; break
                if vol(p2, P["chute"]) > 0 or vol(p2, P["cup"]) > 0:
                    landed = dz; break
            if okay and landed is not None:
                found = (sl, landed); break
        print("     roller at %d deg:" % a, ("slides %d mm out of the pocket, then drops %d mm straight onto the chute" % found) if found else "no free straight drop")

print("D) seal: the pocket must never connect the feed gap with the outlet")
bl = d.X_BODY1 - d.X_BODY0
cone = lambda dr: cq.Workplane("XY").polyline([(0, 0), (0, d.R0 - dr), (bl, d.ROLLER_R - dr), (bl, 0)]).close().revolve(360, (0, 0, 0), (1, 0, 0))
env = cone(0.0).cut(cone(1.2))            # a 1.2 mm skin of the roller: only what is open at the surface counts
feed = d.from_pocket_frame(d.box(d.XA, d.XB, -d.T_DEAD, d.T_FLANK, 0, 60))
so = math.sin(math.radians(d.OUT_ANG)); h0, h1 = d.bore_r(d.X_BODY0) * so, d.BORE_R * so
outl = cq.Workplane("XY").workplane(offset=-1).polyline([(d.X_BODY0, -h0), (d.X_BODY1, -h1), (d.X_BODY1, h1), (d.X_BODY0, h0)]).close().extrude(d.ZA + 1)
for pk in ("XL_soft", "L_cap00", "S_tab6"):
    void = env.cut(d.roller(pk)).intersect(d.box(0.9, bl - 0.1, -40, 40, -40, 40)); leak = []; closed = opened = None
    for a in range(0, 148, 3):
        v = void.rotate((0, 0, 0), (1, 0, 0), a).translate((d.X_BODY0, 0, d.ZA))
        f, o = vol(v, feed), vol(v, outl)
        if f < 0.05 and closed is None: closed = a
        if o > 0.05 and opened is None: opened = a
        if f > 0.05 and o > 0.05: leak.append(a)
    print("   %s: closed towards the hopper from %s deg, opens to the outlet at %s deg, leak angles: %s" % (pk, closed, opened, leak or "none"))

print("E) home light barrier: beam blocked by the flag only at the home position")
beam = d.home_beam()
print("   " + "  ".join("%+d deg: %s" % (a, "blocked" if vol(d.rot_z(up(d.home_flag()), a), beam) > 0.5 else "free") for a in (-14, -10, -6, -3, 0, 3, 6, 10, 14, 180)))
