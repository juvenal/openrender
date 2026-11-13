#!/usr/bin/env bash
# timing_sc007.sh: SC-007, the render time of three scaled scratch scenes before and after spec 032 (the method of
# specs/029-hider-shading-separation/tools/timing_sc010.sh, itself spec 026's and spec 028's section 0.5: interleaved
# series, because the fanless M2's wall time follows machine load and its thermal state as much as the code). The scenes
# (clarification Q9 B) are scratch scenes in this directory, never tracked suite scenes:
#   perf-occlusion-cache   occlusion() through a written irradiance cache (maxerror > 0), 640 x 480, ShadingRate 1
#   perf-indirect-photon   a photon pass writing a global map (one thread), then indirectdiffuse() reading it
#   perf-ptc               bake3d() in frame 1, texture3d() in frame 2
#
#   timing_sc007.sh <starting-tree-build-dir> <finished-tree-build-dir> [series]
#
# Run it as ONE job under the lock:  ~/.claude/bin/cpujob 032-sc007 specs/032-gi-cache-split/tools/timing_sc007.sh build-base build 5
# (Phase 1 runs it base against base: build-base build-base, to measure the run-to-run spread that is the bar.)
#
# For each scene it runs the starting tree's orender and the finished tree's orender alternately, `series` times (default
# 5), from a scratch directory, with the environment the Visual_* tests use for that tree. The RIB is the scratch template
# of this directory, with @THREADS@ replaced by THREADS (default 4) and scaled by SCALES, for both trees. It prints per
# scene and per tree the median, min, max and spread (max - min), and the difference of the medians. A difference of the
# medians beyond the larger of the two spreads is reported as BEYOND SPREAD: that is a defect to report, not a threshold
# to move. Exit 0 when every scene is within spread (or faster), 1 when a scene is slower beyond the spread, 3 when no
# scene is slower but one median is under MIN_SECONDS (the scaling is too small to time: raise a factor, do not widen
# anything).
#
# SCALES (default below, raised from the unscaled 0.05 to 0.1 s renders of the 640 x 480 templates) is one word per scene, `scene:format=F,shadingrate=S,emit=E` (omitted keys are 1); format=F multiplies every
# `Format w h a` by F, shadingrate=S divides every `ShadingRate r` by S, emit=E multiplies a photon hider's
# `"int emit" [N]`. The templates carry 640 x 480 and ShadingRate 1; the first run prints each
# median and flags `TOO SHORT` under MIN_SECONDS (default 3): raise the factor there and record it in validation.md.

set -u
BASE_BUILD="${1:?usage: timing_sc007.sh <starting-tree-build-dir> <finished-tree-build-dir> [series]}"
FINAL_BUILD="${2:?usage: timing_sc007.sh <starting-tree-build-dir> <finished-tree-build-dir> [series]}"
SERIES="${3:-5}"
THREADS="${THREADS:-4}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
export ROOT HERE BASE_BUILD FINAL_BUILD SERIES THREADS

SCENES="${SCENES:-perf-occlusion-cache perf-indirect-photon perf-ptc}"
SCALES="${SCALES:-perf-occlusion-cache:format=12,shadingrate=3 perf-indirect-photon:format=6,shadingrate=3,emit=10 perf-ptc:format=6,shadingrate=3}"
MIN_SECONDS="${MIN_SECONDS:-3}"
SCENE_THREADS="${SCENE_THREADS:-}"
export SCENES SCALES MIN_SECONDS SCENE_THREADS

exec python3 - <<'EOF'
import os, re, statistics, subprocess, sys, tempfile, time

root = os.environ["ROOT"]
series = int(os.environ["SERIES"])
threads = os.environ["THREADS"]
scenes = os.environ["SCENES"].split()
min_seconds = float(os.environ.get("MIN_SECONDS", "3"))
# SCENE_THREADS: `scene=N` words, a thread count for one scene (perf-ptc=1 on Linux: bake3d crashes with several threads, GitHub #122)
scene_threads = dict(kv.split("=") for kv in os.environ.get("SCENE_THREADS", "").split())
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
    if sys.platform.startswith("linux"):
        # a copied build tree keeps RUNPATH entries naming the original tree; LD_LIBRARY_PATH is searched before RUNPATH,
        # so the copy loads its own shared libraries (check with ldd / LD_DEBUG=libs)
        libdirs = sorted({r for r, _, fs in os.walk(os.path.join(b, "src")) for f in fs if ".so" in f})
        e["LD_LIBRARY_PATH"] = ":".join(libdirs + ([e["LD_LIBRARY_PATH"]] if e.get("LD_LIBRARY_PATH") else []))
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

scratch = tempfile.mkdtemp(prefix="sc007-")
times = {(s, t): [] for s in scenes for t in trees}
ribdir = os.path.join(scratch, "rib")
os.makedirs(ribdir, exist_ok=True)
rib_of = {}
for s in scenes:
    src = os.path.join(os.environ["HERE"], s + ".rib")
    text = open(src).read().replace("@THREADS@", scene_threads.get(s, threads))
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
            for stale in os.listdir(wd):    # a photon pass writes .gpm maps and a cache is read back when it exists; a stale one would change what the next run does
                if stale.endswith((".gpm", ".icache", ".ptc")):
                    os.remove(os.path.join(wd, stale))
            t0 = time.monotonic()
            r = subprocess.run([exe, rib], cwd=wd, env=env_for(b),
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
