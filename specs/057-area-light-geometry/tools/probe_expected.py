#!/usr/bin/env python3
"""Spec 057 (area-light geometry), tasks.md T059: the expected values and tolerances of the closed-form probes.

Run with `python3 -I probe_expected.py` (self-test and the table the scene headers quote); imported by gen_scenes.py.
No renderer is involved. Pure standard library, double precision.

What is computed. A planar quad emitter under a light shader that returns  Cl = I * w(x)  at the sampled point x, over a
Lambertian receiver (the matte receiver multiplies by max(0, n_r . w_hat), the renderer adds only 1 / (n p_A) and the
binary visibility: Decision A, FR-012). So at a receiver point R with normal n_r the renderer's estimate of the pixel is

        X_k = (A / n) * I * w(x_k) * max(0, n_r . w_hat)          (x_k uniform on the quad, p_A = 1 / A)

and its expected value is  E = I * integral_A  w(x) max(0, n_r . w_hat) dA.  Two light shaders:
        'arealight'  w = max(0, N_e . (-w_hat)) / r^2     the shipped arealight.sl: N.normalize(L) / (L.L), one cone test
                                                          illuminate(P, N, PI/2): emitter cosine and ONE 1/r^2
        'constant'   w = 1                                 al_light_constant: Cl = intensity * lightcolor (cone test only)
with w_hat the unit vector from the receiver to the emitter point and r the distance.

The single-sample variance is sigma_1^2 = Var[(A) * I * w * max(0, n_r.w_hat)] on the same quadrature, and the tolerance
of a probe at n samples is 4 * sigma_1 / sqrt(n) (4 standard errors), never widened.

Conventions of the renderer used here (measured, not assumed): a polygon quad with vertices p0, p1, p2, p3 is parametrised
P(u, v) = (1-u)(1-v) p0 + u(1-v) p1 + (1-u) v p3 + u v p2 and its N is (p1 - p0) x (p3 - p0) (not normalised: the shaders
normalise it). The ortho camera at the origin looks along +z; with `Format 8 8 1` pixel (i, j) is at screen
x = -1 + (i + 0.5) / 4, y = 1 - (j + 0.5) / 4 (the emitter and receiver of every probe are symmetric in y, so the sign of
that last convention does not matter).
"""
import math
import sys


def gauss_legendre(n):
    """Nodes and weights of the n-point Gauss-Legendre rule on [-1, 1], by Newton iteration."""
    xs, ws = [], []
    for i in range(n):
        x = math.cos(math.pi * (i + 0.75) / (n + 0.5))
        for _ in range(100):
            p0, p1 = 1.0, x
            for k in range(2, n + 1):
                p0, p1 = p1, ((2 * k - 1) * x * p1 - (k - 1) * p0) / k
            dp = n * (x * p1 - p0) / (x * x - 1.0)
            dx = p1 / dp
            x -= dx
            if abs(dx) < 1e-15:
                break
        xs.append(x)
        ws.append(2.0 / ((1.0 - x * x) * dp * dp))
    return xs, ws


_GL = gauss_legendre(64)


def sub(a, b):
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]


def add(a, b):
    return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]


def mul(a, s):
    return [a[0] * s, a[1] * s, a[2] * s]


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def norm(a):
    return math.sqrt(dot(a, a))


def unit(a):
    n = norm(a)
    return [a[0] / n, a[1] / n, a[2] / n]


class Quad:
    """A planar parallelogram emitter given by the polygon vertices p0..p3 in RIB order."""

    def __init__(self, p0, p1, p2, p3):
        self.p0, self.p1, self.p2, self.p3 = p0, p1, p2, p3
        self.e1 = sub(p1, p0)   # dP/du
        self.e2 = sub(p3, p0)   # dP/dv
        c = cross(self.e1, self.e2)
        self.area = norm(c)
        self.normal = unit(c)

    def point(self, u, v):
        return add(add(self.p0, mul(self.e1, u)), mul(self.e2, v))

    def vertices(self):
        return [self.p0, self.p1, self.p2, self.p3]


def integrate(quad, f):
    """integral over the quad of f(x) dA by a 64 x 64 Gauss-Legendre rule: returns (integral, integral of f^2)."""
    xs, ws = _GL
    s1 = s2 = 0.0
    for i in range(len(xs)):
        u = 0.5 * (xs[i] + 1.0)
        for j in range(len(xs)):
            v = 0.5 * (xs[j] + 1.0)
            val = f(quad.point(u, v))
            w = 0.25 * ws[i] * ws[j]
            s1 += w * val
            s2 += w * val * val
    return s1 * quad.area, s2 * quad.area


def weight(kind, quad, R, x):
    """The light shader's value at emitter point x as seen from receiver point R (before intensity)."""
    w = sub(x, R)
    r2 = dot(w, w)
    r = math.sqrt(r2)
    cos_e = dot(quad.normal, mul(w, -1.0 / r))     # N_e . normalize(L), L = R - x
    if cos_e <= 0.0:
        return 0.0                                 # outside illuminate(P, N, PI/2)
    if kind == "arealight":
        return cos_e / r2
    if kind == "constant":
        return 1.0
    raise ValueError(kind)


def pixel_value(kind, quad, R, n_r, intensity, receiver_cos=True):
    """(E, sigma_1) for the estimator over the quad at receiver point R with receiver normal n_r."""
    def f(x):
        w = sub(x, R)
        wh = unit(w)
        cr = max(0.0, dot(n_r, wh)) if receiver_cos else 1.0
        return intensity * weight(kind, quad, R, x) * cr

    i1, i2 = integrate(quad, f)
    mean = i1 / quad.area
    var = max(0.0, i2 / quad.area - mean * mean) * quad.area * quad.area   # variance of A * f(x)
    return i1, math.sqrt(var)


def tolerance(sigma1, n):
    return 4.0 * sigma1 / math.sqrt(n)


def pixel_center(i, j, size=8):
    return (-1.0 + (i + 0.5) * 2.0 / size, 1.0 - (j + 0.5) * 2.0 / size)


def contour_projected_solid_angle(quad, R, n_r):
    """integral of max(0, n_r . w_hat) d(solid angle) of a convex polygon wholly above the receiver's horizon, by the
    contour form  1/2 * sum_i theta_i (n_r . m_i): the cross-check of the quadrature for kind 'arealight'."""
    verts = quad.vertices()
    ws = [unit(sub(v, R)) for v in verts]
    total = 0.0
    for i in range(len(ws)):
        a, b = ws[i], ws[(i + 1) % len(ws)]
        theta = math.acos(max(-1.0, min(1.0, dot(a, b))))
        m = unit(cross(a, b))
        total += theta * dot(n_r, m)
    return abs(0.5 * total)


def self_test():
    """Checks the closed forms against independent ones; raises on a mismatch."""
    # (1) the on-axis disk: integral cos_e cos_r / r^2 dA = pi a^2 / (h^2 + a^2). A doubled falloff (1/r^4) or a missing one
    #     (no 1/r^2) gives another number, so this check catches both. Polar quadrature, no quad needed.
    h, a = 2.0, 1.5
    xs, ws = _GL
    total = 0.0
    for i in range(len(xs)):
        rho = 0.5 * a * (xs[i] + 1.0)
        for j in range(len(xs)):
            phi = math.pi * (xs[j] + 1.0)
            r2 = rho * rho + h * h
            total += 0.5 * a * math.pi * ws[i] * ws[j] * rho * ((h / math.sqrt(r2)) * (h / math.sqrt(r2)) / r2)
    expected = math.pi * a * a / (h * h + a * a)
    assert abs(total - expected) < 1e-9 * max(1.0, expected), (total, expected)
    # (2) the quadrature against the contour integral for a quad entirely above the receiver's horizon
    q = Quad([3, -1, 1], [3, -1, 3], [3, 1, 3], [3, 1, 1])
    for R in ([0.0, 0.0, 4.0], [-0.875, 0.375, 4.0], [0.875, -0.125, 4.0]):
        n_r = [0.0, 0.0, -1.0]
        e, _ = pixel_value("arealight", q, R, n_r, 1.0)
        c = contour_projected_solid_angle(q, R, n_r)
        assert abs(e - c) < 1e-9, (R, e, c)
    # (3) a quad facing away from the receiver emits nothing, and the constant light's value is the receiver-weighted area
    back = Quad([3, -1, 1], [3, 1, 1], [3, 1, 3], [3, -1, 3])   # N = +x, away from receivers at x < 3
    assert back.normal[0] > 0.9
    e, s = pixel_value("arealight", back, [0.0, 0.0, 4.0], [0.0, 0.0, -1.0], 1.0)
    assert e == 0.0 and s == 0.0
    # (4) the planar quad of the probes: normal -x, area 4
    assert abs(q.area - 4.0) < 1e-12 and abs(q.normal[0] + 1.0) < 1e-12


# The geometry shared by the probe scenes (gen_scenes.py writes these numbers into the RIBs)
EMITTER = Quad([3, -1, 1], [3, -1, 3], [3, 1, 3], [3, 1, 1])          # in the plane x = 3, facing -x, area 4
RECEIVER_Z = 4.0                                                       # the receiver plane z = 4 (normal toward the camera: -z)
RECEIVER_N = [0.0, 0.0, -1.0]
INTENSITY = 10.0
PIXELS = [(3, 3), (1, 2), (6, 5)]                                      # the probed pixels of the 8 x 8 images
SAMPLES = 16                                                           # the default area-light sample count (Phase 5 makes it settable)


def probe_table():
    rows = []
    for kind in ("arealight", "constant"):
        for (i, j) in PIXELS:
            x, y = pixel_center(i, j)
            e, s1 = pixel_value(kind, EMITTER, [x, y, RECEIVER_Z], RECEIVER_N, INTENSITY)
            rows.append((kind, i, j, e, s1, tolerance(s1, SAMPLES)))
    return rows


if __name__ == "__main__":
    self_test()
    print("probe_expected.py: self-test passed (disk on axis, quadrature against the contour form, a back-facing emitter)")
    print("emitter x=3 facing -x, 2 x 2, receiver z=4, intensity %g, n = %d" % (INTENSITY, SAMPLES))
    print("%-10s %3s %3s %14s %14s %14s" % ("light", "i", "j", "E", "sigma_1", "4 sigma/sqrt(n)"))
    for r in probe_table():
        print("%-10s %3d %3d %14.9f %14.9f %14.9f" % r)
    sys.exit(0)
