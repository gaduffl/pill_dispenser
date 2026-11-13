"""Tiny software renderer (numpy z-buffer) for checking the CAD without a GPU."""
import math
import numpy as np
from PIL import Image


def mesh_of(shape, tol=0.25, ang=0.35):
    solid = shape.val() if hasattr(shape, "val") else shape
    v, t = solid.tessellate(tol, ang)
    V = np.array([[p.x, p.y, p.z] for p in v], dtype=np.float32)
    T = np.array(t, dtype=np.int32)
    return V, T


def render(items, path, az=35.0, el=25.0, size=(1100, 800), zoom=None, center=None, bg=(250, 250, 248)):
    """items: list of (V, T, (r, g, b)). Orthographic view, Lambert shading."""
    az_r, el_r = math.radians(az), math.radians(el)
    # camera basis
    f = np.array([math.cos(el_r) * math.cos(az_r), math.cos(el_r) * math.sin(az_r), math.sin(el_r)])  # towards camera
    r = np.array([-math.sin(az_r), math.cos(az_r), 0.0])
    u = np.cross(f, r)
    allv = np.vstack([V for V, _, _ in items])
    if center is None:
        center = (allv.min(0) + allv.max(0)) / 2
    W, H = size
    px = (allv - center) @ r
    py = (allv - center) @ u
    if zoom is None:
        zoom = 0.92 * min(W / (px.max() - px.min() + 1e-6), H / (py.max() - py.min() + 1e-6))
    img = np.empty((H, W, 3), dtype=np.float32)
    img[:] = bg
    zbuf = np.full((H, W), -1e9, dtype=np.float32)
    light = np.array([0.35, -0.45, 0.82])
    light /= np.linalg.norm(light)
    for V, T, col in items:
        P = V - center
        sx = P @ r * zoom + W / 2
        sy = H / 2 - P @ u * zoom
        sz = P @ f
        a, b, c = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
        n = np.cross(b - a, c - a)
        ln = np.linalg.norm(n, axis=1)
        ok = ln > 1e-9
        n[ok] /= ln[ok][:, None]
        facing = n @ f
        shade = 0.38 + 0.62 * np.clip(np.abs(n @ light), 0, 1)
        col = np.array(col, dtype=np.float32)
        for i in np.nonzero(ok)[0]:
            i0, i1, i2 = T[i]
            x0, y0, x1, y1, x2, y2 = sx[i0], sy[i0], sx[i1], sy[i1], sx[i2], sy[i2]
            minx, maxx = int(max(0, math.floor(min(x0, x1, x2)))), int(min(W - 1, math.ceil(max(x0, x1, x2))))
            miny, maxy = int(max(0, math.floor(min(y0, y1, y2)))), int(min(H - 1, math.ceil(max(y0, y1, y2))))
            if maxx < minx or maxy < miny:
                continue
            den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
            if abs(den) < 1e-9:
                continue
            xs = np.arange(minx, maxx + 1) + 0.5
            ys = np.arange(miny, maxy + 1) + 0.5
            X, Y = np.meshgrid(xs, ys)
            w0 = ((y1 - y2) * (X - x2) + (x2 - x1) * (Y - y2)) / den
            w1 = ((y2 - y0) * (X - x2) + (x0 - x2) * (Y - y2)) / den
            w2 = 1 - w0 - w1
            m = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
            if not m.any():
                continue
            z = w0 * sz[i0] + w1 * sz[i1] + w2 * sz[i2]
            zb = zbuf[miny:maxy + 1, minx:maxx + 1]
            m &= z > zb
            if not m.any():
                continue
            zb[m] = z[m]
            k = shade[i] * (1.0 if facing[i] > 0 else 0.55)
            img[miny:maxy + 1, minx:maxx + 1][m] = col * k
    Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(path)
    return zoom, center
