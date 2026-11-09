#!/usr/bin/env python3
"""Spec 056 golden capture (tasks.md T045): the 19 goldens of the unchanged starting tree.

For every registered Golden_atmosphere-* and Golden_volume-none-* test, runs the test's own command (taken from
`ctest --show-only=json-v1`, so the environment and paths are exactly the test's) with --write, THREE times into scratch
directories, requires the three renders byte-identical, and only then writes the golden to the path the test reads
(tests/visual/golden/ on macOS, tests/visual/golden/linux/ on Linux).

  tools/capture_goldens.sh --platform macos|linux [--build <dir>]

Run through cpujob: ~/.claude/bin/cpujob 056-p1-golden tools/capture_goldens.sh --platform macos
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile


def main():
    args = sys.argv[1:]
    platform = args[args.index("--platform") + 1] if "--platform" in args else None
    if platform not in ("macos", "linux"):
        print("usage: capture_goldens.sh --platform macos|linux [--build <dir>]", file=sys.stderr)
        return 2
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, "..", "..", ".."))
    build = args[args.index("--build") + 1] if "--build" in args else os.path.join(root, "build")

    listing = subprocess.run(["ctest", "--test-dir", build, "--show-only=json-v1", "-R", "^Golden_(atmosphere|volume-none)-"],
                             check=True, capture_output=True, text=True).stdout
    tests = json.loads(listing)["tests"]
    if len(tests) != 19:
        print("capture_goldens: expected 19 tests, found %d" % len(tests), file=sys.stderr)
        return 1

    for t in sorted(tests, key=lambda x: x["name"]):
        cmd = list(t["command"])
        golden = cmd[4]    # test_golden_compare <orender> <rib> <out.tif> <golden.tif>
        assert cmd[0].endswith("test_golden_compare") and golden.endswith(".tif"), cmd
        want_dir = "golden/linux" if platform == "linux" else "golden"
        if os.path.dirname(golden).replace(os.sep, "/").split("tests/visual/")[-1] != want_dir:
            print("capture_goldens: %s reads %s, not tests/visual/%s" % (t["name"], golden, want_dir), file=sys.stderr)
            return 1
        env = dict(os.environ)
        for p in t.get("properties", []):
            if p["name"] == "ENVIRONMENT":
                for kv in p["value"]:
                    k, _, v = kv.partition("=")
                    env[k] = v
        digests, renders = [], []
        for run in range(3):
            scratch = tempfile.mkdtemp(prefix="golden056-")
            out = os.path.join(scratch, "golden.tif")
            run_cmd = cmd[:4] + [out, "--write"]
            r = subprocess.run(run_cmd, cwd=scratch, env=env, capture_output=True, text=True)
            if r.returncode != 0 or not os.path.exists(out):
                print("capture_goldens: %s run %d failed:\n%s%s" % (t["name"], run, r.stdout, r.stderr), file=sys.stderr)
                return 1
            data = open(out, "rb").read()
            digests.append(hashlib.sha256(data).hexdigest())
            renders.append(out)
        if len(set(digests)) != 1:
            print("capture_goldens: %s: the three renders differ: %s" % (t["name"], digests), file=sys.stderr)
            return 1
        os.makedirs(os.path.dirname(golden), exist_ok=True)
        shutil.copyfile(renders[0], golden)
        print("%-62s %s" % (t["name"], digests[0]))
        for p in renders:
            shutil.rmtree(os.path.dirname(p), ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
