#!/usr/bin/env bash
# timing_sc010.sh: SC-010, the render time of message-passing and coordinate-system heavy scenes before and after
# spec 026 (spec 028's section 0.5 method: interleaved, because the fanless M2's wall time follows machine load and
# its thermal state as much as the code).
#
#   timing_sc010.sh <starting-tree-build-dir> <finished-tree-build-dir> [series]
#
# Run it as ONE job under the lock:  ~/.claude/bin/cpujob 026-sc010 specs/026-shading-contract-v2/tools/timing_sc010.sh build-base build
#
# For each scene it runs the starting tree's orender and the finished tree's orender alternately, `series` times
# (default 5), at the same thread count (THREADS, default 4), from a scratch directory, with the environment the
# Visual_* tests use for that tree. The RIB is the finished tree's, for both. It prints per scene and per tree the
# median, min, max and spread (max - min), and the difference of the medians. A difference of the medians beyond the
# larger of the two spreads is reported as BEYOND SPREAD: that is a defect to report, not a threshold to move.
# Exit 0 when every scene is within spread (or faster), 1 when a scene is slower beyond the spread.

set -u
BASE_BUILD="${1:?usage: timing_sc010.sh <starting-tree-build-dir> <finished-tree-build-dir> [series]}"
FINAL_BUILD="${2:?usage: timing_sc010.sh <starting-tree-build-dir> <finished-tree-build-dir> [series]}"
SERIES="${3:-5}"
THREADS="${THREADS:-4}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
export ROOT BASE_BUILD FINAL_BUILD SERIES THREADS

# message passing (surface/displacement/atmosphere/incident queries, attribute(), option()), coordinate systems
# (the wood families transform into shader and named spaces), gather (the tracer interface), and the JIT halves
SCENES="teapot-somewood-reyes teapot-somewood-raytrace teapot-wood-reyes teapot-blue_marble-reyes teapot-somewood-reyes-slo sphere-gather-reyes"
export SCENES

exec python3 - <<'EOF'
import os, statistics, subprocess, sys, tempfile, time

root = os.environ["ROOT"]
series = int(os.environ["SERIES"])
threads = os.environ["THREADS"]
scenes = os.environ["SCENES"].split()
trees = {"start": os.path.abspath(os.environ["BASE_BUILD"]), "final": os.path.abspath(os.environ["FINAL_BUILD"])}

def display_path(b):
    parts = []
    for d in ("file", "rgbe", "framebuffer", "openexr"):
        p = os.path.join(b, "src", "display", d)
        if os.path.isdir(p):
            parts.append(p)
    return ":".join(parts)

def env_for(b):
    e = dict(os.environ)
    e.update({
        "SHADERS": os.path.join(b, "shaders"),
        "ORENDERHOME": os.path.join(b, "tests", "visual", "_home"),
        "DISPLAYS": display_path(b),
        "GEOMETRIES": os.path.join(root, "geometry"),
        "TEXTURES": os.path.join(root, "examples", "rib", "tests"),
    })
    return e

for name, b in trees.items():
    exe = os.path.join(b, "src", "orender", "orender")
    if not os.path.exists(exe):
        sys.exit("missing %s (%s tree)" % (exe, name))

scratch = tempfile.mkdtemp(prefix="sc010-")
times = {(s, t): [] for s in scenes for t in trees}
for i in range(series):
    for s in scenes:
        rib = os.path.join(root, "examples", "rib", "tests", s + ".rib")
        for name in (("start", "final") if i % 2 == 0 else ("final", "start")):
            b = trees[name]
            exe = os.path.join(b, "src", "orender", "orender")
            wd = os.path.join(scratch, name + "-" + s)
            os.makedirs(wd, exist_ok=True)
            t0 = time.monotonic()
            r = subprocess.run([exe, "-t:" + threads, rib], cwd=wd, env=env_for(b),
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            dt = time.monotonic() - t0
            if r.returncode != 0:
                sys.exit("%s tree: orender failed (%d) on %s" % (name, r.returncode, s))
            times[(s, name)].append(dt)
    print("series %d of %d done" % (i + 1, series), flush=True)

print()
print("%-28s %-6s %8s %8s %8s %8s   runs" % ("scene", "tree", "median", "min", "max", "spread"))
slower = 0
for s in scenes:
    row = {}
    for name in ("start", "final"):
        v = times[(s, name)]
        med, lo, hi = statistics.median(v), min(v), max(v)
        row[name] = (med, hi - lo)
        print("%-28s %-6s %8.2f %8.2f %8.2f %8.2f   %s" % (s, name, med, lo, hi, hi - lo, ", ".join("%.2f" % x for x in v)))
    diff = row["final"][0] - row["start"][0]
    spread = max(row["start"][1], row["final"][1])
    if diff > spread:
        verdict = "BEYOND SPREAD (slower)"
        slower += 1
    elif diff < -spread:
        verdict = "beyond spread (faster)"
    else:
        verdict = "within spread"
    print("%-28s diff of medians %+.2f s (%+.1f%%), larger spread %.2f s: %s" % (s, diff, 100.0 * diff / row["start"][0], spread, verdict))
    print()
print("scenes slower beyond the spread: %d" % slower)
sys.exit(1 if slower else 0)
EOF
