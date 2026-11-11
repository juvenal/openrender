"""volume-incident-opposite (US7, T032): incident() and opposite() from both sides."""
from common import *

P_INT, P_EXT = 0.2, 0.9


def scenes():
    out = []
    for suf, h, slo in variants("i"):
        name = "volume-incident-opposite%s" % suf
        body = [
            VIS,
            "AttributeBegin",
            '  Interior "param_query_volume" "float testVolParam" [%g]' % P_INT,
            '  Exterior "param_query_volume" "float testVolParam" [%g]' % P_EXT,
            '  Surface "incident_opposite_probe"',
            "  " + quad(4.0, -1, 0, -1, 1, (0, 0, -1)),      # seen from outside
            "AttributeEnd",
            "AttributeBegin",
            '  Surface "volume_through_probe"',
            "  " + quad(4.0, 0, 1, -1, 1, (0, 0, -1)),       # looks through itself to the probe behind
            "AttributeEnd",
            "AttributeBegin",
            '  Interior "param_query_volume" "float testVolParam" [%g]' % P_INT,
            '  Exterior "param_query_volume" "float testVolParam" [%g]' % P_EXT,
            '  Surface "incident_opposite_probe"',
            "  " + quad(6.0, 0, 1, -1, 1, (0, 0, 1)),        # N = +z: the camera ray arrives from the interior side
            "AttributeEnd",
        ]
        comment = [
            "Spec 056, family volume-incident-opposite.",
            "Camera space (orthographic, roll + translate camera). incident_opposite_probe writes (incident(), opposite()) of",
            "testVolParam into Ci. Interior = param_query_volume with testVolParam %g, Exterior = the same with %g." % (P_INT, P_EXT),
            "Pixel (2,4): the left polygon faces the camera (N = -z), the ray arrives from the EXTERIOR side:",
            "  incident() reads Exterior, opposite() reads Interior -> (%g, %g, 0)." % (P_EXT, P_INT),
            "Pixel (5,4): the right polygon traces straight through itself to a probe at z = 6 with N = +z; the ray arrives",
            "from the INTERIOR side: incident() reads Interior, opposite() Exterior -> (%g, %g, 0)." % (P_INT, P_EXT),
            "On the starting tree incident() always reads Interior: pixel (2,4) is red, pixel (5,4) passes by coincidence.",
        ]
        out.append(Scene(name, h, slo, body, comment))
        probe(name, 2, 4, TOL_COLOUR, (P_EXT, P_INT, 0.0), "incident-outside")
        probe(name, 5, 4, TOL_COLOUR, (P_INT, P_EXT, 0.0), "incident-inside")
    return out
