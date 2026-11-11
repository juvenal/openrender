"""volume-csg-fragment (US5, T035): CSG fragments obey the general rule; camera rays get no volume step."""
from common import *
import reflection as R

# A unit cube as a closed PointsPolygons mesh (the topology of csg-polymesh-box-difference-*.rib)
CUBE = ('PointsPolygons [4 4 4 4 4 4] [0 3 2 1  4 5 6 7  0 1 5 4  2 3 7 6  1 2 6 5  0 4 7 3] '
        '"P" [-0.5 -0.5 -0.5  0.5 -0.5 -0.5  0.5 0.5 -0.5  -0.5 0.5 -0.5  -0.5 -0.5 0.5  0.5 -0.5 0.5  0.5 0.5 0.5  -0.5 0.5 0.5]')

LEFT_PIXEL, RIGHT_PIXEL = 0, 7
CC = (0.3, 0.7, 0.5)    # the constant surface of the left solid


def solid(x0, x1, hole_x, constant):
    """A box [x0,x1] x [-1.5,1.5] x [4,6] minus a small box in front of its middle."""
    cx, sx = (x0 + x1) / 2, (x1 - x0)
    b = [
        "AttributeBegin",
        "  Color %s" % col(CC if constant else CB),
        '  Surface "constant"' if constant else '  Surface "volume_reflect_probe" "float tiltx" [%g]' % R.TILT,
        '  Interior "fog" ' + fog(0.5, (0.9, 0.1, 0.1)),
        '  Exterior "fog" ' + fog(R.D_EXT, R.BG_EXT),
        '  SolidBegin "difference"',
        '    SolidBegin "primitive"',
        "      TransformBegin",
        "        Translate %g 0 5" % cx,
        "        Scale %g 3 2" % sx,
        "        " + CUBE,
        "      TransformEnd",
        "    SolidEnd",
        '    SolidBegin "primitive"',
        "      TransformBegin",
        "        Translate %g 0 4" % hole_x,
        "        Scale 0.25 0.5 1",
        "        " + CUBE,
        "      TransformEnd",
        "    SolidEnd",
        "  SolidEnd",
        "AttributeEnd",
    ]
    return b


def scenes():
    out = []
    right_x = pixel_centre(RIGHT_PIXEL)
    refl_x = right_x + R.R[0] / -R.R[2] * (R.ZA - 3.0)
    want_hit = fogged(CB2, R.hit_length(), R.D_EXT, R.BG_EXT)
    for twin in (False, True):
        for suf, h, slo in variants("c"):
            name = "volume-csg-fragment%s%s" % ("-none" if twin else "", suf)
            body = [VIS]
            left = solid(-1.4, -0.3, -0.55, True)
            right = solid(0.3, 1.4, 0.55, False)
            if twin:
                left = [l for l in left if not l.strip().startswith(("Interior", "Exterior"))]
                right = [l for l in right if not l.strip().startswith(("Interior", "Exterior"))]
            body += left + right
            body += ["AttributeBegin", "  Color %s" % col(CB2), '  Surface "constant"', "  " + quad(3.0, 1.7, 8, -2, 2, (0, 0, -1)), "AttributeEnd"]
            comment = [
                "Spec 056, family volume-csg-fragment%s (FR-011, FR-014)." % ("-none (the no-volume twin)" if twin else ""),
                "Camera space (orthographic, roll + translate camera). Two CSG difference solids (a box minus a small box),",
                "each with Interior fog (distance 0.5, background red) and Exterior fog (distance %g, background %s)." % (R.D_EXT, R.BG_EXT),
                "Left solid (x in [-1.4, -0.3]), constant surface: pixel (%d,4) is a CAMERA ray on a CSG fragment. A camera ray has" % LEFT_PIXEL,
                "no spawning surface, so no volume step runs: the colour is the surface's, %s. (The arrival-side rule of the" % (CC,),
                "starting tree fogs it, so this pixel is red before Phase 3.)",
                "Right solid (x in [0.3, 1.4]), volume_reflect_probe: pixel (%d,4) at x = %.3f reflects to x = %.3f and hits the" % (RIGHT_PIXEL, right_x, refl_x),
                "polygon at z = 3 (x >= 1.7) after %.4f: the exterior fog applies to that segment, like any surface:" % R.hit_length(),
                "  %s (tolerance %g)." % (tuple(round(v, 6) for v in want_hit), TOL_FOG),
            ]
            out.append(Scene(name, h, slo, body, comment))
            tol = TOL_COLOUR if twin else TOL_FOG
            probe(name, LEFT_PIXEL, 4, TOL_COLOUR, CC, "csg-camera" + ("-none" if twin else ""))
            probe(name, RIGHT_PIXEL, 4, tol, CB2 if twin else want_hit, "csg-reflect" + ("-none" if twin else ""))
    return out
