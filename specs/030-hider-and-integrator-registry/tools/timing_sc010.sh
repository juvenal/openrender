#!/usr/bin/env bash
# timing_sc010.sh: SC-010, the render time of one representative reyes scene (teapot-somewood-reyes, shading-heavy
# through the REYES hider) and one representative raytrace scene (killeroo-raytrace, through the raytrace hider) before
# and after spec 030 (spec 029's method, itself spec 026's and 028's: interleaved, because the fanless M2's wall time
# follows machine load and its thermal state as much as the code).
#
#   timing_sc010.sh <starting-tree-build-dir> <finished-tree-build-dir> [series]
#
# Run it as ONE job under the lock:  ~/.claude/bin/cpujob 030-sc010 specs/030-hider-and-integrator-registry/tools/timing_sc010.sh build-base build
#
# For each scene it runs the starting tree's orender and the finished tree's orender alternately, `series` times
# (default 5), at the same thread count (THREADS, default 4), from a scratch directory, with the environment the
# Visual_* tests use for that tree. The RIB is the finished tree's, for both. It prints per scene and per tree the
# median, min, max and spread (max - min), and the difference of the medians. A difference of the medians beyond the
# larger of the two spreads is reported as BEYOND SPREAD: that is a defect to report, not a threshold to move.
# Exit 0 when every scene is within spread (or faster), 1 when a scene is slower beyond the spread, 3 when no scene is
# slower but one median is under MIN_SECONDS (the scaling below is too small to time: raise a factor, do not widen anything).
#
# SCALED SCENES (maintainer decision on F1, 2026-11-02): the suite RIBs render in 0.1 to 0.8 s, which is process
# start-up noise. The script therefore times a scratch COPY of each RIB, generated once per run into the scratch directory
# (never a tracked scene; both trees render the identical bytes), with the factors of SCALES: format=F multiplies every
# `Format w h a` by F (inserting 640*F x 480*F after the Display line when the RIB has none: the RISpec default), shadingrate=S
# divides every `ShadingRate r` by S (inserting 1/S when there is none), emit=E multiplies a photon hider's `"int emit" [N]`.
# SCALES is one word per scene, `scene:format=F,shadingrate=S,emit=E` (omitted keys are 1); a scene without a word is not scaled.
# The factors below are the initial choice made from the unscaled times and the cost model (pixels x 1/ShadingRate); the first
# run prints each median and flags `TOO SHORT` under MIN_SECONDS (default 3): raise the factor there and record it in validation.md.

set -u
BASE_BUILD="${1:?usage: timing_sc010.sh <starting-tree-build-dir> <finished-tree-build-dir> [series]}"
FINAL_BUILD="${2:?usage: timing_sc010.sh <starting-tree-build-dir> <finished-tree-build-dir> [series]}"
SERIES="${3:-5}"
THREADS="${THREADS:-4}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
export ROOT BASE_BUILD FINAL_BUILD SERIES THREADS

# one representative reyes scene and one representative raytrace scene. SCENE_THREADS names a scene that must run at
# another thread count (none today).
# (SCENES and SCENE_THREADS can be overridden from the environment.)
SCENES="${SCENES:-teapot-somewood-reyes killeroo-raytrace}"
SCENE_THREADS="${SCENE_THREADS:-}"
export SCENE_THREADS
export SCENES
# unscaled time at -t:4 -> factors (pixels x 1/ShadingRate x photons)
SCALES="${SCALES:-teapot-somewood-reyes:format=3,shadingrate=2 killeroo-raytrace:format=3,shadingrate=2}"
MIN_SECONDS="${MIN_SECONDS:-3}"
export SCALES MIN_SECONDS

exec python3 - <<'EOF'
import os, re, statistics, subprocess, sys, tempfile, time

root = os.environ["ROOT"]
series = int(os.environ["SERIES"])
threads = os.environ["THREADS"]
scenes = os.environ["SCENES"].split()
scene_threads = dict(kv.split("=") for kv in os.environ.get("SCENE_THREADS", "").split())
min_seconds = float(os.environ.get("MIN_SECONDS", "3"))
scales = {}
for word in os.environ.get("SCALES", "").split():
    name, _, kv = word.partition(":")
    scales[name] = {k: float(v) for k, v in (x.split("=") for x in kv.split(",") if x)}

def fmt_num(x):
    return ("%g" % x)

def scale_rib(text, f):
    """The scratch copy of a RIB: Format x format, ShadingRate / shadingrate, photon emit x emit."""
    F, S, E = f.get("format", 1.0), f.get("shadingrate", 1.0), f.get("emit", 1.0)
    if F != 1.0:
        if re.search(r"^\s*Format\s", text, re.M):
            text = re.sub(r"^(\s*Format\s+)(\d+)(\s+)(\d+)(\s+[\d.]+)",
                          lambda m: m.group(1) + str(int(int(m.group(2)) * F)) + m.group(3) + str(int(int(m.group(4)) * F)) + m.group(5),
                          text, flags=re.M)
        else:
            text = re.sub(r"^(\s*Display\b[^\n]*\n)", lambda m: m.group(1) + "Format %d %d 1\n" % (640 * F, 480 * F), text, count=1, flags=re.M)
    if S != 1.0:
        if re.search(r"^\s*ShadingRate\s", text, re.M):
            text = re.sub(r"^(\s*ShadingRate\s+)([-+0-9.eE]+)", lambda m: m.group(1) + fmt_num(float(m.group(2)) / S), text, flags=re.M)
        else:
            text = re.sub(r"^(\s*Display\b[^\n]*\n)", lambda m: m.group(1) + "ShadingRate %s\n" % fmt_num(1.0 / S), text, count=1, flags=re.M)
    if E != 1.0:
        text = re.sub(r'("int emit"\s*\[\s*)(\d+)', lambda m: m.group(1) + str(int(int(m.group(2)) * E)), text)
    return text
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
ribdir = os.path.join(scratch, "rib")
os.makedirs(ribdir, exist_ok=True)
rib_of = {}
for s in scenes:
    src = os.path.join(root, "examples", "rib", "tests", s + ".rib")
    text = open(src).read()
    if s in scales:
        text = scale_rib(text, scales[s])
        print("scaled %-26s %s" % (s, ", ".join("%s x%s" % (k, fmt_num(v)) for k, v in scales[s].items())), flush=True)
    rib_of[s] = os.path.join(ribdir, s + ".rib")
    open(rib_of[s], "w").write(text)
for i in range(series):
    for s in scenes:
        rib = rib_of[s]
        for name in (("start", "final") if i % 2 == 0 else ("final", "start")):
            b = trees[name]
            exe = os.path.join(b, "src", "orender", "orender")
            wd = os.path.join(scratch, name + "-" + s)
            os.makedirs(wd, exist_ok=True)
            for stale in os.listdir(wd):    # a photon pass writes .gpm maps; a stale one would change what the next run does
                if stale.endswith(".gpm"):
                    os.remove(os.path.join(wd, stale))
            t0 = time.monotonic()
            r = subprocess.run([exe, "-t:" + scene_threads.get(s, threads), rib], cwd=wd, env=env_for(b),
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            dt = time.monotonic() - t0
            if r.returncode != 0:
                sys.exit("%s tree: orender failed (%d) on %s" % (name, r.returncode, s))
            times[(s, name)].append(dt)
    print("series %d of %d done" % (i + 1, series), flush=True)

print()
print("%-28s %-6s %8s %8s %8s %8s   runs" % ("scene", "tree", "median", "min", "max", "spread"))
slower = 0
short = 0
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
    for name in ("start", "final"):
        if row[name][0] < min_seconds:
            short += 1
            print("%-28s %-6s median %.2f s: TOO SHORT (< %g s): raise this scene's factor" % (s, name, row[name][0], min_seconds))
    print("%-28s diff of medians %+.2f s (%+.1f%%), larger spread %.2f s: %s" % (s, diff, 100.0 * diff / row["start"][0], spread, verdict))
    print()
print("scenes slower beyond the spread: %d" % slower)
print("scene medians under %g s: %d" % (min_seconds, short))
sys.exit(1 if slower else (3 if short else 0))
EOF
