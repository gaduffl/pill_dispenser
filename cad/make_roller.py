#!/usr/bin/env python3
"""Generate the metering roller and its gate for one specific pill.

  python3 make_roller.py --shape oblong --L 19 --W 8 --T 6.5 --name magnesium
  python3 make_roller.py --shape round  --L 8 --T 3.5 --name vitd

shape  round  = round flat tablet (L = diameter, T = thickness)
       oblong = capsule, softgel or oblong tablet (L = length, W = width, T = thickness)
Limits of the standard module: L <= 30 mm, W <= 12.5 mm, T <= 12.8 mm.
Measure the pill with callipers and enter the real size - the script adds the clearance.
Two files come out: roller_<name>.stl and gate_<name>.stl. They belong together.
"""
import argparse, os
import cadquery as cq
import dispenser as d
from export import clean_stl, print_pose, to_bed

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("--shape", choices=["round", "oblong"], required=True)
ap.add_argument("--L", type=float, required=True); ap.add_argument("--W", type=float); ap.add_argument("--T", type=float, required=True)
ap.add_argument("--name", default="custom")
a = ap.parse_args()
pill = (a.shape, a.L, a.W if a.W else a.L, a.T)
g = d.pocket_geo(pill)
root = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "stl")
for kind, part in (("roller", d.roller(pill)), ("gate", d.gate(pill))):
    out = os.path.join(root, "%s_%s.stl" % (kind, a.name))
    clean_stl(to_bed(print_pose(kind, part)), out)
    print("wrote", out)
print("pocket %.1f x %.1f mm, %.1f mm deep; the gate leaves %.1f mm above the roller" % (g["lp"], g["wp"], g["depth"], g["gap"]))
if g["can_stand"]:
    print("note: this pill is short and thick (length < %.1f mm). It can tip up on end inside the pocket;" % (g["gap"] + g["depth"] + 1.5))
    print("      the relieved pocket edge and the retry routine deal with most of that, but expect more retries.")
