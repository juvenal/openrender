#!/usr/bin/env python3
"""Render scenes of examples/rib/tests with the environment of a registered visual test (spec 056, T081, T100).

  tools/render_scenes.py --out <dir> <scene-name>...        (run through cpujob)

The environment (SHADERS, ORENDERHOME, DISPLAYS ...) is taken from `ctest --show-only=json-v1` for the test
Parity_volume-glass-sphere-reyes, so the renders are the ones the tests make. Each <scene>.tif is copied to <dir>.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.abspath(os.path.join(here, "..", "..", ".."))
build = os.path.join(root, "build")
args = sys.argv[1:]
out = args[args.index("--out") + 1]
scenes = [a for i, a in enumerate(args) if a != "--out" and (i == 0 or args[i - 1] != "--out")]
os.makedirs(out, exist_ok=True)
tests = json.loads(subprocess.run(["ctest", "--test-dir", build, "--show-only=json-v1", "-R", "^Parity_volume-glass-sphere-reyes$"],
                                  check=True, capture_output=True, text=True).stdout)["tests"]
env = dict(os.environ)
for p in tests[0]["properties"]:
    if p["name"] == "ENVIRONMENT":
        for kv in p["value"]:
            k, _, v = kv.partition("=")
            env[k] = v
orender = tests[0]["command"][1]
for s in scenes:
    scratch = tempfile.mkdtemp(prefix="render056-")
    r = subprocess.run([orender, os.path.join(root, "examples", "rib", "tests", s + ".rib")], cwd=scratch, env=env, capture_output=True, text=True)
    tif = os.path.join(scratch, s + ".tif")
    if r.returncode != 0 or not os.path.exists(tif):
        print("render failed: %s\n%s%s" % (s, r.stdout, r.stderr), file=sys.stderr)
        sys.exit(1)
    shutil.copyfile(tif, os.path.join(out, s + ".tif"))
    shutil.rmtree(scratch, ignore_errors=True)
    print("rendered", s)
