"""Tessellate the assembly for the web preview: quantised int16 vertices + uint16 indices, base64."""
import base64, json, math, sys
import numpy as np
import cadquery as cq
import dispenser as d
from build import build_parts, log

SCALE = 50.0   # 0.02 mm steps

def pack(shape, tol=0.3, ang=0.35):
    v, t = shape.val().tessellate(tol, ang)
    V = np.array([[p.x, p.y, p.z] for p in v]); T = np.array(t, dtype=np.int64)
    q = np.round(V * SCALE).astype(np.int64)
    # weld
    uq, inv = np.unique(q, axis=0, return_inverse=True)
    T = inv[T]
    T = T[(T[:, 0] != T[:, 1]) & (T[:, 1] != T[:, 2]) & (T[:, 0] != T[:, 2])]
    assert len(uq) < 65536, len(uq)
    assert np.abs(uq).max() < 32767
    return dict(v=base64.b64encode(uq.astype("<i2").tobytes()).decode(), i=base64.b64encode(T.astype("<u2").tobytes()).decode(),
                nv=int(len(uq)), nt=int(len(T)))

P = build_parts(); log("built")
out = {"scale": SCALE, "dims": dict(zPlateTop=d.Z_PLATE_TOP, zAxis=d.Z_AXIS, za=d.ZA, xBody0=d.X_BODY0, hopperTop=d.HOPPER_TOP,
                                    slots=d.N_SLOTS, gearC=d.GEAR_C, motorM=d.MOTOR_M, chVTop=d.CH_VTOP, chX0=d.CH_X0 + 2, chX1=d.CH_X1,
                                    chSlope=d.CH_SLOPE, cupX0=d.CUP_X0, floorT=d.FLOOR_T), "parts": {}}
def add(name, shape, **kw):
    out["parts"][name] = pack(shape, **kw); log(name, out["parts"][name]["nt"])
add("tub", P["base_tub"]); add("tower", P["tower"]); add("chute", P["chute"]); add("cup", P["cup"])
add("dog", P["drive_dog"]); add("pinion", P["pinion"], tol=0.2); add("carousel", P["carousel"], tol=0.35, ang=0.5)
add("module", P["module_body"]); add("lid", P["module_lid"]); add("hubcap", P["hub_cap"])
add("stepper", d.stepper_dummy()); add("servo", d.servo_dummy()); add("axle", d.axle_dummy())
for k in d.PILLS: add("roller_" + k, d.roller(k), tol=0.25)
for k in d.PILLS: add("gate_" + k, d.gate_in_module(k), tol=0.25)
sd = list(d.sensor_dummies().values())
add("sensors", sd[0].union(sd[1]).union(sd[2]).union(sd[3]))
out["rollers"] = list(d.PILLS)
out["pockets"] = {}
for k, (shape, L, W, T) in d.PILLS.items():
    g = d.pocket_geo(k)
    # seated pill in the roller frame: centre, and the two rotations that put it there
    c = d.from_gen_frame(cq.Workplane("XY").sphere(0.5).translate((g["x0"] + g["lp"] / 2, 0, g["floor"] + T / 2 + 0.1))) \
        .rotate((0, 0, 0), (1, 0, 0), d.PARK_DEG + g["ang"]).val().Center()
    out["pockets"][k] = dict(shape=shape, L=L, W=W, T=T, lp=round(g["lp"], 2), wp=round(g["wp"], 2), depth=round(g["depth"], 2),
                             gap=round(g["gap"], 2), pos=[round(c.x, 3), round(c.y, 3), round(c.z, 3)],
                             rx=round(math.radians(d.PARK_DEG + g["ang"]), 4), ry=round(-math.radians(d.TAPER_DEG), 4))
out["dims"].update(stroke=d.STROKE_DEG, rock=d.ROCK_DEG)
json.dump(out, open("/tmp/viewer_data.json", "w"))
import os; print("bytes", os.path.getsize("/tmp/viewer_data.json"), "tris", sum(p["nt"] for p in out["parts"].values()))

# ---- build the preview page
import base64 as b64
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tpl = open(os.path.join(root, "docs", "preview", "template.html")).read()
poster = "data:image/png;base64," + b64.b64encode(open(os.path.join(root, "docs", "img", "overview.png"), "rb").read()).decode()
html = tpl.replace("__DATA__", json.dumps(out, separators=(",", ":"))).replace("__POSTER__", poster)
open(os.path.join(root, "docs", "preview", "index.html"), "w").write(html)
print("index.html", len(html))
