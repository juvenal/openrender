"""Shared pieces of the spec 056 scene generator (tools/gen_scenes.py).

A family module exposes scenes() -> list of Scene.  Every probe scene is written in CAMERA SPACE: the camera
transform is a roll plus a translation (not the identity, research R11), and the geometry sits inside a block
whose transform is the inverse of it, so the camera-space coordinates in the family modules are the ones the
tracer sees.  Hand-computed expectations are computed here, from the same numbers the RIB gets.
"""
import math

HIDERS = ["reyes", "zbuffer", "raytrace"]
FORMATS = ["rslo", "slo"]

# The camera transform (RIB order: Rotate, then Translate).  World = inverse of it.
ROLL = 20.0
CAM_T = (0.3, -0.2, 2.0)

# One shared pair of fog parameters per family keeps the headers readable
DIST = 4.0
BG = (0.1, 0.3, 0.9)
CB = (0.8, 0.5, 0.2)    # a backdrop / hit colour
CB2 = (0.2, 0.6, 0.4)   # a second hit colour

TOL_FOG = 0.0001        # absolute, a value that contains exp()
TOL_COLOUR = 0.00001    # absolute, a pass-through colour

PROBES = []             # every probe registration, in generation order: see gen_scenes.py


class Probe:
    """One pixel read back with test_pixel_probe: scene name, pixel, tolerance, expected channels."""
    def __init__(self, scene, x, y, tol, values, tag):
        self.scene, self.x, self.y, self.tol, self.values, self.tag = scene, x, y, tol, values, tag


class Scene:
    def __init__(self, name, hider, slo, body, comment, fmt=(8, 8), floats=True, projection="orthographic",
                 clipping="0.1 100", camera=True, pixel_samples="1 1", extra_header=None, world_extra=None):
        self.name = name
        self.hider = hider
        self.slo = slo
        self.body = body
        self.comment = comment
        self.fmt = fmt
        self.floats = floats
        self.projection = projection
        self.clipping = clipping
        self.camera = camera
        self.pixel_samples = pixel_samples
        self.extra_header = extra_header or []
        self.world_extra = world_extra or []


def fog(distance=DIST, bg=BG):
    return '"float distance" [%g] "color background" [%g %g %g]' % ((distance,) + tuple(bg))


def col(c):
    return "[%g %g %g]" % tuple(c)


def quad(z, x0, x1, y0, y1, n=(0, 0, -1)):
    """An axis-aligned polygon at camera-space depth z with an explicit normal."""
    p = [x0, y0, z, x1, y0, z, x1, y1, z, x0, y1, z]
    return ('Polygon "P" [%s] "N" [%s]' % (" ".join("%g" % v for v in p),
                                           " ".join("%g" % v for v in list(n) * 4)))


def pixel_centre(i, size=8):
    """Screen coordinate (-1..1) of the centre of pixel i of `size`."""
    return -1.0 + 2.0 * (i + 0.5) / size


def fogged(c, length, distance=DIST, bg=BG):
    """The attenuated colour of `c` over a segment: c*T + bg*d, T = exp(-length/distance)."""
    T = math.exp(-length / distance)
    d = 1.0 - T
    return tuple(c[k] * T + bg[k] * d for k in range(3))


def hider_line(hider):
    if hider == "raytrace":
        return 'Hider "raytrace" "jitter" [0]'
    if hider == "zbuffer":
        return 'Hider "zbuffer" "jitter" [0]'
    return 'Hider "reyes" "jitter" [0]'


def rib_text(s):
    L = ["##RenderMan RIB-Structure 1.0"]
    for line in s.comment:
        L.append(("# " + line).rstrip())
    L.append("version 3.03")
    if s.slo:
        L.append('Option "shaderformat" "default" ["slo"]')
    L.append(hider_line(s.hider))
    L.append("PixelSamples %s" % s.pixel_samples)
    L.append('PixelFilter "box" 1 1')
    if s.floats:
        L.append('Quantize "rgba" 0 0 0 0')
    L.extend(s.extra_header)
    L.append('Display "%s.tif" "file" "rgba"' % s.name)
    L.append("Format %d %d 1" % s.fmt)
    L.append("Projection %s" % (s.projection if s.projection.startswith('"') or " " in s.projection else '"%s"' % s.projection))
    L.append("Clipping %s" % s.clipping)
    if s.camera:
        L.append("Rotate %g 0 0 1" % ROLL)
        L.append("Translate %g %g %g" % CAM_T)
    L.append("WorldBegin")
    L.extend("  " + w for w in s.world_extra)
    if s.camera:
        L.append("  TransformBegin")
        L.append("    # camera space: the inverse of the camera transform above")
        L.append("    Translate %g %g %g" % (-CAM_T[0], -CAM_T[1], -CAM_T[2]))
        L.append("    Rotate %g 0 0 1" % -ROLL)
        L.extend("    " + b for b in s.body)
        L.append("  TransformEnd")
    else:
        L.extend("  " + b for b in s.body)
    L.append("WorldEnd")
    return "\n".join(L) + "\n"


def variants(base, hiders=HIDERS, formats=FORMATS):
    """(suffix, hider, slo) for every variant of a family: name-<hider>[-slo]."""
    for h in hiders:
        for f in formats:
            yield ("-%s%s" % (h, "-slo" if f == "slo" else ""), h, f == "slo")


def probe(scene, x, y, tol, values, tag):
    PROBES.append(Probe(scene, x, y, tol, values, tag))


VIS = 'Attribute "visibility" "int specular" [1] "int transmission" [1]'
BIAS = 0.01                                   # the default trace bias, pinned in the scenes that end a segment at a destination
BIAS_LINE = 'Attribute "trace" "float bias" [%g]' % BIAS


def exit_through_aabb(origin, direction, bmin, bmax):
    """Far exit distance of a ray from an axis-aligned box (double precision slab test), >= 0; 0 if it never enters."""
    t_near, t_far = 0.0, 1e300
    for k in range(3):
        if bmin[k] > bmax[k]:
            return 0.0
        o, d = origin[k], direction[k]
        if d == 0:
            if o < bmin[k] or o > bmax[k]:
                return 0.0
            continue
        t0, t1 = (bmin[k] - o) / d, (bmax[k] - o) / d
        if t0 > t1:
            t0, t1 = t1, t0
        t_near, t_far = max(t_near, t0), min(t_far, t1)
    if t_near > t_far:
        return 0.0
    return max(0.0, t_far)


def to_world(p):
    """Camera-space point -> world-space point (the block transform: Translate(-t) * Rotate(-ROLL))."""
    a = math.radians(-ROLL)
    x = p[0] * math.cos(a) - p[1] * math.sin(a)
    y = p[0] * math.sin(a) + p[1] * math.cos(a)
    return (x - CAM_T[0], y - CAM_T[1], p[2] - CAM_T[2])


def world_aabb(points):
    ws = [to_world(p) for p in points]
    return (tuple(min(w[k] for w in ws) for k in range(3)), tuple(max(w[k] for w in ws) for k in range(3)))
