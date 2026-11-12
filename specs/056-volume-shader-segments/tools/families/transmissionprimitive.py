"""volume-transmission-primitive-hitmode (US2/US3, FR-005, tasks.md T136): transmission() through a wall read in primitive hit mode.

The scene of volume-transmission-interior with one change: the half-transparent wall carries
Attribute "shade" "string transmissionhitmode" ["primitive"], so the shadow hit is not shaded: shade() copies the
attribute opacity into Oi and returns (the shadow-primitive branch), and the volume step still has to attenuate the segment
(FR-005: the hit mode decides how the hit gets its colour, not whether the segment is attenuated).
"""
from common import *

ZR, ZW, DZ = 4.0, 5.0, 1000.0   # receiver, the half-transparent wall behind it, the transmission reach
D_FOG = 1000.0                  # as volume-transmission-interior: long reach and distance, so the jitter of the sampled ray is negligible
OPAC = 0.5


def scenes():
    out = []
    want_t = OPAC * math.exp(-(DZ - BIAS) / D_FOG)   # (1 - O) * exp(-(DZ - bias)/D): the same as the shaded wall's
    for twin in (False, True):
        for suf, h, slo in variants("t"):
            name = "volume-transmission-primitive-hitmode%s%s" % ("-none" if twin else "", suf)
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
                '  Attribute "shade" "string transmissionhitmode" ["primitive"]',
                "  " + quad(ZW, -4, 4, -4, 4, (0, 0, -1)),
                "AttributeEnd",
            ]
            body = [b for b in body if b != ""]
            v = (OPAC,) * 3 if twin else (want_t,) * 3
            comment = [
                "Spec 056, family volume-transmission-primitive-hitmode%s (FR-005, tasks.md T136)." % ("-none (the no-volume twin)" if twin else ""),
                "The scene of volume-transmission-interior with the wall read in primitive hit mode: transmissionhitmode \"primitive\" makes the",
                "shadow hit copy the attribute opacity (%g) into Oi without running the wall's shader (shade(), the shadow-primitive branch)." % OPAC,
                "The receiver at z = %g (N = -z) calls transmission() toward P + (0, 0, %g): the ray goes into the interior side," % (ZR, DZ),
                "crosses the wall at z = %g (opacity %g, transparent: it must not switch the shader, A5) and ends at z = %g," % (ZW, OPAC, ZR + DZ),
                "less the trace bias (%g, pinned below). The segment is attenuated all the same: with the over-form of fog the" % BIAS,
                "transmitted fraction is (1 - %g) * exp(-(%g - %g)/%g) = %g, the value of the shaded wall." % (OPAC, DZ, BIAS, D_FOG, want_t),
                "The twin has no Interior: %g. Nothing lies beyond the destination (GitHub #153); the wall is not a shadow hit in shader" % OPAC,
                "mode, so GitHub #148's nesting is not reached.",
                "Probe pixel (4,4), RGB (grey): %s (tolerance %g)." % (tuple(round(x, 6) for x in v), TOL_COLOUR if twin else TOL_FOG),
            ]
            out.append(Scene(name, h, slo, body, comment))
            probe(name, 4, 4, TOL_COLOUR if twin else TOL_FOG, v, "transmission-primitive-hitmode" + ("-none" if twin else ""))
    return out
