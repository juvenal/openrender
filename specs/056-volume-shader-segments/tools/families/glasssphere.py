"""volume-interior-fog-glass-sphere (US1, T024): FR-019's literal scene, a non-CSG glass sphere with Interior fog, 8-bit."""
from common import *


def scenes():
    out = []
    # family, 8-bit?  The 8-bit scenes are the parity and reference subjects; test_render_compare (the differ tests) and
    # test_pixel_probe read 32-bit float TIFFs only, so each hider also has a small float pair for VolumeDiffer_*.
    for family, floats in (("volume-interior-fog-glass-sphere", False), ("volume-interior-fog-glass-sphere-float", True)):
      for twin in (False, True):
        for suf, h, slo in variants("glass"):
            if twin and slo:
                continue   # the twins are interpreter scenes: the differ tests compare against them
            if floats and slo:
                continue   # the float pair is a differ subject, rslo only
            name = "%s%s%s" % (family, "-none" if twin else "", suf)
            body = [
                'LightSource "ambientlight" 1 "float intensity" [0.3]',
                'LightSource "distantlight" 2 "float intensity" [0.9] "point from" [5 5 -5] "point to" [0 0 0]',
                VIS,
                "AttributeBegin",
                "  Color [0.8 0.8 0.9]",
                '  Surface "matte"',
                "  Polygon \"P\" [-6 -6 9  6 -6 9  6 6 9  -6 6 9] \"N\" [0 0 -1 0 0 -1 0 0 -1 0 0 -1]",
                "AttributeEnd",
                "AttributeBegin",
                "  Color [1 1 1]",
                '  Surface "glass"',
                "" if twin else '  Interior "fog" "float distance" [0.8] "color background" [0.9 0.1 0.1]',
                "  Translate 0 0 5",
                "  Sphere 1.5 -1.5 1.5 360",
                "AttributeEnd",
            ]
            body = [b for b in body if b != ""]
            comment = [
                "Spec 056, family volume-interior-fog-glass-sphere%s." % ("-none (the no-volume twin)" if twin else ""),
                "A non-CSG glass sphere (radius 1.5) in front of a lit backdrop, 8-bit output. With Interior fog (distance 0.8,",
                "background red) the refracted view through the sphere shifts toward red with the path length; the chord varies",
                "across a pixel, so there is no closed form. Checked by VolumeDiffer_glass-sphere-<hider> (the image differs from",
                "its twin), by Parity_volume-glass-sphere-* (cross-hider and cross-backend agreement) and, after Phase 2, by a",
                "reference image approved by the maintainer.",
            ]
            if slo:
                comment.append("UNREGISTERED until GitHub #157 (the shipped glass shader is broken under the JIT: it disagrees with its .rslo and is")
                comment.append("not deterministic across thread counts): no parity, sweep or visual test uses this scene; the #157 fix re-adds them.")
            out.append(Scene(name, h, slo, body, comment, fmt=(48, 48) if floats else (96, 96), floats=floats, projection="perspective",
                             clipping="0.1 100", camera=False, pixel_samples="1 1" if floats else "2 2"))
    return out
