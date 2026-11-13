"""volume-exterior-fog-reflection (US2, T025), volume-primitive-hitmode (T026), volume-environment-raytrace (T029),
volume-miss-clipping (T031): A reflective polygon whose reflected rays leave into the exterior side."""
from common import *

TILT = 0.5
RX, RZ = math.sqrt(1 - TILT * TILT) * 1.0, 0.0
# reflected direction of a camera ray along +z: (2*t*c, 0, 1 - 2*c^2), c = sqrt(1 - t^2)
C = math.sqrt(1 - TILT * TILT)
R = (2 * TILT * C, 0.0, 1 - 2 * C * C)
ZA, ZB, ZF = 4.0, 3.0, 0.5            # the mirror, the hit plane, the frame polygon that sets the bound's near face
BX0, BX1 = 1.2, 8.0                   # B's x extent (outside the camera's view, which is x in [-1, 1])
HIT_PIXEL, MISS_PIXEL = 5, 0
D_EXT, BG_EXT = 4.0, BG
D_INT, BG_INT = 0.5, (0.9, 0.1, 0.1)  # the other side, different, so a wrong side shows


def geometry(prim=False):
    b = [VIS,
         "AttributeBegin",
         '  Surface "volume_reflect_probe" "float tiltx" [%g]' % TILT,
         "@VOLUMES@",
         "  " + quad(ZA, -2, 2, -2, 2, (0, 0, -1)),
         "AttributeEnd",
         "AttributeBegin",
         "  Color %s" % col(CB2),
         '  Surface "constant"']
    if prim:
        b.append('  Attribute "shade" "string specularhitmode" ["primitive"]')
    b += ["  " + quad(ZB, BX0, BX1, -2, 2, (0, 0, -1)),
          "AttributeEnd",
          "AttributeBegin",
          '  Surface "constant"',
          "  " + quad(ZF, 7, 8, -1, 1, (0, 0, -1)),
          "AttributeEnd"]
    return b


def with_volumes(body, volumes):
    out = []
    for line in body:
        if line == "@VOLUMES@":
            out.extend(volumes)
        else:
            out.append(line)
    return out


def hit_length():
    return (ZA - ZB) / -R[2]


def miss_camera_length():
    x = pixel_centre(MISS_PIXEL)
    return exit_through_aabb((x, 0.0, ZA), R, (-2, -2, ZF), (8, 2, ZA))


def miss_world_prediction():
    """What the length would be if the bound's numbers were world-space numbers read as camera space."""
    pts = []
    for (x0, x1, y0, y1, z) in ((-2, 2, -2, 2, ZA), (BX0, BX1, -2, 2, ZB), (7, 8, -1, 1, ZF)):
        for x in (x0, x1):
            for y in (y0, y1):
                pts.append((x, y, z))
    bmin, bmax = world_aabb(pts)
    x = pixel_centre(MISS_PIXEL)
    return exit_through_aabb((x, 0.0, ZA), R, bmin, bmax)


def comment_common(extra):
    lc, lw = miss_camera_length(), miss_world_prediction()
    return [
        "Camera space: an orthographic camera looks along +z (roll and translation camera transform, not the identity).",
        "A mirror polygon at z = %g (N = -z) is traced from with a fixed tilted shading normal (volume_reflect_probe," % ZA,
        "tiltx %g), so a camera ray along +z goes to R = (%.4f, 0, %.4f), leaving into the exterior side (R.N > 0)." % (TILT, R[0], R[2]),
        "A target polygon at z = %g spans x in [%g, %g] (outside the view); a frame polygon at z = %g sets the" % (ZB, BX0, BX1, ZF),
        "world bound's near face. Pixel (%d,4) at x = %.3f: reflected x = %.3f, hits the target after %.4f." % (HIT_PIXEL, pixel_centre(HIT_PIXEL), pixel_centre(HIT_PIXEL) + R[0] / -R[2] * (ZA - ZB), hit_length()),
        "Pixel (%d,4) at x = %.3f: reflected x = %.3f < %g, misses the target and leaves the world bound through its" % (MISS_PIXEL, pixel_centre(MISS_PIXEL), pixel_centre(MISS_PIXEL) + R[0] / -R[2] * (ZA - ZB), BX0),
        "near face z = %g after %.4f in CAMERA space (bound read as camera space); read as WORLD-space numbers" % (ZF, lc),
        "the same ray would leave after %.4f (the probe of tasks T074/T075 tells which is true)." % lw,
    ] + extra


def make(family, vols, hit_values, miss_values, tag, prim=False, via=None, clipping="0.1 100", hit=True, miss=True, twin_tag=None):
    out = []
    for twin in (False, True):
        for suf, h, slo in variants(family):
            name = "%s%s%s" % (family, "-none" if twin else "", suf)
            body = with_volumes(geometry(prim), [] if twin else vols)
            if via:
                body = [b.replace('"float tiltx" [%g]' % TILT, '"float tiltx" [%g] "string via" ["%s"]' % (TILT, via)) for b in body]
            comment = ["Spec 056, family %s%s." % (family, "-none (the no-volume twin)" if twin else "")] + comment_common(
                ["Expected, RGB (tolerance %g for fog, %g for a pass-through colour):" % (TOL_FOG, TOL_COLOUR)])
            vh = CB2 if twin else hit_values
            vm = (0.0, 0.0, 0.0) if twin else miss_values
            comment.append("  hit pixel (%d,4): %s" % (HIT_PIXEL, tuple(round(v, 6) for v in vh)) if hit else "  (no hit pixel)")
            if miss:
                comment.append("  miss pixel (%d,4): %s" % (MISS_PIXEL, tuple(round(v, 6) for v in vm)))
            out.append(Scene(name, h, slo, body, comment, clipping=clipping))
            tol = TOL_COLOUR if twin else TOL_FOG
            if hit:
                probe(name, HIT_PIXEL, 4, tol, vh, tag + ("-none-hit" if twin else "-hit"))
            if miss:
                probe(name, MISS_PIXEL, 4, TOL_COLOUR if twin else TOL_FOG, vm, tag + ("-none-miss" if twin else "-miss"))
    return out


def scenes():
    out = []
    ext = ['  Exterior "fog" ' + fog(D_EXT, BG_EXT)]
    both = ext + ['  Interior "fog" ' + fog(D_INT, BG_INT)]
    lh, lm = hit_length(), miss_camera_length()
    hit_v = fogged(CB2, lh, D_EXT, BG_EXT)
    miss_v = fogged((0, 0, 0), lm, D_EXT, BG_EXT)
    out += make("volume-exterior-fog-reflection", ext, hit_v, miss_v, "reflection")
    out += make("volume-primitive-hitmode", ext, hit_v, miss_v, "primitive", prim=True, miss=False)
    out += make("volume-environment-raytrace", both, hit_v, miss_v, "environment", via="environment")
    # the camera-space probe of T074: far clipping 1e30, a miss only; the camera and world predictions differ by >= 0.05
    out += make("volume-miss-clipping", ext, hit_v, miss_v, "miss-clipping", clipping="0.1 1e30", hit=False)
    dv = max(abs(a - b) for a, b in zip(fogged((0, 0, 0), lm, D_EXT, BG_EXT), fogged((0, 0, 0), miss_world_prediction(), D_EXT, BG_EXT)))
    assert dv >= 0.05, "camera-space and world-space predictions of the miss differ by only %g" % dv
    return out
