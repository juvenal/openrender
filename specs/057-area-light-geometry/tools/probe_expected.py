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


# ---- Phase 6: soft shadows (an occluder sphere between a bound emitter and the receiver) ---------------------------------------------
class Sphere:
    """The occluder: a sphere that blocks the segment from a receiver point to an emitter point (flagged transmission 1)."""

    def __init__(self, c, r):
        self.c, self.r = c, r

    def blocks(self, a, b):
        d = sub(b, a)
        f = sub(a, self.c)
        A = dot(d, d)
        B = 2.0 * dot(f, d)
        C = dot(f, f) - self.r * self.r
        disc = B * B - 4.0 * A * C
        if disc <= 0.0:
            return False
        s = math.sqrt(disc)
        t0, t1 = (-B - s) / (2.0 * A), (-B + s) / (2.0 * A)
        return t0 < 1.0 and t1 > 0.0          # the open segment (0, 1) meets the sphere


def pixel_value_occluded(kind, quad, R, n_r, intensity, sphere, n=360):
    """(E, sigma_1, visible fraction) at receiver point R: the closed form of pixel_value with the emitter points that the
    sphere hides removed. A dense midpoint grid over the quad (the integrand is discontinuous at the shadow edge, so no Gauss rule)."""
    s1 = s2 = vis = 0.0
    total = n * n
    for i in range(n):
        u = (i + 0.5) / n
        for j in range(n):
            v = (j + 0.5) / n
            x = quad.point(u, v)
            if sphere.blocks(R, x):
                continue
            vis += 1.0
            w = sub(x, R)
            wh = unit(w)
            f = intensity * weight(kind, quad, R, x) * max(0.0, dot(n_r, wh))
            s1 += f
            s2 += f * f
    mean = s1 / total
    var = max(0.0, s2 / total - mean * mean) * quad.area * quad.area
    return mean * quad.area, math.sqrt(var), vis / total


# The soft-shadow family (the emitter is smaller than the quad probe's so that the sphere throws a shadow with an umbra, a penumbra and
# a fully lit area inside the window): a 0.5 x 0.5 quad in the plane x = 3 facing -x, centre (3, 0, 2); a sphere of radius 0.2 at
# (0.75, 0, 3.5) between it and the receiver plane z = 4. The sphere projects onto the image at x in [0.55, 0.95], clear of the probed pixels,
# which lie on the row y ~ 0 at x < 0: lit (x ~ -0.9), penumbra (x ~ -0.55) and umbra (x ~ -0.1).
EMITTER_SOFT = Quad([3, -0.25, 1.75], [3, -0.25, 2.25], [3, 0.25, 2.25], [3, 0.25, 1.75])
OCCLUDER = Sphere([0.75, 0.0, 3.5], 0.2)
INTENSITY_SOFT = 40.0
SOFT_SIZE = 256
SOFT_SAMPLES = 64                                                       # Attribute "arealight" "int samples" [64]


def soft_probe_pixels():
    """Three probed pixels of the 256 x 256 images, on the row y ~ 0, left of the sphere's image: the lit area (visible fraction 1,
    nearest the left edge), the penumbra (visible fraction nearest 0.5) and the umbra (visible fraction 0, nearest x = -0.1),
    as (name, i, j, E, sigma_1, visible fraction)."""
    j = SOFT_SIZE // 2 - 1
    cands = []
    for i in range(0, SOFT_SIZE // 2, 2):
        x, y = pixel_center(i, j, SOFT_SIZE)
        e, s1, fr = pixel_value_occluded("arealight", EMITTER_SOFT, [x, y, RECEIVER_Z], RECEIVER_N, INTENSITY_SOFT, OCCLUDER, n=40)
        cands.append((i, x, fr))
    lit = next(c for c in cands if c[2] >= 0.999)
    pen = min(cands, key=lambda c: abs(c[2] - 0.5))
    umb = min((c for c in cands if c[2] == 0.0), key=lambda c: abs(c[1] + 0.1))
    rows = []
    for name, c in (("lit", lit), ("penumbra", pen), ("umbra", umb)):
        i = c[0]
        x, y = pixel_center(i, j, SOFT_SIZE)
        e, s1, fr = pixel_value_occluded("arealight", EMITTER_SOFT, [x, y, RECEIVER_Z], RECEIVER_N, INTENSITY_SOFT, OCCLUDER)
        rows.append((name, i, j, e, s1, fr))
    return rows


def pixel_value_multi(kind, quads, R, n_r, intensity):
    """(E, sigma_1) for one light whose scope holds several emitter quads (no occluder): the estimator picks a leaf with probability
    proportional to its area and a point on it uniformly, so p_A = 1 / (total area) and  sigma_1^2 = A_tot * sum_q int f^2 dA - E^2."""
    a_tot = sum(q.area for q in quads)
    e = 0.0
    i2 = 0.0
    for q in quads:
        def f(x, q=q):
            w = sub(x, R)
            wh = unit(w)
            return intensity * weight(kind, q, R, x) * max(0.0, dot(n_r, wh))
        a1, a2 = integrate(q, f)
        e += a1
        i2 += a2
    return e, math.sqrt(max(0.0, a_tot * i2 - e * e))


# AreaLight_SelfExclusion_stacked: two flagged emitter quads of ONE light, the larger in front of the soft emitter from the receiver's
# side. Nothing else blocks: with the light's own scope excluded from its occlusion rays (FR-017) the receiver reads E_A + E_B; with
# no exclusion, B hides the samples of A.
EMITTER_FRONT = Quad([2.5, -0.5, 1.5], [2.5, -0.5, 3.0], [2.5, 0.5, 3.0], [2.5, 0.5, 1.5])


# ---- Phase 7: quadric emitters (a disk and a sphere under the shipped arealight) --------------------------------------------------------
class QuadricEmitter:
    """A curved emitter parametrized over the unit square by an equal-area map, so a uniform (a, b) is a uniform-by-area point.
    point(s, t) -> (P, unit outward/facing normal); the area element is `area` per unit of the map's own measure."""

    def mean(self, f):
        """(mean of f over the surface by area, mean of f^2): Gauss-Legendre over the map's two coordinates."""
        raise NotImplementedError

    def integrate(self, f):
        m1, m2 = self.mean(f)
        return m1 * self.area, m2 * self.area


class DiskEmitter(QuadricEmitter):
    """A flat disk of radius R at `centre`, facing `normal` (the +z of RiDisk before the transform); full turn."""

    def __init__(self, centre, radius, normal=(0.0, 0.0, 1.0)):
        self.c, self.R, self.normal = list(centre), radius, list(unit(list(normal)))
        self.area = math.pi * radius * radius
        e1 = unit(cross(self.normal, [1.0, 0.0, 0.0] if abs(self.normal[0]) < 0.9 else [0.0, 1.0, 0.0]))
        self.e1, self.e2 = e1, cross(self.normal, e1)

    def point(self, s, phi):
        """s in [0, 1] the radius fraction, phi the angle: the point and its normal."""
        r = self.R * s
        return add(self.c, add(mul(self.e1, r * math.cos(phi)), mul(self.e2, r * math.sin(phi)))), self.normal

    def mean(self, f):
        xs, ws = _GL
        nphi = 64
        m1 = m2 = 0.0
        for i in range(len(xs)):
            s = 0.5 * (xs[i] + 1.0)
            wi = 0.5 * ws[i] * 2.0 * s            # dA / A = 2 s ds dphi / (2 pi)
            for k in range(nphi):
                phi = 2.0 * math.pi * (k + 0.5) / nphi
                p, n = self.point(s, phi)
                val = f(p, n)
                m1 += wi * val / nphi
                m2 += wi * val * val / nphi
        return m1, m2


class SphereEmitter(QuadricEmitter):
    """A full sphere of radius r at `centre`; the normal is outward."""

    def __init__(self, centre, radius):
        self.c, self.r = list(centre), radius
        self.area = 4.0 * math.pi * radius * radius

    def point(self, z, phi):
        """z in [-1, 1] (Archimedes: uniform z is uniform area), phi the longitude."""
        rho = math.sqrt(max(0.0, 1.0 - z * z))
        n = [rho * math.cos(phi), rho * math.sin(phi), z]
        return add(self.c, mul(n, self.r)), n

    def mean(self, f):
        xs, ws = _GL
        nphi = 64
        m1 = m2 = 0.0
        for i in range(len(xs)):
            z = xs[i]
            wi = 0.5 * ws[i]
            for k in range(nphi):
                phi = 2.0 * math.pi * (k + 0.5) / nphi
                p, n = self.point(z, phi)
                val = f(p, n)
                m1 += wi * val / nphi
                m2 += wi * val * val / nphi
        return m1, m2


def pixel_value_emitter(emitter, R, n_r, intensity):
    """(E, sigma_1) at receiver point R for the shipped arealight over a quadric emitter: f = I cos_e cos_r / r^2 on the points that face
    R (the light's own illuminate(P, N, PI/2) test), zero elsewhere; sigma_1 is the standard deviation of A f(x) over a uniform draw."""
    def f(p, n):
        w = sub(p, R)
        r2 = dot(w, w)
        r = math.sqrt(r2)
        cos_e = dot(n, mul(w, -1.0 / r))
        if cos_e <= 0.0:
            return 0.0
        return intensity * cos_e * max(0.0, dot(n_r, mul(w, 1.0 / r))) / r2

    m1, m2 = emitter.mean(f)
    return m1 * emitter.area, emitter.area * math.sqrt(max(0.0, m2 - m1 * m1))


def sphere_closed_form(centre, radius, R, n_r, intensity):
    """E = I pi (r / d)^2 cos(theta_c): the projected solid angle of a sphere wholly above the receiver's horizon, theta_c the angle
    between the receiver normal and the direction to the centre."""
    w = sub(centre, R)
    d = norm(w)
    return intensity * math.pi * (radius / d) ** 2 * dot(n_r, mul(w, 1.0 / d))


def disk_closed_form_on_axis(radius, d, intensity):
    """E = I pi R^2 / (R^2 + d^2): a receiver on the axis of a disk, facing it, at distance d."""
    return intensity * math.pi * radius * radius / (radius * radius + d * d)


DISK = DiskEmitter([3.0, 0.0, 2.0], 1.2)             # Translate 3 0 2, Disk 0 1.2 360: in the plane z = 2, facing +z (toward the receiver z = 4)
SPHERE = SphereEmitter([3.0, 0.0, 2.0], 0.8)         # Translate 3 0 2, Sphere 0.8 -0.8 0.8 360
QUADRIC_INTENSITY = 10.0


def quadric_self_test():
    # the disk on its axis, quadrature against the closed form (receiver normal toward the disk: -z... the disk is below, so n_r = -z)
    disk = DiskEmitter([0.0, 0.0, 2.0], 1.2)
    for d in (1.0, 2.0, 3.5):
        e, _ = pixel_value_emitter(disk, [0.0, 0.0, 2.0 + d], [0.0, 0.0, -1.0], 10.0)
        assert abs(e - disk_closed_form_on_axis(1.2, d, 10.0)) < 2e-4 * e, ("disk on axis", d, e)
    # the sphere at several receiver points and normals, quadrature against the closed form (sphere wholly above the horizon)
    for R in ([0.0, 0.0, 4.0], [-0.125, 0.125, 4.0], [0.625, -0.25, 4.0], [1.0, 1.0, 4.0]):
        e, _ = pixel_value_emitter(SPHERE, R, [0.0, 0.0, -1.0], 10.0)
        c = sphere_closed_form(SPHERE.c, SPHERE.r, R, [0.0, 0.0, -1.0], 10.0)
        assert abs(e - c) < 2e-3 * c, ("sphere", R, e, c)


def quadric_samples(emitter, pixels, n_min=16, rel=0.2):
    """The light's sample count n: the smallest power of two >= n_min with 4 sigma_1 / sqrt(n) <= rel * E at every probed pixel."""
    n = n_min
    while True:
        ok = True
        for (i, j) in pixels:
            x, y = pixel_center(i, j)
            e, s1 = pixel_value_emitter(emitter, [x, y, RECEIVER_Z], RECEIVER_N, QUADRIC_INTENSITY)
            if tolerance(s1, n) > rel * e:
                ok = False
        if ok:
            return n
        n *= 2


# ---- Phase 12 (T167): a bent bilinear patch as the emitter -------------------------------------------------------------------------

class BilinearPatchEmitter(QuadricEmitter):
    """A bilinear patch P(u, v) = (1-u)(1-v) p00 + u (1-v) p10 + (1-u) v p01 + u v p11 (the RIB order of `Patch "bilinear"`); the normal is
    dP/du x dP/dv (the renderer's Ng), the area element |dP/du x dP/dv|. Not planar when the four points are not: a twisted (saddle) surface."""

    def __init__(self, p00, p10, p01, p11):
        self.p00, self.p10, self.p01, self.p11 = list(p00), list(p10), list(p01), list(p11)
        xs, ws = _GL
        self._nodes = [(0.5 * (x + 1.0), 0.5 * w) for x, w in zip(xs, ws)]
        self.area = 0.0
        for (u, wu) in self._nodes:
            for (v, wv) in self._nodes:
                self.area += wu * wv * norm(self._jac(u, v))

    def point_only(self, u, v):
        return add(add(mul(self.p00, (1 - u) * (1 - v)), mul(self.p10, u * (1 - v))), add(mul(self.p01, (1 - u) * v), mul(self.p11, u * v)))

    def _jac(self, u, v):
        du = add(mul(sub(self.p10, self.p00), (1 - v)), mul(sub(self.p11, self.p01), v))
        dv = add(mul(sub(self.p01, self.p00), (1 - u)), mul(sub(self.p11, self.p10), u))
        return cross(du, dv)

    def point(self, u, v):
        c = self._jac(u, v)
        return self.point_only(u, v), unit(c)

    def mean(self, f):
        m1 = m2 = 0.0
        for (u, wu) in self._nodes:
            for (v, wv) in self._nodes:
                p, n = self.point(u, v)
                val = f(p, n)
                w = wu * wv * norm(self._jac(u, v)) / self.area
                m1 += w * val
                m2 += w * val * val
        return m1, m2
