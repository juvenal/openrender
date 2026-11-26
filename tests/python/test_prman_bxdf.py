# Spec 035 (closure tree and BSDF), Phase 4 (T093), Bxdf_Bindings_python (SC-008): the Python RIB helper writes the Bxdf statement as the
# RIB hand-written statement reads and as ribOut writes it: `Bxdf "name" "handle" <parameters>`, the handle omitted when None, and its place
# relative to Surface kept (the call order is the file order, so a Surface/Bxdf order survives).
#
# New file; the approved tests/python/test_prman.py stays as it is. Registered in CTest (label `helpers`, `bxdf`) behind find_program(Python3). The
# helper directory comes from the environment (PRMAN_PYTHON_DIR, default src/python under the current directory) and the RIB files are written
# to the working directory and removed afterwards.
#
# Red until Phase 4's helper task: prman.Ri has no Bxdf method (AttributeError).
import os
import sys

sys.path.append(os.environ.get("PRMAN_PYTHON_DIR", os.path.join(os.getcwd(), "src/python")))

import prman


def read_back(path):
    with open(path, "r") as f:
        content = f.read()
    os.remove(path)
    return content


def test_bxdf_statements():
    target = "bxdf_helper.rib"
    ri = prman.Ri()
    ri.Begin(target)
    ri.Bxdf("diffuse")
    ri.Bxdf("plastic", "h1")
    ri.Bxdf("plastic", None, {"float Kd": 0.25})
    ri.Bxdf("conductor", "m2", {"float Ks": 0.5})
    ri.Bxdf("diffuse", None, {"string note": "x"})
    ri.Surface("matte")
    ri.Bxdf("dielectric")
    ri.End()
    content = read_back(target)
    print("--- Generated RIB ---")
    print(content)
    print("---------------------")

    lines = [l.strip() for l in content.splitlines()]
    assert 'Bxdf "diffuse"' in lines, lines
    assert 'Bxdf "plastic" "h1"' in lines, lines
    assert 'Bxdf "plastic" "float Kd" [0.25]' in lines, lines
    assert 'Bxdf "conductor" "m2" "float Ks" [0.5]' in lines, lines
    # a bare-string first parameter is bracketed, so the grammar does not read it as a handle
    assert 'Bxdf "diffuse" "string note" ["x"]' in lines, lines
    # the place relative to Surface is the call order
    assert lines.index('Surface "matte"') < lines.index('Bxdf "dielectric"'), lines
    assert lines.index('Bxdf "diffuse"') < lines.index('Surface "matte"'), lines


def test_existing_statements_unchanged():
    target = "bxdf_helper_surface.rib"
    ri = prman.Ri()
    ri.Begin(target)
    ri.Surface("matte")
    ri.LightSource("distantlight")
    ri.End()
    content = read_back(target)
    assert 'Surface "matte"' in content
    assert 'LightSource "distantlight" 1' in content


if __name__ == "__main__":
    test_bxdf_statements()
    test_existing_statements_unchanged()
    print("test_prman_bxdf: ok")
