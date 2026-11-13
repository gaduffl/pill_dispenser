"""Export every printable part as STL + STEP in its print orientation, plus an assembly STEP."""
import os, sys, json, math
import cadquery as cq
import dispenser as d
from build import build_parts, up, log

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STL, STEP = os.path.join(ROOT, "stl"), os.path.join(ROOT, "step")

def clean_stl(shape, path, tol=0.03, ang=0.12):
    """export, then weld vertices and drop zero-area triangles so slicers get a closed mesh"""
    import trimesh, numpy as np
    cq.exporters.export(shape, path, tolerance=tol, angularTolerance=ang)
    m = trimesh.load(path)
    m.merge_vertices(digits_vertex=4)
    m.update_faces(m.nondegenerate_faces())
    m.update_faces(m.unique_faces())
    m.remove_unreferenced_vertices()
    if not m.is_watertight:
        trimesh.repair.fill_holes(m)
    trimesh.repair.fix_normals(m)
    m.export(path)
    return m.is_watertight


def to_bed(o):
    bb = o.val().BoundingBox()
    return o.translate((-(bb.xmin + bb.xmax) / 2, -(bb.ymin + bb.ymax) / 2, -bb.zmin))

def print_pose(name, o):
    if name == "module_body":   return o.rotate((0, 0, 0), (1, 0, 0), 180)          # hopper rim on the bed
    if name == "module_lid":    return o.rotate((0, 0, 0), (1, 0, 0), 180)          # flat top on the bed
    if name.startswith("roller"): return o.rotate((0, 0, 0), (0, 1, 0), -90)        # axis vertical, inner end down
    if name.startswith("gate"):                                                      # flat on the face that meets the roof
        return o.rotate((0, 0, 0), (0, 1, 0), d.TAPER_DEG).rotate((0, 0, 0), (1, 0, 0), 180)
    if name == "carousel":      return o.rotate((0, 0, 0), (1, 0, 0), 180)          # plate top on the bed
    if name == "chute":         return o.rotate((0, 0, 0), (1, 0, 0), 90)           # on its side: beam ports vertical
    if name == "drive_dog":     return o.rotate((0, 0, 0), (0, 1, 0), 90)           # hub down, prongs up
    if name == "hub_cap":       return o.rotate((0, 0, 0), (1, 0, 0), 180)
    return o                                                                         # tub, tower, cup, pinion: as modelled

if __name__ == "__main__":
    os.makedirs(STL, exist_ok=True); os.makedirs(STEP, exist_ok=True)
    P = build_parts(); log("built")
    rows = []
    for name, o in P.items():
        if name.startswith("_"): continue
        q = to_bed(print_pose(name, o))
        clean_stl(q, os.path.join(STL, name + ".stl"))
        cq.exporters.export(q, os.path.join(STEP, name + ".step"))
        bb = q.val().BoundingBox()
        rows.append(dict(part=name, x=round(bb.xlen, 1), y=round(bb.ylen, 1), z=round(bb.zlen, 1),
                         solid_cm3=round(q.val().Volume() / 1000, 1)))
        log("exported", name, rows[-1])
    json.dump(rows, open(os.path.join(ROOT, "docs", "parts.json"), "w"), indent=1)
    # assembly STEP (one module shown, the others are identical)
    a = cq.Assembly(name="pill_dispenser")
    Z = d.Z_PLATE_TOP
    a.add(P["base_tub"], name="base_tub", color=cq.Color(0.36, 0.39, 0.44))
    a.add(P["tower"], name="tower", color=cq.Color(0.36, 0.39, 0.44))
    a.add(P["chute"], name="chute", color=cq.Color(0.92, 0.8, 0.35))
    a.add(P["cup"], name="cup", color=cq.Color(0.95, 0.95, 0.95))
    a.add(P["drive_dog"], name="drive_dog", color=cq.Color(0.86, 0.27, 0.27))
    a.add(P["pinion"].translate((-d.GEAR_C, 0, d.MOTOR_M + 1.5)), name="pinion", color=cq.Color(0.86, 0.27, 0.27))
    a.add(up(P["carousel"]), name="carousel", color=cq.Color(0.8, 0.81, 0.83))
    a.add(d.stepper_dummy(), name="ref_28BYJ48", color=cq.Color(0.66, 0.68, 0.24))
    a.add(d.servo_dummy(), name="ref_MG90S", color=cq.Color(0.24, 0.35, 0.78))
    a.add(d.axle_dummy(), name="ref_M8_axle_608", color=cq.Color(0.63, 0.63, 0.65))
    for n, o in d.sensor_dummies().items():
        a.add(o, name="ref_light_barrier_" + n, color=cq.Color(0.15, 0.15, 0.17))
    keys = list(d.PILLS)
    for i in range(d.N_SLOTS):
        ang = i * d.SLOT_ANG
        a.add(d.rot_z(up(P["module_body"]), ang), name="module_%d" % (i + 1), color=cq.Color(0.47, 0.67, 0.84))
        a.add(d.rot_z(up(d.roller_in_module(keys[i % len(keys)])), ang), name="roller_%d" % (i + 1), color=cq.Color(0.93, 0.59, 0.24))
        a.add(d.rot_z(up(d.gate_in_module(keys[i % len(keys)])), ang), name="gate_%d" % (i + 1), color=cq.Color(0.35, 0.65, 0.4))
        a.add(d.rot_z(up(P["module_lid"], Z + d.HOPPER_TOP), ang), name="lid_%d" % (i + 1), color=cq.Color(0.94, 0.94, 0.94))
    a.save(os.path.join(STEP, "assembly.step")); log("assembly step")
