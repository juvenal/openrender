"""volume-shadow-light (US3, T028): a ray-traced shadow() in a light that rewrites N, with different Interior and Exterior."""
from common import *

REACH = 1000.0
D_EXT, BG_EXT = 1000.0, BG   # long reach and distance: see volume-transmission-interior
D_INT, BG_INT = 0.5, (0.9, 0.1, 0.1)


def scenes():
    out = []
    t_ext = math.exp(-(REACH - BIAS) / D_EXT)
    for twin in (False, True):
        for suf, h, slo in variants("s"):
            name = "volume-shadow-light%s%s" % ("-none" if twin else "", suf)
            body = [
                VIS,
                BIAS_LINE,
                'LightSource "volume_shadow_light" 1 "float intensity" [1] "float reach" [%g]' % REACH,
                "AttributeBegin",
                "  Color %s" % col(CB),
                '  Surface "matte" "float Ka" [0] "float Kd" [1]',
                "" if twin else '  Interior "fog" ' + fog(D_INT, BG_INT),
                "" if twin else '  Exterior "fog" ' + fog(D_EXT, BG_EXT),
                "  " + quad(4.0, -3, 3, -3, 3, (0, 0, -1)),
                "AttributeEnd",
            ]
            body = [b for b in body if b != ""]
            v = CB if twin else tuple(CB[k] * t_ext for k in range(3))
            comment = [
                "Spec 056, family volume-shadow-light%s." % ("-none (the no-volume twin)" if twin else ""),
                "Camera space (orthographic, roll + translate camera). A matte receiver at z = 4 faces the camera (N = -z) and is",
                "lit by volume_shadow_light, a light %g units behind each point along -z; its ray-traced shadow() ray runs toward" % REACH,
                "the light, i.e. along -z: dot(R, N) > 0, the EXTERIOR side. The light overwrites N with -N before it asks for the",
                "shadow, so a step that read the light's N would pick the Interior shader instead; the snapshot of the surface's N",
                "(research R7) picks the Exterior. Interior = fog distance %g background %s, Exterior = fog distance %g" % (D_INT, BG_INT, D_EXT),
                "background %s. Nothing occludes the ray: the segment ends at the light, %g less the trace bias %g (pinned" % (BG_EXT, REACH, BIAS),
                "below) long; the light's visibility is 1 - shadow = exp(-(%g - %g)/%g) = %.6f;" % (REACH, BIAS, D_EXT, t_ext),
                "matte: Ci = Cb * visibility (Ka = 0, Kd = 1, N.L = 1).",
                "Probe pixel (4,4), RGB: %s (tolerance %g)." % (tuple(round(x, 6) for x in v), TOL_COLOUR if twin else TOL_FOG),
            ]
            out.append(Scene(name, h, slo, body, comment))
            probe(name, 4, 4, TOL_COLOUR if twin else TOL_FOG, v, "shadow" + ("-none" if twin else ""))
    return out
