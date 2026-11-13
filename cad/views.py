import sys, time, math
import cadquery as cq
import dispenser as d
from build import build_parts, up, log
from preview import mesh_of, render

COL = dict(tub=(92, 100, 112), carousel=(205, 208, 212), module=(120, 170, 215), lid=(240, 240, 240),
           roller=(238, 150, 60), tower=(92, 100, 112), chute=(235, 205, 90), cup=(245, 245, 245),
           dog=(220, 70, 70), gate=(90, 165, 100), sensor=(40, 40, 46), pinion=(220, 70, 70), motor=(170, 175, 60), servo=(60, 90, 200), axle=(160, 160, 165))

def assembly(P, car_ang=0.0, roll_ang=0.0, n_mod=d.N_SLOTS, pill="XL_soft", lids=True, cut=None):
    items = []
    def add(name, obj, col):
        if cut is not None:
            try:
                obj = obj.cut(cut)
            except Exception:
                obj = obj.cut(cut, clean=False)
        items.append((name, obj, col))
    add("tub", P["base_tub"], COL["tub"]); add("tower", P["tower"], COL["tower"])
    add("chute", P["chute"], COL["chute"]); add("cup", P["cup"], COL["cup"])
    add("dog", P["drive_dog"].rotate((0, 0, d.Z_AXIS), (1, 0, d.Z_AXIS), roll_ang), COL["dog"])
    add("pinion", P["pinion"].translate((-d.GEAR_C, 0, d.MOTOR_M + 1.5)), COL["pinion"])
    add("motor", d.stepper_dummy(), COL["motor"]); add("servo", d.servo_dummy(), COL["servo"])
    add("axle", d.axle_dummy(), COL["axle"])
    for n, o in d.sensor_dummies().items(): add(n, o, COL["sensor"])
    add("carousel", d.rot_z(up(P["carousel"]), car_ang), COL["carousel"])
    keys = list(d.PILLS)
    for i in range(n_mod):
        a = car_ang + i * d.SLOT_ANG
        add("module%d" % i, d.rot_z(up(P["module_body"]), a), COL["module"])
        ra = roll_ang if i == 0 else 0.0
        kind = keys[i % len(keys)] if pill is None else pill
        add("roller%d" % i, d.rot_z(up(d.roller_in_module(kind, ra)), a), COL["roller"])
        add("gate%d" % i, d.rot_z(up(d.gate_in_module(kind)), a), COL["gate"])
        if lids:
            add("lid%d" % i, d.rot_z(up(P["module_lid"], d.Z_PLATE_TOP + d.HOPPER_TOP), a), COL["lid"])
    return items

def shoot(items, path, **kw):
    ms = []
    for name, obj, col in items:
        try:
            V, T = mesh_of(obj, 0.3, 0.4)
            if len(T): ms.append((V, T, col))
        except Exception as e:
            print("skip", name, e)
    render(ms, path, **kw)

if __name__ == "__main__":
    P = build_parts(); log("built")
    which = sys.argv[1:] or ["iso", "sec", "top", "mod"]
    if "iso" in which:
        shoot(assembly(P, pill=None), "/tmp/v_iso.png", az=-35, el=28); log("iso")
    if "sec" in which:
        cutb = d.box(-300, 300, 0, 300, -50, 300)
        shoot(assembly(P, cut=cutb, n_mod=6), "/tmp/v_sec.png", az=90, el=0, size=(1500, 900)); log("sec")
    if "top" in which:
        its = [(n, o, c) for n, o, c in assembly(P, n_mod=0) if n not in ("carousel", "axle")]
        shoot(its, "/tmp/v_top.png", az=-60, el=50); log("top")
    if "mod" in which:
        cutb = d.box(-300, 300, 0, 300, -50, 300)
        m = P["module_body"].cut(cutb); r = d.roller_in_module("XL_soft").cut(cutb); g = d.gate_in_module("XL_soft").cut(cutb)
        shoot([("m", m, COL["module"]), ("r", r, COL["roller"]), ("g", g, COL["gate"])], "/tmp/v_mod.png", az=70, el=15, size=(1000, 800))
        cutb2 = d.box(78, 300, -300, 300, -50, 300)
        m = P["module_body"].cut(cutb2); r = d.roller_in_module("XL_soft", 0).cut(cutb2); g = d.gate_in_module("XL_soft").cut(cutb2)
        shoot([("m", m, COL["module"]), ("r", r, COL["roller"]), ("g", g, COL["gate"])], "/tmp/v_mod2.png", az=0, el=8, size=(1000, 800)); log("mod")
