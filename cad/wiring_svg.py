"""Draw the wiring guide: docs/img/layout.svg (where the parts sit in the base, top view) and
docs/img/wiring.svg (breadboard, hole by hole). Positions come from dispenser.py, so the drawings follow the CAD."""
import math, os
import dispenser as d

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = 'font-family="Helvetica, Arial, sans-serif"'
C5V, CGND, C3V3, CULN, CSIG, CSRV, COPT, CLINK = "#d62728", "#222222", "#f28e2b", "#2ca02c", "#1f6fd1", "#8e44ad", "#7f7f7f", "#555555"


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def text(x, y, s, size=12, anchor="start", weight="normal", fill="#222", extra=""):
    return '<text x="%.1f" y="%.1f" font-size="%d" text-anchor="%s" font-weight="%s" fill="%s" %s %s>%s</text>' % (
        x, y, size, anchor, weight, fill, FONT, extra, esc(s))


# ------------------------------------------------------------------ breadboard, hole by hole
P = 24.0                         # px per 2.54 mm
X0, Y0 = 330.0, 140.0            # hole a1
ROWX = dict(a=0, b=1, c=2, d=3, e=4, f=7, g=8, h=9, i=10, j=11)


def hole(h):                     # "12b" -> (x, y)
    col, row = int(h[:-1]), h[-1]
    return X0 + ROWX[row] * P, Y0 + (col - 1) * P


XIAO_L = ["D0", "D1", "D2", "D3", "D4", "D5", "D6"]          # row c, columns 1..7
XIAO_R = ["5V", "GND", "3V3", "D10", "D9", "D8", "D7"]       # row g, columns 1..7

# jumpers on the breadboard: (from, to, colour, dashed)
LINKS = [("1i", "9f", C5V, False), ("2i", "10g", CGND, False), ("3i", "11f", C3V3, False), ("10e", "10f", CGND, False),
         ("12b", "1a", CSIG, False), ("13b", "5a", CSIG, False), ("10j", "14f", CGND, True)]
# wires that leave the breadboard: (hole, side, colour, dashed)
OUT = [("2a", "L", CSIG, False), ("3a", "L", COPT, True), ("4a", "L", CSRV, False), ("6a", "L", COPT, True),
       ("10a", "L", CGND, False), ("10b", "L", CGND, False), ("10c", "L", CGND, False), ("10d", "L", CGND, False),
       ("12a", "L", CSIG, False), ("13a", "L", CSIG, False),
       ("4j", "R", CULN, False), ("5j", "R", CULN, False), ("6j", "R", CULN, False), ("7j", "R", CULN, False),
       ("9g", "R", C5V, False), ("9h", "R", C5V, False), ("10h", "R", CGND, False), ("10i", "R", CGND, False),
       ("11g", "R", C3V3, False), ("11h", "R", C3V3, False), ("11i", "R", C3V3, False),
       ("14g", "R", COPT, True), ("14h", "R", COPT, True)]
# labels at the edge: (column, side, text, colour)
LABELS = [(2, "L", "D1  <- home receiver, white", CSIG), (3, "L", "D2  <- push button (optional)", COPT),
          (4, "L", "D3  <- servo, orange", CSRV), (6, "L", "D5  <- buzzer + (optional)", COPT),
          (9.6, "L", "GND <- black wires of both", CGND), (10.4, "L", "light barriers (4 wires)", CGND),
          (12, "L", "drop receiver, white (-> D0)", CSIG), (13, "L", "drop emitter, red (-> D4)", CSIG),
          (4, "R", "D10 -> ULN2003 IN4", CULN), (5, "R", "D9  -> ULN2003 IN3", CULN), (6, "R", "D8  -> ULN2003 IN2", CULN),
          (7, "R", "D7  -> ULN2003 IN1", CULN), (8.75, "R", "5V  -> ULN2003 +,  servo red", C5V),
          (9.85, "R", "GND -> ULN2003 -,  servo brown", CGND), (10.95, "R", "3V3 -> red wires: home receiver,", C3V3),
          (11.55, "R", "home emitter, drop receiver", C3V3), (14.4, "R", "GND -> button, buzzer - (optional)", COPT)]


def wire_path(a, b, bulge):
    (x1, y1), (x2, y2) = hole(a), hole(b)
    mx, my = (x1 + x2) / 2, (y1 + y2) / 2
    if bulge == "L":
        cx = min(x1, x2) - 2.2 * P
        return "M%.1f %.1f C%.1f %.1f %.1f %.1f %.1f %.1f" % (x1, y1, cx, y1, cx, y2, x2, y2)
    if bulge == "R":
        cx = max(x1, x2) + 2.2 * P
        return "M%.1f %.1f C%.1f %.1f %.1f %.1f %.1f %.1f" % (x1, y1, cx, y1, cx, y2, x2, y2)
    return "M%.1f %.1f Q%.1f %.1f %.1f %.1f" % (x1, y1, mx + 0.5 * P, my, x2, y2)


def breadboard_svg():
    W, H = 900, 660
    o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d">' % (W, H, W, H),
         '<rect width="%d" height="%d" fill="#ffffff"/>' % (W, H),
         text(W / 2, 24, "Breadboard, seen from above: back wall at the top, front of the dispenser below", 15, "middle", "bold"),
         text(W / 2, 44, "columns 1-17 from the back wall, rows a-e on the left, f-j on the right (as printed on most boards)", 12, "middle", fill="#555")]
    # back wall + USB-C
    o.append('<rect x="%.1f" y="%.1f" width="%.1f" height="8" fill="#5e6a75"/>' % (X0 - 3 * P, Y0 - 2.6 * P, 17 * P))
    o.append(text(X0 - 3.2 * P, Y0 - 2.75 * P, "back wall of the base", 11, "start", fill="#333"))
    # board
    o.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="6" fill="#f3f1ea" stroke="#bdb8a8"/>' % (
        X0 - 0.9 * P, Y0 - 1.25 * P, 12.8 * P, 18.5 * P))
    o.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="#e2ded2"/>' % (X0 + 4.7 * P, Y0 - 1.25 * P, 1.6 * P, 18.5 * P))
    for col in range(1, 18):
        for r in ROWX:
            x, y = hole("%d%s" % (col, r))
            o.append('<rect x="%.1f" y="%.1f" width="7" height="7" fill="#8a857a"/>' % (x - 3.5, y - 3.5))
    for r, k in ROWX.items():
        o.append(text(X0 + k * P, Y0 + 17.05 * P, r, 11, "middle", fill="#777"))
    # XIAO
    xa, xb = X0 + 1.5 * P, X0 + 8.5 * P
    ya, yb = Y0 - 1.13 * P, Y0 + 7.13 * P
    o.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="5" fill="#1d3557" fill-opacity="0.88"/>' % (xa, ya, xb - xa, yb - ya))
    o.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="3" fill="#b8bec6" stroke="#6c737c"/>' % (
        X0 + 3.4 * P, ya - 0.55 * P, 3.6 * P, 0.95 * P))
    o.append(text(X0 + 5.2 * P, ya - 0.7 * P, "USB-C", 10, "middle", fill="#333"))
    o.append(text(X0 + 5.0 * P, Y0 + 3.2 * P, "XIAO", 15, "middle", "bold", "#ffffff"))
    o.append(text(X0 + 5.0 * P, Y0 + 3.9 * P, "ESP32-C6", 11, "middle", fill="#ffffff"))
    for k in range(7):
        for name, row, anc, dx in ((XIAO_L[k], "c", "start", 9), (XIAO_R[k], "g", "end", -9)):
            x, y = hole("%d%s" % (k + 1, row))
            o.append('<circle cx="%.1f" cy="%.1f" r="4.2" fill="#e8c35a" stroke="#7a6320"/>' % (x, y))
            o.append(text(x + dx, y + 4, name, 11, anc, "bold", "#ffffff"))
    # capacitor across 5V (1j) and GND (2j), lying flat beside the board
    x1, y1 = hole("1j"); x2, y2 = hole("2j")
    o.append('<path d="M%.1f %.1f L%.1f %.1f M%.1f %.1f L%.1f %.1f" stroke="#444" stroke-width="2"/>' % (
        x1, y1, x1 + 1.6 * P, y1 - 0.2 * P, x2, y2, x2 + 1.6 * P, y2 + 0.2 * P))
    o.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="5" fill="#2b3a67" stroke="#111"/>' % (
        x1 + 1.6 * P, y1 - 0.75 * P, 1.6 * P, (y2 - y1) + 1.5 * P))
    o.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="#c9d1e6"/>' % (x1 + 1.6 * P, y2 + 0.45 * P, 1.6 * P, 0.3 * P))
    o.append(text(x1 + 3.4 * P, y1 - 0.35 * P, "470 uF: long leg (+) in 1j,", 11, fill="#222"))
    o.append(text(x1 + 3.4 * P, y1 + 0.3 * P, "short leg (-, stripe) in 2j; bend it", 11, fill="#222"))
    o.append(text(x1 + 3.4 * P, y1 + 0.95 * P, "flat along the edge of the board,", 11, fill="#222"))
    o.append(text(x1 + 3.4 * P, y1 + 1.6 * P, "body towards the front", 11, fill="#222"))
    # wires leaving the board
    for h, side, colr, dash in OUT:
        x, y = hole(h)
        xe = X0 - 3.1 * P if side == "L" else X0 + 14.1 * P
        dd = ' stroke-dasharray="6 4"' if dash else ""
        o.append('<path d="M%.1f %.1f L%.1f %.1f" stroke="%s" stroke-width="3" stroke-linecap="round"%s/>' % (x, y, xe, y, colr, dd))
        o.append('<circle cx="%.1f" cy="%.1f" r="3.4" fill="%s"/>' % (x, y, colr))
    # jumpers on the board
    for a, b, colr, dash in LINKS:
        bulge = "L" if b in ("1a", "5a") else None
        dd = ' stroke-dasharray="6 4"' if dash else ""
        o.append('<path d="%s" fill="none" stroke="%s" stroke-width="3.2" stroke-linecap="round" stroke-opacity="0.9"%s/>' % (
            wire_path(a, b, bulge), colr, dd))
        for hh in (a, b):
            x, y = hole(hh)
            o.append('<circle cx="%.1f" cy="%.1f" r="3.6" fill="%s"/>' % (x, y, colr))
    halo = 'paint-order="stroke" stroke="#ffffff" stroke-width="4"'
    for col in range(1, 18):
        o.append(text(X0 - 1.45 * P, Y0 + (col - 1) * P + 4, str(col), 11, "end", "bold", "#777", halo))
        o.append(text(X0 + 12.45 * P, Y0 + (col - 1) * P + 4, str(col), 11, "start", "bold", "#777", halo))
    for col, side, s, colr in LABELS:
        y = Y0 + (col - 1) * P + 4
        if side == "L":
            o.append(text(X0 - 3.3 * P, y, s, 12, "end", fill=colr))
        else:
            o.append(text(X0 + 14.3 * P, y, s, 12, "start", fill=colr))
    # legend
    ly = H - 34
    items = [(C5V, "5 V"), (CGND, "GND"), (C3V3, "3.3 V"), (CULN, "stepper driver"), (CSIG, "light barrier signals"),
             (CSRV, "servo signal"), (COPT, "optional (dashed)")]
    x = 40
    for colr, s in items:
        o.append('<path d="M%.1f %.1f L%.1f %.1f" stroke="%s" stroke-width="4" stroke-linecap="round"/>' % (x, ly, x + 22, ly, colr))
        o.append(text(x + 28, ly + 4, s, 12))
        x += 40 + 7.2 * len(s)
    o.append(text(40, ly + 24, "Holes right next to the XIAO (rows b and h, columns 1-8) touch the edge of its board: use them last.", 11, fill="#555"))
    o.append("</svg>")
    return "\n".join(o)


# ------------------------------------------------------------------ top view of the base
S = 2.0                          # px per mm
CX, CY = 430.0, 300.0            # axle


def pt(x, y):                    # world (x to the front, y to the right) -> drawing (front at the bottom)
    return CX + S * y, CY + S * x


def rot(x, y, ang):
    a = math.radians(ang)
    return x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)


def poly(pts, fill, stroke="#333", extra=""):
    return '<polygon points="%s" fill="%s" stroke="%s" %s/>' % (" ".join("%.1f,%.1f" % pt(*p) for p in pts), fill, stroke, extra)


def rect_rot(x0, x1, y0, y1, ang):
    return [rot(x, y, ang) for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]


def layout_svg():
    W, H = 960, 720
    o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d">' % (W, H, W, H),
         '<rect width="%d" height="%d" fill="#ffffff"/>' % (W, H),
         text(W / 2, 26, "Inside the base, seen from above (carousel removed): back at the top, station and cup at the bottom", 15, "middle", "bold")]
    LX, RX = CX - d.TUB_R * S - 14, CX + d.TUB_R * S + 14       # label columns left and right of the tub

    def callout(side, ly, label, target, size=12, weight="bold", fill="#222"):
        """label outside the tub with a thin leader line to target (world x, y); no target: label only"""
        if target is None:
            o.append(text(LX if side == "L" else RX, ly, label, size, "end" if side == "L" else "start", weight, fill))
            return
        px, py = pt(*target)
        if side == "L":
            o.append('<path d="M%.1f %.1f L%.1f %.1f" stroke="#8a929a" stroke-width="1"/>' % (LX + 4, ly - 4, px, py))
            o.append(text(LX, ly, label, size, "end", weight, fill))
        else:
            o.append('<path d="M%.1f %.1f L%.1f %.1f" stroke="#8a929a" stroke-width="1"/>' % (RX - 4, ly - 4, px, py))
            o.append(text(RX, ly, label, size, "start", weight, fill))
        o.append('<circle cx="%.1f" cy="%.1f" r="2.5" fill="#8a929a"/>' % (px, py))

    R = d.TUB_R
    # tub outline with the flat front face
    a_f = math.degrees(math.acos(d.FLAT_X / R))
    pts = [(R * math.cos(math.radians(a)), R * math.sin(math.radians(a))) for a in [a_f + k * (360 - 2 * a_f) / 120 for k in range(121)]]
    o.append(poly(pts, "#eef1f4", "#5e6a75", 'stroke-width="5"'))
    # tower and cup outside the front face
    o.append(poly([(96.2, -40), (144, -40), (144, 40), (96.2, 40)], "#d5dade", "#5e6a75"))
    o.append(poly([(d.CUP_X0, -23.8), (136.2, -23.8), (136.2, 23.8), (d.CUP_X0, 23.8)], "#fafafa", "#888"))
    x, y = pt(132, 0); o.append(text(x, y + 4, "cup", 12, "middle", fill="#555"))
    x, y = pt(140, -38); o.append(text(x, y + 4, "tower + servo", 11, "start", fill="#555"))
    # chute and drop light barrier
    o.append(poly([(d.CH_X0, -d.CH_HW), (d.CH_X1, -d.CH_HW), (d.CH_X1, d.CH_HW), (d.CH_X0, d.CH_HW)], "#f3dd8a", "#a88a1e"))
    for s in (1, -1):
        y0, y1 = sorted((s * 12.2, s * 32.2))
        o.append(poly([(72.5, y0), (82.5, y0), (82.5, y1), (72.5, y1)], "#26282c", "#000"))
    x, y = pt(64, 0); o.append(text(x, y + 4, "chute", 12, "middle", fill="#6b5a10"))
    # centre pillar, flag ring, index light barrier
    x, y = pt(0, 0)
    o.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="#cfd5db" stroke="#5e6a75"/>' % (x, y, 13 * S))
    o.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="#888"/>' % (x, y, 4 * S))
    o.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="none" stroke="#999" stroke-dasharray="5 4"/>' % (x, y, d.HOME_R * S))
    o.append(text(x, y + d.HOME_R * S + 15, "index flags pass here (r 34)", 10, "middle", fill="#777"))
    a1 = math.sqrt(d.HOME_R ** 2 - d.HOME_B ** 2)
    for a0 in (a1 - 12.25 - 20.0, a1 + 7.25):
        o.append(poly(rect_rot(a0, a0 + 20.0, d.HOME_B - 4.0, d.HOME_B + 4.0, d.HOME_ROT), "#26282c", "#000"))
    # stepper, pinion, posts
    mx = -(d.GEAR_C + 8.0)
    sx, sy = rot(mx, 0, d.STEPPER_ANG - 180)
    x, y = pt(sx, sy)
    o.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="#c9cf7a" stroke="#6b7020"/>' % (x, y, 14.1 * S))
    px_, py_, _ = d.pinion_pos()
    x2, y2 = pt(px_, py_)
    o.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="#e06b5f" stroke="#8a2a20"/>' % (x2, y2, 10 * S))
    for s in (1, -1):
        o.append(poly(rect_rot(mx - 5, mx + 5, s * 17.5 - 5, s * 17.5 + 5, d.STEPPER_ANG - 180), "#9aa3ab", "#5e6a75"))
    # ULN2003
    o.append(poly(rect_rot(d.ULN_R - 16, d.ULN_R + 16, -17.5, 17.5, d.ULN_ANG), "#2a7a4b", "#14482b"))
    # breadboard + XIAO + USB-C plug in the back wall
    o.append(poly(rect_rot(d.BB_R1 - 46.9, d.BB_R1, -17.5, 17.5, d.BB_ANG), "#f3f1ea", "#bdb8a8"))
    xo = d.BB_R1 + 0.8
    o.append(poly(rect_rot(xo - 21.0, xo, -8.9, 8.9, d.BB_ANG), "#1d3557", "#0d1b2a"))
    o.append(poly(rect_rot(d.TUB_R - 3, d.TUB_R + 18, -6.2, 6.2, d.BB_ANG), "#26282c", "#000"))
    x, y = pt(-90, 0); o.append(text(x, y + 4, "XIAO", 11, "middle", "bold", "#ffffff"))
    x, y = pt(-118, 0); o.append(text(x + 18, y + 4, "USB-C cable through the back wall", 12, "start", "bold"))
    # button hole, servo cable notch
    bx, by = rot(d.TUB_R, 0, d.BTN_ANG)
    x, y = pt(bx, by)
    o.append('<circle cx="%.1f" cy="%.1f" r="7" fill="#ffffff" stroke="#7f7f7f" stroke-width="2" stroke-dasharray="3 2"/>' % (x, y))
    x, y = pt(d.FLAT_X - 1, 29)
    o.append('<rect x="%.1f" y="%.1f" width="7" height="7" fill="%s"/>' % (x - 3.5, y - 3.5, CSRV))
    # cable routes, all flat on the floor
    def route(points, colr, width=5, dash=False):
        dd = ' stroke-dasharray="8 5"' if dash else ""
        return '<polyline points="%s" fill="none" stroke="%s" stroke-width="%d" stroke-opacity="0.6" stroke-linejoin="round" stroke-linecap="round"%s/>' % (
            " ".join("%.1f,%.1f" % pt(*p) for p in points), colr, width, dd)
    ex, ey = rot(mx - 15, 0, d.STEPPER_ANG - 180)                                                                # stepper cable exit
    u2x, u2y = rot(d.ULN_R, 17.5, d.ULN_ANG)                                                                     # ULN2003, back edge
    CHOME, CSTEP = "#17a2b8", "#6b7020"
    o.append(route([(94, 29), (60, 42), (20, 46), (-5, 30), (-40, 24), (-62, 18)], CSRV))                       # servo
    o.append(route([(40, 66), (10, 58), (-15, 28), (-45, 20), (-70, 16)], CULN))                               # ULN2003 -> D7..D10, 5V, GND
    o.append(route([(ex, ey), (-22, 70), (u2x - 2, u2y - 1)], CSTEP))                                          # stepper cable -> ULN
    o.append(route([(77, -34), (45, -36), (5, -30), (-25, -24), (-62, -18)], CSIG))                            # drop barrier
    o.append(route([(25, -1), (20, -14), (0, -18), (-35, -20), (-75, -18)], CHOME))                            # index receiver
    o.append(route([(5, -57), (-20, -40), (-50, -22), (-72, -18)], CHOME))                                     # index emitter
    o.append(route([(bx - 3, by - 2), (30, 70), (0, 40), (-40, 22), (-70, 15)], COPT, 3, True))                 # button
    # labels
    callout("L", 150, "breadboard + XIAO ESP32-C6", (-d.BB_R1 + 12, -17.5))
    a_e = a1 + 7.25
    callout("L", 300, "index light barrier", rot(a_e + 6, d.HOME_B, d.HOME_ROT))
    callout("L", 316, "emitter outside the flag ring,", None, 11, "normal", "#555")
    callout("L", 330, "receiver inside it", None, 11, "normal", "#555")
    callout("L", 470, "drop light barrier in the chute", (77.5, -32.2))
    callout("R", 200, "stepper 28BYJ-48 under its pinion", (sx, sy + 12))
    callout("R", 320, "ULN2003 driver board", rot(d.ULN_R + 10, 0, d.ULN_ANG))
    callout("R", 396, "hole for the optional push button", (bx - 3, by), 11, "normal", "#555")
    callout("R", 450, "servo cable + 15 cm extension", (77, 35.5), 12, "bold", CSRV)
    callout("R", 520, "servo cable comes in through this slot", (d.FLAT_X - 1, 29), 11, "normal", CSRV)
    # legend and notes
    ly = H - 74
    items = [(CSRV, "servo"), (CULN, "ULN2003 to breadboard"), (CSTEP, "stepper to ULN2003"),
             (CSIG, "drop light barrier"), (CHOME, "index light barrier"), (COPT, "push button (optional)")]
    x = 30
    for colr, s in items:
        dd = ' stroke-dasharray="8 5"' if colr == COPT else ""
        o.append('<path d="M%.1f %.1f L%.1f %.1f" stroke="%s" stroke-width="5" stroke-opacity="0.6" stroke-linecap="round"%s/>' % (
            x, ly, x + 22, ly, colr, dd))
        o.append(text(x + 28, ly + 4, s, 11))
        x += 42 + 6.2 * len(s)
    zf = d.Z_PLATE_TOP - d.HUB_H - 13.0 - d.FLOOR_T                 # lowest point of the index flags above the floor
    zs = d.MOTOR_M - 19.0 - d.FLOOR_T                                # underside of the stepper body
    o.append(text(30, H - 46, "All wires lie flat on the floor. The index flags under the carousel come down to %.0f mm above the floor, "
                  "the stepper body to %.0f mm:" % (zf, zs), 11, fill="#555"))
    o.append(text(30, H - 30, "wires may pass under the stepper, but no loop of wire may stand up inside the base. "
                  "Left and right as seen standing in front of the dispenser.", 11, fill="#555"))
    o.append(text(W - 30, 70, "BACK", 13, "end", "bold", "#5e6a75"))
    o.append(text(W - 30, H - 100, "FRONT", 13, "end", "bold", "#5e6a75"))
    o.append("</svg>")
    return "\n".join(o)


if __name__ == "__main__":
    img = os.path.join(ROOT, "docs", "img")
    open(os.path.join(img, "wiring.svg"), "w").write(breadboard_svg())
    open(os.path.join(img, "layout.svg"), "w").write(layout_svg())
    print("wrote docs/img/wiring.svg and docs/img/layout.svg")
