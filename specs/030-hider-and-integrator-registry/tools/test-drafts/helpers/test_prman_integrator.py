# Spec 030 (hider and integrator registry), FR-017: the Python RIB helper writes the Integrator statement.
#
# New file; the approved tests/python/test_prman.py stays as it is. Registered in CTest (label `helpers`) behind
# find_program(Python3); the helper directory comes from the environment (PRMAN_PYTHON_DIR, default src/python under
# the current directory) and the RIB files are written to the working directory and removed afterwards.
#
# Red until Phase 7: prman.Ri has no Integrator method (AttributeError).
import os
import sys

sys.path.append(os.environ.get("PRMAN_PYTHON_DIR", os.path.join(os.getcwd(), "src/python")))

import prman


def read_back(path):
    with open(path, "r") as f:
        content = f.read()
    os.remove(path)
    return content


def test_integrator_statements():
    target = "integrator_helper.rib"
    ri = prman.Ri()
    ri.Begin(target)
    ri.Integrator("classic")
    ri.Integrator("classic", {"int maxdepth": 5})
    ri.Integrator("classic", {"string mode": "fast"})
    ri.Integrator("classic", {"float gain": 0.5, "int maxdepth": 5})
    ri.Hider("reyes")
    ri.End()
    content = read_back(target)
    print("--- Generated RIB ---")
    print(content)
    print("---------------------")

    lines = [l.strip() for l in content.splitlines()]
    assert 'Integrator "classic"' in lines, lines
    assert 'Integrator "classic" "int maxdepth" [5]' in lines, lines
    assert 'Integrator "classic" "string mode" ["fast"]' in lines, lines
    assert 'Integrator "classic" "float gain" [0.5] "int maxdepth" [5]' in lines, lines
    # the statement is written beside Hider and does not change it
    assert 'Hider "reyes"' in lines, lines
    assert lines.index('Hider "reyes"') > lines.index('Integrator "classic"'), lines


def test_existing_hider_unchanged():
    target = "integrator_helper_hider.rib"
    ri = prman.Ri()
    ri.Begin(target)
    ri.Hider("zbuffer")
    ri.End()
    content = read_back(target)
    assert 'Hider "zbuffer"' in content


if __name__ == "__main__":
    test_integrator_statements()
    test_existing_hider_unchanged()
    print("test_prman_integrator: ok")
