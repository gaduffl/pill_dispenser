"""
Modular carousel pill dispenser - parametric CadQuery model.

Coordinate system (assembly): Z up, carousel axis = Z through the origin,
the dispensing station is on the +X axis. All dimensions in mm.

Concept
-------
* 10 passive wedge-shaped modules sit on a rotating carousel plate.
* Each module = hopper (~100 ml) + a horizontal, slightly conical metering
  roller with ONE pill-sized pocket + a gate that keeps pills one layer deep
  in front of the pocket. Roller and gate are the only pill-specific parts.
* The base holds everything active: a 28BYJ-48 stepper indexes the carousel,
  one micro servo at the station drives whichever roller is parked in front
  of it through a slide-through dog coupling, a V-chute with a light barrier
  counts every pill on its way into the cup. A second light barrier and a
  flag under the carousel hub give the home position.
"""
import math
import cadquery as cq

# --------------------------------------------------------------------------
# Parameters
# --------------------------------------------------------------------------
N_SLOTS = 10
SLOT_ANG = 360.0 / N_SLOTS
HALF = math.radians(SLOT_ANG / 2)

# module envelope
R_IN = 38.0            # inner (flat) face of a module
R_OUT = 106.0          # outer (arc) face of a module
MOD_GAP = 1.0          # gap between neighbouring modules
WALL = 2.0             # hopper wall
HOPPER_TOP = 97.0      # module height (set so that the hopper holds >= 100 ml)
FILL_MARGIN = 4.0      # head-room that is not counted as storage

# metering roller
ZA = 24.0              # roller axis above the module bottom
ROLLER_R = 20.0
BORE_R = 20.5
X_BORE0 = 60.0         # blind end of the bore
X_BODY0 = 60.5         # pocket zone of the roller
X_BODY1 = 95.0
X_KEEP1 = 97.7         # keeper groove 95.0 .. 97.7
KEEP_R = 17.2
X_TOOTH0 = 100.5       # drive tooth 100.5 .. 105.5
X_TOOTH1 = 105.5
TOOTH_FLAT = 2.75      # drive flat sits this far above the roller axis
WEB_SPH = 100.6        # outer end of the roller stays inside this sphere (see roller())
TAPER_DEG = 8.0        # the roller is a cone, small end inside: pills on it drift towards the pocket
TAPER = math.tan(math.radians(TAPER_DEG))
# Feed side. All of this is measured in the "pocket frame": x along the roller from its inner end,
# t across the pocket (positive = against the turning direction), r out of the pocket.
PARK_DEG = 15.0        # the parked pocket sits 15 deg past the top, on the side the roller turns to
T_LEAD = 6.65          # leading pocket wall, the same on every roller
T_DEAD = T_LEAD + 0.8  # dead-end wall of the feed gap, just beyond the leading pocket wall
T_FLANK = 6.95         # the feed flank ends here = trailing edge of the widest pocket
T_ROOF = 7.5           # front edge of the roof over the pocket
D_BODY = 17.0          # roof height above the roller; every pill size brings its own gate that hangs below it
FLANK_DEG = 25.0       # feed flank, relative to the roof (= 40 deg to the horizontal)
BEAM_DEG = 60.0        # upper side of the roof beam (= 45 deg to the horizontal)
BEAM_NOSE = 2.0
EAR_W, EAR_H = 1.6, 2.4   # ledges that carry the outer end of the gate
STROKE_DEG = 145.0     # roller rotation for one pill
ROCK_DEG = 22.0        # short forward turn that lines pills up against the dead-end wall
X_THR0, X_THR1 = 61.2, 94.2   # feed gap along the roller (inside the roller body 60.5 .. 95)
OUT_ANG = 55.0         # outlet opening = +-55 deg around the bottom of the bore
PIN_X = 96.35          # keeper pin (a piece of 1.75 mm filament)
PIN_Z = ZA - 18.4
PIN_D = 2.3

# passage groove in the module face (the drive prongs travel through it)
GROOVE_R = 100.1
GROOVE_LO = 3.0
GROOVE_HI = 3.75

# module <-> carousel locking studs (on the module underside)
STUDS = [(46.0, 0.0), (88.0, 22.8), (88.0, -22.8)]
STUD_SLIDE = 7.0
STUD_STEM_D = 5.0
STUD_HEAD_D = 8.0
STUD_STEM_L = 3.9      # plate thickness + play
BUMP_X = 102.0

# carousel
PLATE_T = 3.0
PLATE_R = 106.0
HUB_H = 29.0           # hub length measured from the plate top
RIB_H = 6.0
GEAR_M = 1.25
GEAR_Z = 60
PINION_Z = 15
GEAR_C = GEAR_M * (GEAR_Z + PINION_Z) / 2 + 0.2
HOME_R = 34.0          # home flag: a short arc wall under the hub
HOME_B = 24.0          # the home light barrier looks along a chord this far from the axis
HOME_ROT = -110.0      # direction of that chord in the base
HOME_FLAG_ANG = HOME_ROT + math.degrees(math.atan2(HOME_B, math.sqrt(HOME_R ** 2 - HOME_B ** 2)))
WIN_X0, WIN_X1 = 59.0, 96.5      # drop window in the plate (follows the outlet of the module)

# stack heights (absolute Z)
Z_PLATE_BOT = 77.0
Z_PLATE_TOP = Z_PLATE_BOT + PLATE_T
Z_AXIS = Z_PLATE_TOP + ZA          # roller / servo axis = 101
FLOOR_T = 2.4

# base tub
TUB_R = 104.0
TUB_WALL = 2.2
TUB_TOP = Z_PLATE_BOT - 1.5
FLAT_X = 96.0                      # flat front face of the tub
FLAT_TOP = Z_PLATE_BOT - 7.5
BAY_HW = 24.2                      # cup bay half width
MOTOR_M = Z_PLATE_BOT - 28.5       # mounting face of the 28BYJ-48
PILLAR_TOP = Z_PLATE_TOP - HUB_H   # inner race seat
AXLE_HEAD_Z = 11.0                 # depth of the hex pocket for the M8 x 80 bolt head

# chute
CH_X0, CH_X1 = 56.2, 84.2
CH_HW = 26.1                       # 0.5 mm play to the fences in the tub
CH_Z0 = 27.3
CH_Z1 = Z_PLATE_BOT - 7.0
CH_SLOPE = 40.0
CH_VHW = 19.0
CH_VTOP = CH_Z1 - CH_VHW          # valley line at the upper end
BEAM_X = 77.5

# station tower / servo (MG90S class micro servo)
TOWER_R = PLATE_R + 1.2            # clearance arc towards the carousel
SLAB_X0 = FLAT_X + 0.2
SLAB_X1 = 112.0
TOWER_HW = 40.0
TOWER_TOP = Z_AXIS + 16.0
X_M = 123.5                        # servo ear plane
NOSE_X1 = 144.0
SERVO_YC = 5.2                     # body centre is offset from the output shaft
SERVO_L, SERVO_W = 23.4, 12.8      # pocket for the 22.8 x 12.2 body
SERVO_EAR_L = 33.4
DOG_R = 11.0
PRONG_TIP = 101.4
PRONG_LO, PRONG_HI = -1.65, 1.95   # prong thickness, relative to the axis
PRONG_Y0, PRONG_Y1 = 2.3, 10.8

# cup
CUP_X0, CUP_L, CUP_H = 79.2, 50.0, 24.0
CUP_HW = 23.8

# --------------------------------------------------------------------------
# Pill presets  (L = longest dimension, W = width, T = thickness)
# --------------------------------------------------------------------------
PILLS = {
    #  name          shape      L     W     T
    "S_tab6":   ("round",    6.0,  6.0,  3.0),   # small round tablet  ~6 x 3
    "M_tab10":  ("round",   10.0, 10.0,  5.0),   # round tablet       ~10 x 5
    "L_cap0":   ("oblong",  21.7,  7.7,  7.7),   # capsule size 0 / oblong tablets
    "L_cap00":  ("oblong",  23.5,  8.6,  8.6),   # capsule size 00
    "XL_soft":  ("oblong",  30.0, 12.5, 12.5),   # big softgels / fish oil, up to 30 x 12.5
}


def roller_r(x):
    """roller radius at module coordinate x (cone between X_BODY0 and X_BODY1)"""
    return ROLLER_R - max(0.0, X_BODY1 - x) * TAPER


def bore_r(x):
    return roller_r(x) + (BORE_R - ROLLER_R)


# --------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------
def wedge_pts(r0, r1, inset):
    """corner points of a wedge whose sides are offset inwards by `inset`"""
    t = math.tan(HALF)
    c = inset / math.cos(HALF)
    y0 = r0 * t - c
    a, b, cc = 1 + t * t, -2 * t * c, c * c - r1 * r1
    x1 = (-b + math.sqrt(b * b - 4 * a * cc)) / (2 * a)
    return (r0, y0), (x1, x1 * t - c)


def wedge(r0, r1, inset, z0, z1):
    (xa, ya), (xb, yb) = wedge_pts(r0, r1, inset)
    return (cq.Workplane("XY").workplane(offset=z0)
            .moveTo(xa, -ya).lineTo(xb, -yb)
            .threePointArc((r1, 0), (xb, yb))
            .lineTo(xa, ya).close().extrude(z1 - z0))


def half_space(p0, axis, ang, L=500.0):
    """solid below a plane through p0; plane normal = +Z rotated about `axis`"""
    b = cq.Workplane("XY").box(L, L, L, centered=(True, True, False)).translate((0, 0, -L))
    return b.rotate((0, 0, 0), axis, ang).translate(p0)


def half_space_n(p0, n, L=500.0):
    """solid below the plane through p0 with (upward) normal n"""
    nx, ny, nz = n
    ln = math.sqrt(nx * nx + ny * ny + nz * nz)
    b = cq.Workplane("XY").box(L, L, L, centered=(True, True, False)).translate((0, 0, -L))
    ang = math.degrees(math.acos(nz / ln))
    if abs(ang) > 1e-9:
        b = b.rotate((0, 0, 0), (-ny, nx, 0), ang)
    return b.translate(p0)


def prism_xz(pts, y0, y1):
    """polygon given as (x, z) points, extruded from y0 to y1"""
    return (cq.Workplane("XY").polyline(pts).close().extrude(y1 - y0)
            .rotate((0, 0, 0), (1, 0, 0), 90).translate((0, y1, 0)))


def prism_yz(pts, x0, x1):
    """polygon given as (y, z) points, extruded from x0 to x1"""
    return cq.Workplane("YZ").workplane(offset=x0).polyline(pts).close().extrude(x1 - x0)


def from_pocket_frame(obj, stroke=0.0):
    """pocket frame -> module coordinates (optionally turned on by `stroke` degrees)"""
    return obj.rotate((0, 0, 0), (1, 0, 0), PARK_DEG + stroke).translate((X_BODY0, 0, ZA))


def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))


def cyl_z(x, y, r, z0, z1):
    return cq.Workplane("XY").workplane(offset=z0).center(x, y).circle(r).extrude(z1 - z0)


def cyl_x(y, z, r, x0, x1):
    return cq.Workplane("YZ").workplane(offset=x0).center(y, z).circle(r).extrude(x1 - x0)


def cyl_y(x, z, r, y0, y1):
    # XZ workplane normal is -Y, local (x, y) = (X, Z)
    return cq.Workplane("XZ").workplane(offset=-y1).center(x, z).circle(r).extrude(y1 - y0)


def rot_z(obj, ang):
    return obj.rotate((0, 0, 0), (0, 0, 1), ang)


def gear_profile(m, z, pa_deg=20.0, backlash=0.12, clearance=0.25, n=7, phase=0.0):
    pa = math.radians(pa_deg)
    rp = m * z / 2
    rb = rp * math.cos(pa)
    ra = rp + m
    rf = rp - (1 + clearance) * m
    inv = lambda a: math.tan(a) - a
    half_p = (m * math.pi / 2 - backlash) / (2 * rp)
    r0 = max(rb, rf)
    rs = [r0 + (ra - r0) * k / (n - 1) for k in range(n)]
    th = [half_p + inv(pa) - inv(math.acos(min(1.0, rb / r))) for r in rs]
    pts = []
    for i in range(z):
        c = phase + 2 * math.pi * i / z
        pol = [(rf, c - math.pi / z), (rf, c - th[0])]
        pol += [(r, c - t) for r, t in zip(rs, th)]
        pol += [(r, c + t) for r, t in zip(reversed(rs), reversed(th))]
        pol += [(rf, c + th[0])]
        for r, a in pol:
            p = (r * math.cos(a), r * math.sin(a))
            if not pts or math.hypot(p[0] - pts[-1][0], p[1] - pts[-1][1]) > 1e-4:
                pts.append(p)
    return pts


# --------------------------------------------------------------------------
# module body (hopper + roller housing)
# --------------------------------------------------------------------------
def roller_rl(xl):
    """roller radius at local position xl (0 = inner end)"""
    return ROLLER_R - max(0.0, (X_BODY1 - X_BODY0) - xl) * TAPER


R0 = roller_rl(0.0)
XA, XB = X_THR0 - X_BODY0, X_THR1 - X_BODY0       # feed gap in the pocket frame


def roof_z(xl):
    """roof over the pocket (a plane that rises with the cone)"""
    return R0 + D_BODY + TAPER * xl


def hopper_cavity(top):
    """Open volume of the hopper. Seen along the roller it is a lopsided funnel: a roof beam covers
    the roller, pills run down the flank on the other side and roll sideways under the roof into
    the pocket. The gap under the roof ends at a wall right behind the pocket."""
    prism = wedge(R_IN + WALL, R_OUT - WALL, MOD_GAP / 2 + WALL, ZA + 4.0, top)
    ze = lambda xl: math.sqrt(roller_rl(xl) ** 2 - T_FLANK ** 2)
    kf = (ze(XB) - ze(XA)) / (XB - XA)
    tf, tb = math.tan(math.radians(FLANK_DEG)), math.tan(math.radians(BEAM_DEG))
    flank = half_space_n((XA, T_FLANK, ze(XA)), (-kf, -tf, 1))
    beam = half_space_n((0, T_ROOF, roof_z(0) + BEAM_NOSE), (-TAPER, tb, 1)).intersect(box(-200, 200, -300, T_ROOF, -300, 400))
    cav = prism.cut(from_pocket_frame(flank)).cut(from_pocket_frame(beam))
    cav = cav.cut(half_space_n((X_THR0, 0, ZA + 13.0), (1, 0, 1)))        # inner end slope
    cav = cav.cut(half_space_n((X_THR1, 0, ZA + 17.0), (-1, 0, 1)))       # outer end slope
    gap = prism_xz([(XA, 0), (XB, 0), (XB, roof_z(XB)), (XA, roof_z(XA))], -T_DEAD, T_ROOF)
    return cav.union(from_pocket_frame(gap))


def gate_window():
    """opening for the gate: through the outer wall, plus the notch for its tongue at the inner end"""
    xo = R_OUT + 2 - X_BODY0
    w = prism_xz([(XB - 0.01, 0), (xo, 0), (xo, roof_z(xo)), (XB - 0.01, roof_z(XB))], -T_DEAD, T_ROOF)
    ze = roof_z(XB + 1.5) - EAR_H
    w = w.union(prism_xz([(XB + 1.5, ze), (xo, ze), (xo, roof_z(xo)), (XB + 1.5, roof_z(XB + 1.5))],
                         -T_DEAD - EAR_W, T_ROOF + EAR_W))
    zt = roof_z(XA - 2.0) - 1.3
    w = w.union(prism_xz([(XA - 2.0, zt), (XA + 0.01, zt), (XA + 0.01, roof_z(XA)), (XA - 2.0, roof_z(XA - 2.0))],
                         -T_DEAD, T_ROOF - 1.5))
    return from_pocket_frame(w)


def module_body():
    body = wedge(R_IN, R_OUT, MOD_GAP / 2, 0, HOPPER_TOP)
    body = body.cut(hopper_cavity(HOPPER_TOP + 1))
    # conical roller bore, open at the outer face
    bore = (cq.Workplane("XY").polyline([(X_BORE0, 0), (X_BORE0, bore_r(X_BORE0)), (X_BODY1, BORE_R),
                                         (R_OUT + 2, BORE_R), (R_OUT + 2, 0)]).close()
            .revolve(360, (0, 0, 0), (1, 0, 0)).translate((0, 0, ZA)))
    body = body.cut(bore)
    body = body.cut(gate_window())
    # outlet under the roller: +-OUT_ANG around the bottom of the bore
    so = math.sin(math.radians(OUT_ANG))
    h0, h1 = bore_r(X_BODY0) * so, BORE_R * so
    body = body.cut(cq.Workplane("XY").workplane(offset=-1).polyline(
        [(X_BODY0, -h0), (X_BODY1, -h1), (X_BODY1, h1), (X_BODY0, h0)]).close().extrude(ZA + 1))
    # passage groove for the drive prongs (lower side chamfered: prints upside down)
    prof = [(GROOVE_R, ZA + GROOVE_HI), (R_OUT + 2, ZA + GROOVE_HI),
            (R_OUT + 2, ZA - GROOVE_LO - (R_OUT + 2 - GROOVE_R)), (GROOVE_R, ZA - GROOVE_LO)]
    groove = cq.Workplane("XZ").polyline(prof).close().revolve(360, (0, 0, 0), (0, 1, 0))
    body = body.cut(groove)
    # keeper pin hole, straight through under the roller
    body = body.cut(cyl_y(PIN_X, PIN_Z, PIN_D / 2, -60, 60))
    # locking studs + detent bump on the underside
    for sx, sy in STUDS:
        stud = cyl_z(sx, sy, STUD_STEM_D / 2, -STUD_STEM_L, 0.5)
        cone = (cq.Workplane("XY").workplane(offset=-STUD_STEM_L).center(sx, sy)
                .circle(STUD_STEM_D / 2).workplane(offset=-2.0).circle(STUD_HEAD_D / 2).loft(combine=True))
        head = cyl_z(sx, sy, STUD_HEAD_D / 2, -STUD_STEM_L - 3.0, -STUD_STEM_L - 2.0)
        body = body.union(stud).union(cone).union(head)
    bump = cq.Workplane("XY").sphere(2.2).translate((BUMP_X, 0, 1.6)).intersect(box(90, 110, -5, 5, -0.6, 0.2))
    body = body.union(bump)
    return body


def module_lid():
    lid = wedge(R_IN, R_OUT, MOD_GAP / 2, 0, 2.0)
    lid = lid.union(box(R_IN - 6, R_IN + 1, -7, 7, 0, 2.0).edges("|Z").fillet(2.0))   # finger tab
    lip = wedge(R_IN + WALL + 0.35, R_OUT - WALL - 0.35, MOD_GAP / 2 + WALL + 0.35, -4.0, 0.01)
    lip = lip.cut(wedge(R_IN + WALL + 1.95, R_OUT - WALL - 1.95, MOD_GAP / 2 + WALL + 1.95, -5, 1))
    return lid.union(lip)


# --------------------------------------------------------------------------
# metering roller (built along +X, axis on y=0 z=0, inner end at x=0)
# --------------------------------------------------------------------------
POCKET_X0 = 1.2        # pocket starts this far from the inner end of the roller
CHAMFER_DEG = 30.0     # relief on the trailing pocket edge: lifts a pill that is only half in


def pill_dims(pill):
    return PILLS[pill] if isinstance(pill, str) else pill


def pocket_dims(shape, L, W, T):
    """pocket length along the roller, width across, depth, and the gap left under the gate"""
    if shape == "round":
        return L + 1.2, L + 1.2, T + 0.5, T + 1.4       # a round tablet lies flat in a round recess
    return L + 1.8, W + 0.8, T + 0.7, 1.04 * T + 1.5


def pocket_geo(pill):
    """Pocket geometry in its own "generator frame": x along the cone surface, y across (0 = pocket centre),
    z = height above the surface line. `ang` turns that frame about the roller axis so that the leading
    pocket edge of every roller ends up at the same place, right in front of the dead-end wall."""
    shape, L, W, T = pill_dims(pill)
    lp, wp, depth, gap = pocket_dims(shape, L, W, T)
    x0 = POCKET_X0
    rmin, rmid = roller_rl(x0), roller_rl(x0 + lp / 2)
    sag = lambda r, t: r - math.sqrt(r * r - t * t)
    floor = -depth - (sag(rmin, wp / 2) if shape == "round" else 0.0)   # a flat tablet must clear the surface at its rim
    cham = min(0.35 * depth, 3.0)
    ang = math.degrees(math.asin(T_LEAD / rmid) - math.asin(wp / 2 / rmid))
    return dict(shape=shape, L=L, W=W, T=T, lp=lp, wp=wp, depth=depth, gap=gap, x0=x0, floor=floor, ang=ang,
                cham=cham, zc=-sag(rmid, wp / 2) - cham, top_clear=-(floor + T + 0.1),
                can_stand=(shape != "round" and L < gap + depth + 1.5))


def from_gen_frame(obj):
    """generator frame -> roller frame (axis = x, pocket still pointing +z)"""
    return obj.rotate((0, 0, 0), (0, 1, 0), -TAPER_DEG).translate((0, 0, R0))


def pill_in_pocket(pill, solid, stroke=0.0, lift=0.0):
    """Put a pill solid (long axis = X, thickness = Z, centred on the origin) into the pocket and turn it
    with the roller by `stroke` degrees; `lift` raises it out of the pocket. Module coordinates."""
    g = pocket_geo(pill)
    s = solid.translate((g["x0"] + g["lp"] / 2, 0, g["floor"] + g["T"] / 2 + 0.1 + lift))
    return from_pocket_frame(from_gen_frame(s), stroke + g["ang"])


def roller(pill="XL_soft"):
    shape, L, W, T = PILLS[pill] if isinstance(pill, str) else pill
    lp, wp, depth, gap = pocket_dims(shape, L, W, T)
    body_len = X_BODY1 - X_BODY0
    x0 = POCKET_X0
    assert lp <= body_len - 2.6, "pill too long for this roller"
    assert wp <= 2 * T_LEAD + 0.01, "pill too wide for this roller"
    assert gap <= D_BODY - 1.6 and depth <= 13.5, "pill too thick for this roller"
    xk0 = body_len                       # keeper groove start
    xk1 = X_KEEP1 - X_BODY0              # keeper groove end
    xt1 = X_TOOTH1 - X_BODY0
    # Outer end: everything beyond the keeper groove stays inside a sphere around the
    # carousel axis (radius WEB_SPH), so the fixed drive prongs can never touch a
    # roller that is merely passing the station.
    dome = lambda rho: math.sqrt(WEB_SPH ** 2 - rho ** 2) - X_BODY0
    rw = KEEP_R
    while xk1 + (rw + 0.05 - KEEP_R) < dome(rw + 0.05):       # 45 deg cone up to the dome
        rw += 0.05
    prof = [(0, 0), (0, roller_rl(0) - 0.6), (0.6, roller_rl(0.6)), (xk0, ROLLER_R), (xk0, KEEP_R), (xk1, KEEP_R)]
    n = 10
    prof += [(dome(rw * (n - k) / n), rw * (n - k) / n) for k in range(n + 1)]
    core = cq.Workplane("XY").polyline(prof).close().revolve(360, (0, 0, 0), (1, 0, 0))
    # drive tooth: segment of a disc above the flat, chamfered rim
    tp = [(xk1 + 0.6, 0), (xk1 + 0.6, rw), (xt1 - 1.2, rw), (xt1, rw - 1.2), (xt1, 0)]
    tooth = (cq.Workplane("XY").polyline(tp).close().revolve(360, (0, 0, 0), (1, 0, 0))
             .intersect(box(0, 60, -30, 30, TOOTH_FLAT, 30)))
    # flared lead-in at both ends of the flat
    for s in (1, -1):
        tooth = tooth.cut(half_space((0, s * 11.5, TOOTH_FLAT), (1, 0, 0), s * 28).intersect(
            box(xk1, xt1 + 1, min(0, s * 30), max(0, s * 30), 0, 30)))
    r = core.union(tooth)
    # Pocket: the pill lies in it, flush with the leading wall. The trailing edge is relieved, so a pill
    # that is only half in is lifted back out by the turning roller instead of being pinched.
    g = pocket_geo(pill)
    wp_ = cq.Workplane("XY").workplane(offset=g["floor"]).center(g["x0"] + g["lp"] / 2, 0)
    pk = (wp_.circle(g["wp"] / 2) if shape == "round" else wp_.slot2D(g["lp"], g["wp"])).extrude(30)
    tc_, tw = math.tan(math.radians(CHAMFER_DEG)), g["wp"] / 2
    pk = pk.union(prism_yz([(tw - 0.3, g["zc"]), (tw + 14, g["zc"] + 14.3 * tc_), (tw + 14, 20), (tw - 0.3, 20)],
                           g["x0"] + 0.6, g["x0"] + g["lp"] - 0.6))
    r = r.cut(from_gen_frame(pk).rotate((0, 0, 0), (1, 0, 0), PARK_DEG + g["ang"]))
    # index mark on the outer face: "this side up"
    r = r.cut(box(xt1 - 0.5, xt1 + 1, -0.7, 0.7, 10.5, 15.5))
    return r


def roller_in_module(pill="XL_soft", stroke=0.0):
    """roller in module coordinates; stroke = degrees turned from the parked position towards the outlet"""
    return roller(pill).rotate((0, 0, 0), (1, 0, 0), stroke).translate((X_BODY0, 0, ZA))


def gate(pill="XL_soft"):
    """The gate belongs to a roller. It hangs under the roof and leaves a gap that is only a little
    higher than one pill, so nothing can stand upright in the pocket and pills arrive one layer deep.
    Its tab sits in the keeper groove of the roller: both go in and come out together. Pocket frame."""
    g = pocket_geo(pill)
    top = lambda x: roof_z(x) - 0.2
    y0, y1 = -T_DEAD + 0.2, T_ROOF - 0.2
    xp1 = R_OUT - X_BODY0
    plate = prism_xz([(XA + 0.3, 0), (XB + 0.3, 0), (XB + 0.3, top(XB + 0.3)), (XA + 0.3, top(XA + 0.3))], y0, y1)
    # underside: the same gap everywhere above the roller
    plate = plate.cut(cq.Workplane("XY").polyline([(0, 0), (0, R0 + g["gap"]), (40, R0 + g["gap"] + 40 * TAPER), (40, 0)])
                      .close().revolve(360, (0, 0, 0), (1, 0, 0)))
    plug = prism_xz([(XB + 0.25, 0), (xp1, 0), (xp1, top(xp1)), (XB + 0.25, top(XB + 0.25))], y0, y1)
    ze = roof_z(XB + 1.5) - EAR_H + 0.15
    ears = prism_xz([(XB + 1.75, ze), (xp1, ze), (xp1, top(xp1)), (XB + 1.75, top(XB + 1.75))], y0 - EAR_W, y1 + EAR_W)
    zt = roof_z(XA - 2.0) - 1.3 + 0.15
    tongue = prism_xz([(XA - 1.8, zt), (XA + 0.4, zt), (XA + 0.4, top(XA + 0.4)), (XA - 1.8, top(XA - 1.8))],
                      y0, T_ROOF - 1.7)
    xk0, xk1 = X_BODY1 - X_BODY0 + 0.25, X_KEEP1 - X_BODY0 - 0.25
    tab = box(xk0, xk1, y0, y1, 0, 30).intersect(cyl_x(0, 0, BORE_R + 0.2, xk0 - 1, xk1 + 1))
    gt = plate.union(plug).union(ears).cut(cyl_x(0, 0, BORE_R, 0, 60)).union(tab).union(tongue)
    gt = gt.cut(cyl_x(0, 0, KEEP_R + 0.4, 0, 60))
    # stay inside the outer face of the module
    face = cyl_z(-X_BODY0, 0, R_OUT - 0.4, -100, 100).rotate((0, 0, 0), (1, 0, 0), -PARK_DEG)
    return gt.intersect(face)


def gate_in_module(pill="XL_soft"):
    return from_pocket_frame(gate(pill))


# --------------------------------------------------------------------------
# carousel (plate + hub with bearings + ring gear), plate top at z = 0
# --------------------------------------------------------------------------
def home_flag():
    """home flag: a short arc wall under the hub; once per turn it passes through the home light barrier"""
    flag = (cyl_z(0, 0, HOME_R + 0.8, -HUB_H - 13.0, -HUB_H + 0.5).cut(cyl_z(0, 0, HOME_R - 0.8, -HUB_H - 14, 0))
            .intersect(box(0, 60, -4.2, 4.2, -HUB_H - 14, 0)))
    return rot_z(flag, HOME_FLAG_ANG)


def keyhole(sx, sy):
    k = cyl_z(sx + STUD_SLIDE, sy, STUD_HEAD_D / 2 + 0.45, -10, 1)
    k = k.union(cyl_z(sx, sy, STUD_STEM_D / 2 + 0.3, -10, 1))
    k = k.union(box(sx, sx + STUD_SLIDE, sy - STUD_STEM_D / 2 - 0.3, sy + STUD_STEM_D / 2 + 0.3, -10, 1))
    return k


def carousel():
    c = cq.Workplane("XY").circle(PLATE_R).extrude(-PLATE_T)
    gear = cq.Workplane("XY").polyline(gear_profile(GEAR_M, GEAR_Z)).close().extrude(-HUB_H)
    c = c.union(gear)
    for i in range(N_SLOTS):
        a = (i + 0.5) * SLOT_ANG
        c = c.union(rot_z(box(36.5, 100.0, -1.2, 1.2, -PLATE_T - RIB_H, -PLATE_T + 0.01), a))
    c = c.union(home_flag())
    # bearing seats (2 x 608), M8 axle
    c = c.cut(cyl_z(0, 0, 11.1, -7.0, 1))
    cone = (cq.Workplane("XY").workplane(offset=-7.0).circle(11.1)
            .workplane(offset=-2.6).circle(8.5).loft(combine=True))
    c = c.cut(cone)
    c = c.cut(cyl_z(0, 0, 8.5, -HUB_H - 1, 0))
    c = c.cut(cyl_z(0, 0, 11.1, -HUB_H - 1, -HUB_H + 7.0))
    # lightening pockets in the hub (open towards the print top)
    ring = cyl_z(0, 0, 31.0, -HUB_H - 1, -PLATE_T - 4.0).cut(cyl_z(0, 0, 15.0, -HUB_H - 2, 0))
    for i in range(6):
        ring = ring.cut(rot_z(box(10, 35, -1.5, 1.5, -HUB_H - 2, 0), i * 60))
    c = c.cut(ring)
    # per-slot features
    so = math.sin(math.radians(OUT_ANG))
    w0, w1 = bore_r(WIN_X0) * so + 0.5, BORE_R * so + 0.5
    cutter = cq.Workplane("XY").workplane(offset=-10).polyline(
        [(WIN_X0, -w0), (X_BODY1, -w1), (WIN_X1, -w1), (WIN_X1, w1), (X_BODY1, w1), (WIN_X0, w0)]).close().extrude(11)
    for sx, sy in STUDS:
        cutter = cutter.union(keyhole(sx, sy))
    cutter = cutter.union(cyl_z(BUMP_X, 0, 1.9, -10, 1))
    for i in range(N_SLOTS):
        c = c.cut(rot_z(cutter, i * SLOT_ANG))
    for i in range(N_SLOTS):
        try:
            txt = (cq.Workplane("XY").workplane(offset=-0.5).center(27.5, 0)
                   .text(str(i + 1), 8.0, 0.6, halign="center", valign="center").rotate((27.5, 0, 0), (27.5, 0, 1), -90))
        except Exception:
            txt = None
            for k in range(i + 1):
                dot = cyl_z(22 + 1.8 * k, 0, 0.6, -0.5, 0.1)
                txt = dot if txt is None else txt.union(dot)
        c = c.cut(rot_z(txt, i * SLOT_ANG))
    return c


def pinion():
    p = cq.Workplane("XY").polyline(
        gear_profile(GEAR_M, PINION_Z, phase=math.pi / PINION_Z)).close().extrude(10.0)
    # 28BYJ-48 shaft: d5 with 3 mm flats, blind from below, 1 mm cap on top
    p = p.cut(cyl_z(0, 0, 2.65, -1, 2.6))
    dd = cyl_z(0, 0, 2.65, 2.59, 9.0).intersect(box(-3, 3, -1.62, 1.62, 2, 9.5))
    return p.cut(dd)


# --------------------------------------------------------------------------
# base tub
# --------------------------------------------------------------------------
def tub():
    full = box(-200, FLAT_X, -200, 200, -1, 200)
    t = cyl_z(0, 0, TUB_R, 0, TUB_TOP).intersect(full)
    inner = cyl_z(0, 0, TUB_R - TUB_WALL, FLOOR_T, TUB_TOP + 1).intersect(
        box(-200, FLAT_X - TUB_WALL, -200, 200, -1, 200))
    t = t.cut(inner)
    # flat front wall is lower (carousel ribs pass above) and open for the cup bay
    t = t.cut(box(FLAT_X - TUB_WALL - 0.01, FLAT_X + 1, -TOWER_HW, TOWER_HW, FLAT_TOP, 200))
    t = t.cut(box(FLAT_X - TUB_WALL - 1, FLAT_X + 1, -BAY_HW, BAY_HW, FLOOR_T, 200))
    # central axle pillar (M8 bolt from below, hex head captured)
    t = t.union(cyl_z(0, 0, 13, 0, PILLAR_TOP - 0.6)).union(cyl_z(0, 0, 5.6, 0, PILLAR_TOP))
    t = t.cut(cyl_z(0, 0, 4.2, -1, 200))
    t = t.cut(cq.Workplane("XY").polygon(6, 13.5 / math.cos(math.pi / 6)).extrude(AXLE_HEAD_Z))
    # stepper posts (28BYJ-48 hangs between them, shaft up)
    mx = -(GEAR_C + 8.0)
    for s in (1, -1):
        post = box(mx - 5, mx + 5, s * 17.5 - 5, s * 17.5 + 5, 0, MOTOR_M - 1.0)
        post = post.union(box(mx - 5, mx + 5, min(s * 12.5, s * 26), max(s * 12.5, s * 26), 0, 14))
        t = t.union(post)
    t = t.cut(cyl_z(mx, 0, 14.6, FLOOR_T, 200))
    for s in (1, -1):
        t = t.cut(cyl_z(mx, s * 17.5, 1.4, MOTOR_M - 14, 200))
    # station: cup bay walls, chute seat, fences
    for s in (1, -1):
        y0, y1 = sorted((s * BAY_HW, s * (BAY_HW + 2.4)))
        t = t.union(box(CH_X0 - 0.2, FLAT_X - TUB_WALL + 0.01, y0, y1, 0, CH_Z0))
        t = t.union(box(CH_X1 + 0.4, FLAT_X - TUB_WALL + 0.01, y0, y1, 0, Z_PLATE_BOT - 8.0))
        f0, f1 = sorted((s * (BAY_HW + 2.4), s * (BAY_HW + 4.4)))
        t = t.union(box(CH_X0 - 2.8, FLAT_X - TUB_WALL + 0.01, f0, f1, 0, CH_Z0 + 3.7))
    t = t.union(box(CH_X0 - 2.8, CH_X0 - 0.4, -BAY_HW - 4.4, BAY_HW + 4.4, 0, CH_Z0 + 3.7))
    t = t.union(box(CUP_X0 - 2.6, CUP_X0 - 0.2, -BAY_HW, BAY_HW, 0, CH_Z0))
    # Home light barrier: cradles for the emitter and receiver housings (20 x 10 x 8 mm, standing on
    # their 8 mm side). The beam runs along a chord and is cut by the flag under the carousel hub.
    a1 = math.sqrt(HOME_R ** 2 - HOME_B ** 2)
    zb = PILLAR_TOP - 6.5
    for a0 in (a1 - 12.0 - 20.5, a1 + 7.0):               # receiver inside the flag circle, emitter outside
        cr = box(a0, a0 + 20.5, HOME_B - 5.8, HOME_B + 5.8, 0, zb + 3.0)
        cr = cr.cut(box(a0 - 1, a0 + 22, HOME_B - 4.3, HOME_B + 4.3, zb - 5.2, 200))
        t = t.union(rot_z(cr, HOME_ROT))
    # tower screws: vertical slots through the flat wall (height adjustment)
    for y in (33.0, -33.0):
        for z in (14.0, 50.0):
            t = t.cut(box(FLAT_X - TUB_WALL - 1, FLAT_X + 1, y - 1.7, y + 1.7, z - 4, z + 4))
    # servo cable notch
    t = t.cut(box(FLAT_X - TUB_WALL - 1, FLAT_X + 1, 27.5, 30.5, FLAT_TOP - 9, 200))
    # USB-C opening for the XIAO (breadboard sits on the floor in front of it)
    usb = box(TUB_R - 6, TUB_R + 2, -8, 8, FLOOR_T + 8.0, FLOOR_T + 20.0)
    t = t.cut(rot_z(usb, 138))
    # breadboard outline on the floor (170-point mini breadboard, 47 x 35)
    bb = box(53.0, 101.0, -18.2, 18.2, FLOOR_T - 0.01, FLOOR_T + 0.8).cut(box(54.2, 99.8, -17.0, 17.0, 0, 9))
    bb = bb.intersect(cyl_z(0, 0, TUB_R - 1, 0, 9))
    t = t.union(rot_z(bb, 138))
    # cable tie slots in the floor are left to the builder: adhesive pads work fine
    return t


# --------------------------------------------------------------------------
# chute with V-valley and IR beam ports
# --------------------------------------------------------------------------
def chute():
    c = box(CH_X0, CH_X1, -CH_HW, CH_HW, CH_Z0, CH_Z1)
    # locating lip between the bay walls
    lip = [(-BAY_HW + 0.3, CH_Z0 + 0.01), (BAY_HW - 0.3, CH_Z0 + 0.01), (BAY_HW - 2.8, CH_Z0 - 2.5), (-BAY_HW + 2.8, CH_Z0 - 2.5)]
    c = c.union(cq.Workplane("YZ").workplane(offset=CH_X0 + 0.3).polyline(lip).close().extrude(CUP_X0 - 3.2 - CH_X0))
    tn = math.tan(math.radians(CH_SLOPE))
    x0 = CH_X0 + 2.0
    # V valley: z >= z_valley(x) + |y|, valley line falling at CH_SLOPE towards the cup
    valley = box(x0, CH_X1 + 2, -CH_VHW, CH_VHW, 0, CH_Z1 + 2)
    valley = valley.cut(half_space_n((x0, 0, CH_VTOP), (tn, -1, 1)))
    valley = valley.cut(half_space_n((x0, 0, CH_VTOP), (tn, 1, 1)))
    c = c.cut(valley)
    # light barrier across the valley: a pocket on each side for a 20 x 10 x 8 mm housing, 2.2 mm aperture
    zb = CH_VTOP - (BEAM_X - x0) * tn + 1.6
    for s in (1, -1):
        y = lambda a, b: sorted((s * a, s * b))
        c = c.cut(cyl_y(BEAM_X, zb, 1.1, *y(0, 12.4)))
        c = c.cut(box(BEAM_X - 5.25, BEAM_X + 5.25, *y(12.0, CH_HW + 1), zb - 4.25, zb + 4.25))
    return c, zb


# --------------------------------------------------------------------------
# station tower with servo mount
# --------------------------------------------------------------------------
def tower():
    t = box(SLAB_X0, SLAB_X1, -TOWER_HW, TOWER_HW, 0, TOWER_TOP)
    # servo mount block + nose around the servo body
    ny0, ny1 = SERVO_YC - SERVO_EAR_L / 2 - 2.4, SERVO_YC + SERVO_EAR_L / 2 + 2.4
    nz0, nz1 = Z_AXIS - 14.0, Z_AXIS + 14.0
    t = t.union(box(SLAB_X1 - 0.01, NOSE_X1, ny0, ny1, nz0, nz1))
    # 45 deg corbel under the nose so the tower prints upright without support
    cor = (cq.Workplane("XZ").polyline([(SLAB_X1, nz0 + 0.01), (NOSE_X1, nz0 + 0.01),
                                        (SLAB_X1, nz0 - (NOSE_X1 - SLAB_X1))]).close()
           .extrude(-(ny1 - ny0)).translate((0, ny0, 0)))
    t = t.union(cor)
    # porch floor + side walls for the cup
    t = t.union(box(SLAB_X0, CUP_X0 + CUP_L + 2, -BAY_HW - 2.4, BAY_HW + 2.4, 0, FLOOR_T))
    for s in (1, -1):
        y0, y1 = sorted((s * BAY_HW, s * (BAY_HW + 2.4)))
        t = t.union(box(SLAB_X1 - 0.01, CUP_X0 + CUP_L + 2, y0, y1, 0, 14.0))
    # cup opening with gabled top (bridges only 12 mm when printed upright)
    zo = CH_Z0 + 0.2
    op = (cq.Workplane("YZ").polyline([(-BAY_HW, FLOOR_T), (BAY_HW, FLOOR_T), (BAY_HW, zo),
                                       (6, zo + BAY_HW - 6), (-6, zo + BAY_HW - 6), (-BAY_HW, zo)])
          .close().extrude(SLAB_X1 - SLAB_X0 + 2).translate((SLAB_X0 - 1, 0, 0)))
    t = t.cut(op)
    # clearance for the carousel (plate, ribs, modules)
    t = t.cut(cyl_z(0, 0, TOWER_R, FLAT_TOP - 0.2, 300))
    # servo: body cavity (open to the front), case pocket, dog bore
    t = t.cut(box(X_M, NOSE_X1 + 1, SERVO_YC - SERVO_EAR_L / 2, SERVO_YC + SERVO_EAR_L / 2,
                  Z_AXIS - SERVO_W / 2 - 0.3, Z_AXIS + SERVO_W / 2 + 0.3))
    t = t.cut(box(X_M - 5.4, X_M + 1, SERVO_YC - SERVO_L / 2, SERVO_YC + SERVO_L / 2,
                  Z_AXIS - SERVO_W / 2, Z_AXIS + SERVO_W / 2))
    t = t.cut(cyl_x(0, Z_AXIS, DOG_R + 0.75, 90, X_M - 5.3))
    for dy in (-13.9, 13.9):
        t = t.cut(cyl_x(SERVO_YC + dy, Z_AXIS, 0.85, X_M - 5.0, X_M + 1))
    # cable way from the nose down into the tub
    t = t.cut(box(SLAB_X0 - 1, SLAB_X1 + 1, 27.5, 30.5, FLAT_TOP - 9, FLAT_TOP - 0.2 + 0.0))
    # screw pilots (M3 self-tapping, from inside the tub)
    for y in (33.0, -33.0):
        for z in (14.0, 50.0):
            t = t.cut(cyl_x(y, z, 1.35, SLAB_X0 - 1, SLAB_X0 + 11))
    return t


def drive_dog():
    """servo side of the coupling: hub on the spline + two flat prongs"""
    x_hub1 = X_M - 10.2          # just clear of the servo output boss
    x_hub0 = x_hub1 - 5.4
    d = cyl_x(0, Z_AXIS, DOG_R, x_hub0, x_hub1)
    d = d.cut(cyl_x(0, Z_AXIS, 2.35, x_hub0 + 1.4, x_hub1 + 1))     # spline bore d4.7
    d = d.cut(cyl_x(0, Z_AXIS, 1.25, x_hub0 - 1, x_hub1))           # horn screw
    lo, hi = Z_AXIS + PRONG_LO, Z_AXIS + PRONG_HI
    for s in (1, -1):
        sec = [(s * PRONG_Y0, lo), (s * (PRONG_Y1 - 2.4), lo), (s * PRONG_Y1, lo + 1.2),
               (s * PRONG_Y1, hi - 1.2), (s * (PRONG_Y1 - 2.4), hi), (s * PRONG_Y0, hi)]
        p = (cq.Workplane("YZ").workplane(offset=PRONG_TIP).polyline(sec).close()
             .extrude(x_hub0 + 0.01 - PRONG_TIP))
        d = d.union(p)
    return d


def cup():
    o = box(CUP_X0, CUP_X0 + CUP_L, -CUP_HW, CUP_HW, FLOOR_T + 0.2, FLOOR_T + 0.2 + CUP_H)
    o = o.edges("|Z").fillet(6.0)
    i = box(CUP_X0 + 1.6, CUP_X0 + CUP_L - 1.6, -CUP_HW + 1.6, CUP_HW - 1.6, FLOOR_T + 1.8, 200)
    i = i.edges("|Z").fillet(4.4)
    c = o.cut(i)
    grip = box(CUP_X0 + CUP_L - 0.5, CUP_X0 + CUP_L + 7, -10, 10, FLOOR_T + 0.2, FLOOR_T + 3.2)
    return c.union(grip)


def hub_cap():
    c = cyl_z(0, 0, 20, 0, 1.6).union(cyl_z(0, 0, 9.5, -9, 0.01))
    c = c.cut(cq.Workplane("XY").workplane(offset=-10).polygon(6, 13.3 / math.cos(math.pi / 6)).extrude(9.0))
    return c


# --------------------------------------------------------------------------
# purchased parts (for the assembly preview / collision checks only)
# --------------------------------------------------------------------------
def stepper_dummy():
    mx = -(GEAR_C + 8.0)
    m = cyl_z(mx, 0, 14.0, MOTOR_M - 19, MOTOR_M)
    m = m.union(box(mx - 3.5, mx + 3.5, -21, 21, MOTOR_M - 1.0, MOTOR_M))
    m = m.union(cyl_z(mx + 8, 0, 4.5, MOTOR_M, MOTOR_M + 1.5)).union(cyl_z(mx + 8, 0, 2.5, MOTOR_M, MOTOR_M + 10))
    m = m.union(box(mx - 17, mx - 13, -7.3, 7.3, MOTOR_M - 17, MOTOR_M))
    return m


def servo_dummy():
    s = box(X_M - 5.0, X_M + 17.8, SERVO_YC - 11.4, SERVO_YC + 11.4, Z_AXIS - 6.1, Z_AXIS + 6.1)
    s = s.union(box(X_M, X_M + 2.5, SERVO_YC - 16.2, SERVO_YC + 16.2, Z_AXIS - 6.1, Z_AXIS + 6.1))
    s = s.union(cyl_x(0, Z_AXIS, 5.8, X_M - 9.9, X_M - 5.0)).union(cyl_x(0, Z_AXIS, 2.4, X_M - 14.0, X_M - 9.9))
    return s


def axle_dummy():
    hexd = 13.0 / math.cos(math.pi / 6)
    a = cyl_z(0, 0, 4.0, AXLE_HEAD_Z, AXLE_HEAD_Z + 80.0)
    a = a.union(cq.Workplane("XY").polygon(6, hexd).extrude(5.3).translate((0, 0, AXLE_HEAD_Z - 5.3)))
    a = a.union(cq.Workplane("XY").workplane(offset=Z_PLATE_TOP + 1.6).polygon(6, hexd).extrude(8.0)
                .cut(cyl_z(0, 0, 4.0, 0, 200)))
    for z in (Z_PLATE_TOP - 7.0, Z_PLATE_TOP - HUB_H):
        a = a.union(cyl_z(0, 0, 11.0, z, z + 7.0).cut(cyl_z(0, 0, 4.0, z - 1, z + 8)))
    return a


def sensor_dummies():
    """the four halves of the two light barriers (20 x 10 x 8 mm housings), where they sit in the base"""
    a1 = math.sqrt(HOME_R ** 2 - HOME_B ** 2)
    zb = PILLAR_TOP - 6.5
    out = {}
    for name, a0 in (("home_rx", a1 - 12.25 - 20.0), ("home_tx", a1 + 7.25)):
        out[name] = rot_z(box(a0, a0 + 20.0, HOME_B - 4.0, HOME_B + 4.0, zb - 5.2, zb + 4.8), HOME_ROT)
    zc = CH_VTOP - (BEAM_X - CH_X0 - 2.0) * math.tan(math.radians(CH_SLOPE)) + 1.6
    for name, sgn in (("drop_a", 1), ("drop_b", -1)):
        y0, y1 = sorted((sgn * 12.2, sgn * 32.2))
        out[name] = box(BEAM_X - 5.0, BEAM_X + 5.0, y0, y1, zc - 4.0, zc + 4.0)
    return out


def home_beam():
    """line of sight of the home light barrier (for the checks)"""
    a1 = math.sqrt(HOME_R ** 2 - HOME_B ** 2)
    return rot_z(cyl_x(HOME_B, PILLAR_TOP - 6.5, 0.5, a1 - 12.0, a1 + 7.0), HOME_ROT)
