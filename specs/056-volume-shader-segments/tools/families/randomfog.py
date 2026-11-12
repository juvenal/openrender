"""volume-random-fog (US8, T033): a volume shader that draws random() numbers, raytrace only."""
from common import *
import reflection as R


def scenes():
    out = []
    for twin in (False, True):
        for slo in ((False, True) if not twin else (False,)):
            name = "volume-random-fog%s-raytrace%s" % ("-none" if twin else "", "-slo" if slo else "")
            body = [
                VIS,
                "AttributeBegin",
                '  Surface "volume_reflect_probe" "float tiltx" [%g]' % R.TILT,
                "" if twin else '  Exterior "volume_random_fog" "float distance" [%g] "color background" [%g %g %g] "float jitter" [0.8]' % ((R.D_EXT,) + R.BG_EXT),
                "  " + quad(R.ZA, -2, 2, -2, 2, (0, 0, -1)),
                "AttributeEnd",
                "AttributeBegin",
                "  Color %s" % col(CB2),
                '  Surface "constant"',
                "  " + quad(R.ZB, R.BX0, R.BX1, -2, 2, (0, 0, -1)),
                "AttributeEnd",
                "AttributeBegin",
                '  Surface "constant"',
                "  " + quad(R.ZF, 7, 8, -1, 1, (0, 0, -1)),
                "AttributeEnd",
            ]
            body = [b for b in body if b != ""]
            comment = [
                "Spec 056, family volume-random-fog%s (raytrace only)." % ("-none (the no-volume twin)" if twin else ""),
                "The geometry of volume-exterior-fog-reflection at 16 x 16 pixels; the Exterior shader draws random() per point, so",
                "the image depends on each ray's sample stream. The determinism test renders it at -t:1, -t:2 and -t:4 and requires",
                "the same bytes; VolumeProbe_random-fog-runs requires it to differ from its twin, so a green determinism test means",
                "the fog ran. The scene has no analytic value.",
            ]
            out.append(Scene(name, "raytrace", slo, body, comment, fmt=(16, 16)))
    return out
