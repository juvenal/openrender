"""volume-interior-fog-slab (US1, T023): a slab of thickness L between two parallel polygons, Interior fog on the front one."""
from common import *

L = 2.5
Z0 = 4.0


def scenes():
    out = []
    want = fogged(CB, L)
    for twin in (False, True):
        for suf, h, slo in variants("slab"):
            name = "volume-interior-fog-slab%s%s" % ("-none" if twin else "", suf)
            body = [
                VIS,
                "AttributeBegin",
                "  Surface \"volume_through_probe\"",
                ("" if twin else "  Interior \"fog\" " + fog()),
                "  " + quad(Z0, -3, 3, -3, 3, (0, 0, -1)),
                "AttributeEnd",
                "AttributeBegin",
                "  Color %s" % col(CB),
                "  Surface \"constant\"",
                "  " + quad(Z0 + L, -4, 4, -4, 4, (0, 0, 1)),
                "AttributeEnd",
            ]
            body = [b for b in body if b != ""]
            comment = [
                "Spec 056 (volume shader segments), family volume-interior-fog-slab%s." % ("-none (the no-volume twin)" if twin else ""),
                "Camera space: an orthographic camera looks along +z (the camera transform is a roll and a translation, not the",
                "identity). A front polygon at z = %g faces the camera (N = -z) and traces straight through itself" % Z0,
                "(volume_through_probe) to a constant-colour back polygon at z = %g, so every segment is %g long." % (Z0 + L, L),
                "Probe pixel (4,4), RGB:",
                ("  no Interior: the backdrop colour %s" % (CB,)) if twin else
                ("  Interior fog distance %g background %s: Cb*T + bg*d, T = exp(-%g/%g) -> %s" % (DIST, BG, L, DIST, tuple(round(v, 6) for v in want))),
            ]
            out.append(Scene(name, h, slo, body, comment))
            if twin:
                probe(name, 4, 4, TOL_COLOUR, CB, "slab-none")
            else:
                probe(name, 4, 4, TOL_FOG, want, "slab")
    return out
