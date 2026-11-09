"""volume-transmission-interior (US3, T027): transmission() from a receiver through a clear-ish polygon to a point."""
from common import *

ZR, ZW, DZ = 4.0, 5.0, 1000.0   # receiver, the half-transparent wall behind it, the transmission reach
D_FOG = 1000.0                  # the fog distance: reach and distance are long so the ray differentials'' jitter of the
                                # sampled ray (a fraction of a pixel) changes the segment length by less than 1e-7 relative
OPAC = 0.5


def scenes():
    out = []
    want_t = OPAC * math.exp(-(DZ - BIAS) / D_FOG)   # (1 - O) * T1 * T2 = (1 - O) * exp(-(DZ - bias)/D), identical for any split
    for twin in (False, True):
        for suf, h, slo in variants("t"):
            name = "volume-transmission-interior%s%s" % ("-none" if twin else "", suf)
            body = [
                VIS,
                BIAS_LINE,
                "AttributeBegin",
                '  Surface "volume_transmission_probe" "float dz" [%g]' % DZ,
                "" if twin else '  Interior "fog" ' + fog(D_FOG),
                "  " + quad(ZR, -3, 3, -3, 3, (0, 0, -1)),
                "AttributeEnd",
                "AttributeBegin",
                "  Color [1 1 1]",
                "  Opacity [%g %g %g]" % (OPAC, OPAC, OPAC),
                '  Surface "constant"',
                "  " + quad(ZW, -4, 4, -4, 4, (0, 0, -1)),
                "AttributeEnd",
            ]
            body = [b for b in body if b != ""]
            v = (OPAC,) * 3 if twin else (want_t,) * 3
            comment = [
                "Spec 056, family volume-transmission-interior%s." % ("-none (the no-volume twin)" if twin else ""),
                "Camera space (orthographic, roll + translate camera). A receiver at z = %g faces the camera (N = -z) and calls" % ZR,
                "transmission() toward P + (0, 0, %g): the ray goes into the interior side (dot(+z, N) < 0), crosses a wall at" % DZ,
                "z = %g with opacity %g (transparent: it must NOT switch the shader, A5) and ends at its destination at" % (ZW, OPAC),
                "z = %g, less the trace bias (%g, pinned below): the ray's initial t is d - bias (executeMisc.cpp:779)." % (ZR + DZ, BIAS),
                "Reach and fog distance are %g, so the sampled ray's jitter (a fraction of a pixel, from the ray differentials)" % DZ,
                "does not change the length measurably. With the over-form of fog the transmitted fraction is",
                "(1 - %g) * T1 * T2 = %g * exp(-(%g - %g)/%g). Nothing lies beyond the destination (GitHub #153)." % (OPAC, 1 - OPAC, DZ, BIAS, D_FOG),
                "Probe pixel (4,4), RGB (grey): %s (tolerance %g)." % (tuple(round(x, 6) for x in v), TOL_COLOUR if twin else TOL_FOG),
            ]
            out.append(Scene(name, h, slo, body, comment))
            probe(name, 4, 4, TOL_COLOUR if twin else TOL_FOG, v, "transmission" + ("-none" if twin else ""))
    return out
