"""volume-gather (US4, T030): gather() of a ray that hits, Exterior fog."""
from common import *
import reflection as R

def scenes():
    out = []
    lh = R.hit_length()
    hit_v = fogged(CB2, lh, R.D_EXT, R.BG_EXT)
    for twin in (False, True):
        for suf, h, slo in variants("g"):
            name = "volume-gather%s%s" % ("-none" if twin else "", suf)
            body = [
                VIS,
                "AttributeBegin",
                '  Surface "volume_gather_probe" "float tiltx" [%g]' % R.TILT,
                "" if twin else '  Exterior "fog" ' + fog(R.D_EXT, R.BG_EXT),
                "  " + quad(R.ZA, -2, 2, -2, 2, (0, 0, -1)),
                "AttributeEnd",
                "AttributeBegin",
                "  Color %s" % col(CB2),
                '  Surface "constant"',
                "  " + quad(R.ZB, R.BX0, R.BX1, -2, 2, (0, 0, -1)),
                "AttributeEnd",
            ]
            body = [b for b in body if b != ""]
            v = CB2 if twin else hit_v
            comment = [
                "Spec 056, family volume-gather%s." % ("-none (the no-volume twin)" if twin else ""),
                "As volume-exterior-fog-reflection (same mirror, same target, same tilted direction), but the surface gathers one",
                "ray along the reflected direction (a cone of angle 0, so the direction is exact) and averages surface:Ci. The",
                "gather point is enclosed along that direction: every ray of the probed pixel hits the target after %.4f, so the" % lh,
                "else branch never runs. Exterior fog (distance %g, background %s) attenuates the hit's Ci before it is recorded." % (R.D_EXT, R.BG_EXT),
                "Probe pixel (%d,4), RGB: %s (tolerance %g)." % (R.HIT_PIXEL, tuple(round(x, 6) for x in v), TOL_COLOUR if twin else TOL_FOG),
            ]
            out.append(Scene(name, h, slo, body, comment))
            probe(name, R.HIT_PIXEL, 4, TOL_COLOUR if twin else TOL_FOG, v, "gather" + ("-none" if twin else ""))
    return out
