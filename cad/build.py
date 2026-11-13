"""Build all parts, run fit checks, export STL/STEP, render previews."""
import sys, time, math, os, json
import cadquery as cq
import dispenser as d

T0 = time.time()
def log(*a): print("[%5.1fs]" % (time.time() - T0), *a, flush=True)

def up(o, z=d.Z_PLATE_TOP): return o.translate((0, 0, z))

def build_parts():
    P = {}
    P["module_body"] = d.module_body()
    P["module_lid"] = d.module_lid()
    for k in d.PILLS: P["roller_" + k] = d.roller(k)
    for k in d.PILLS: P["gate_" + k] = d.gate(k)
    P["carousel"] = d.carousel()
    P["pinion"] = d.pinion()
    P["base_tub"] = d.tub()
    ch, zb = d.chute(); P["chute"] = ch; P["_beam_z"] = zb
    P["tower"] = d.tower()
    P["drive_dog"] = d.drive_dog()
    P["cup"] = d.cup()
    P["hub_cap"] = d.hub_cap()
    return P

def vol(a, b):
    try:
        r = a.intersect(b)
        return sum(s.Volume() for s in r.vals() if hasattr(s, "Volume"))
    except Exception as e:
        return float("nan")

if __name__ == "__main__":
    P = build_parts(); log("parts built")
    for k, v in P.items():
        if k.startswith("_"): continue
        bb = v.val().BoundingBox()
        print("  %-16s bbox %6.1f x %6.1f x %6.1f   vol %7.1f cm3" % (k, bb.xlen, bb.ylen, bb.zlen, v.val().Volume() / 1000))
