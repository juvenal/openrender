"""atmosphere-fog, atmosphere-depthcue (US6, T034): Atmosphere on camera rays, characterised on the starting tree."""
from common import *

FOG = 'Atmosphere "fog" "float distance" [6] "color background" [0.7 0.8 1]'
DEPTH = 'Atmosphere "depthcue" "float mindistance" [3] "float maxdistance" [10] "color background" [0.7 0.8 1]'


def geometry():
    return [
        "AttributeBegin",
        "  Color %s" % col(CB),
        '  Surface "constant"',
        '  Polygon "P" [-12 -12 9  12 -12 9  12 12 9  -12 12 9] "N" [0 0 -1 0 0 -1 0 0 -1 0 0 -1]',
        "AttributeEnd",
        "AttributeBegin",
        "  Color %s" % col(CB2),
        '  Surface "constant"',
        "  Translate 0.4 0 5",
        "  Sphere 1.5 -1.5 1.5 360",
        "AttributeEnd",
    ]


def scene(name, h, slo, atmo, kind, floats, fmt):
    comment = [
        "Spec 056 (US6), family %s: %s on camera rays only." % (kind, kind),
        "A constant-coloured sphere (radius 1.5) in front of a constant-coloured wall, a perspective camera, %s." % atmo,
        "No RIB in the repository used Atmosphere before this one: it characterises the starting tree. The float scenes are",
        "compared bit for bit with a golden image captured on the unchanged starting tree (test_golden_compare, per platform);",
        "the 8-bit scenes are the cross-hider and cross-backend parity pairs of US6 AS1.",
    ]
    return Scene(name, h, slo, [VIS] + geometry(), comment, fmt=fmt, floats=floats, projection="perspective",
                 camera=False, clipping="0.1 100", world_extra=[atmo])


def scenes():
    out = []
    for kind, atmo in (("fog", FOG), ("depthcue", DEPTH)):
        for suf, h, slo in variants(kind):
            out.append(scene("atmosphere-%s%s" % (kind, suf), h, slo, atmo, kind, True, (32, 32)))
        for h in HIDERS:
            for slo in (False, True) if h == "raytrace" else (False,):
                out.append(scene("atmosphere-%s-8bit-%s%s" % (kind, h, "-slo" if slo else ""), h, slo, atmo, kind, False, (64, 64)))
    out += reflection_scenes()
    return out


# --- the reflection scene: no fog in a reflected hit ---
FOG_D = 6.0
FOG_BG = (0.7, 0.8, 1.0)


def reflection_scenes():
    """A reflective surface in the same scene; the pixel on its reflection holds the reflected colour fogged ONCE, at
    the mirror's own camera hit (length(I) = 4 for the central ray of a perspective camera), never at the reflected hit."""
    out = []
    import reflection as R
    d = 1 - math.exp(-R.ZA / FOG_D)
    want = tuple(CB2[k] * (1 - d) + FOG_BG[k] * d for k in range(3))
    for slo in (False, True):
        name = "atmosphere-fog-reflection-raytrace%s" % ("-slo" if slo else "")
        body = [
            VIS,
            "AttributeBegin",
            '  Surface "volume_reflect_probe" "float tiltx" [%g]' % R.TILT,
            "  " + quad(R.ZA, -4, 4, -4, 4, (0, 0, -1)),
            "AttributeEnd",
            "AttributeBegin",
            "  Color %s" % col(CB2),
            '  Surface "constant"',
            "  " + quad(R.ZB, R.BX0, R.BX1, -4, 4, (0, 0, -1)),
            "AttributeEnd",
        ]
        comment = [
            "Spec 056 (US6 AS2), family atmosphere-fog-reflection-raytrace.",
            "A mirror polygon at z = %g (volume_reflect_probe, tilt %g) in an Atmosphere fog scene, perspective camera. The" % (R.ZA, R.TILT),
            "central pixel of the 9 x 9 image looks along +z: its camera hit at the mirror is fogged once, with length(I) = %g," % R.ZA,
            "d = 1 - exp(-%g/%g) = %.6f; the reflected ray hits a constant-colour polygon, which must NOT be fogged again." % (R.ZA, FOG_D, d),
            "Expected RGB at pixel (4,4): C2*(1 - d) + bg*d = %s (tolerance %g)." % (tuple(round(v, 6) for v in want), TOL_FOG),
        ]
        out.append(Scene(name, "raytrace", slo, body, comment, fmt=(9, 9), projection="perspective", camera=False,
                         clipping="0.1 100", world_extra=['Atmosphere "fog" "float distance" [%g] "color background" [%g %g %g]' % ((FOG_D,) + FOG_BG)]))
        probe(name, 4, 4, TOL_FOG, want, "atmosphere-fog-reflection")
    return out
