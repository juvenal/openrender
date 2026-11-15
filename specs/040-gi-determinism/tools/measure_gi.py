#!/usr/bin/env python3
"""measure_gi.py: spec 040's plan-stage measurements of the starting tree (research.md R1, R2, R7).

Run every mode as ONE job under the lock, from the worktree root:
  ~/.claude/bin/cpujob 040-measure python3 specs/040-gi-determinism/tools/measure_gi.py <mode> <build-dir> [args]

Modes
  repeat <rib> <output> <runs> <dirmode> [orender options...]
      Renders <rib> <runs> times, each as a separate orender process, and reports the md5 histogram of <output>, the photon
      count read from it (for a .gpm), and how many runs failed (exit code, signal). dirmode is `fresh` (a new empty
      directory per run: no stale map is read before the pass) or `reuse` (one directory: the previous run's map is there
      when the next run starts, as when a test is re-run). For a photon map, the first two distinct files are compared
      byte by byte and every differing byte is classified by where it lies in the file (header, item k and its field, the
      trailer); MallocPreScribble=1 in the environment of a second series shows whether unassigned heap bytes are the cause.
  stats <rib> [orender options...]
      One render with `Option "statistics" "endofframe" [3]` prepended; prints the GI and shading counters and the wall time.

The environment of a render is the Visual_* tests' for that build tree (timing_sc007.sh's env_for()).
"""
import collections
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))

# .gpm layout (fileResource.cpp ropen header, then CMap::write, then CPhotonStore::write's trailer)
ITEM = 44  # sizeof(CPhoton): P 0-11, N 12-23, flags 24-25, pad 26-27, C 28-39, theta 40, phi 41, pad 42-43
FIELDS = [(0, 12, "P"), (12, 24, "N"), (24, 26, "flags"), (26, 28, "pad1"), (28, 40, "C"), (40, 41, "theta"),
          (41, 42, "phi"), (42, 44, "pad2")]


def env_for(b):
    e = dict(os.environ)
    if sys.platform.startswith("linux"):
        libdirs = sorted({r for r, _, fs in os.walk(os.path.join(b, "src")) for f in fs if ".so" in f})
        e["LD_LIBRARY_PATH"] = ":".join(libdirs + ([e["LD_LIBRARY_PATH"]] if e.get("LD_LIBRARY_PATH") else []))
    disp = [os.path.join(b, "src", "display", d) for d in ("file", "rgbe", "framebuffer", "openexr")]
    e.update({
        "SHADERS": os.path.join(b, "shaders"),
        "ORENDERHOME": os.path.join(b, "tests", "visual", "_home"),
        "DISPLAYS": ":".join(p for p in disp if os.path.isdir(p)),
        "GEOMETRIES": os.path.join(ROOT, "geometry"),
        "TEXTURES": os.path.join(ROOT, "examples", "rib", "tests"),
    })
    return e


def gpm_header_len(d):
    # magic (4), version[4] (16), the type name's length (4), the name and its NUL
    n = struct.unpack_from("<i", d, 20)[0]
    return 24 + n + 1


def gpm_layout(d):
    h = gpm_header_len(d)
    num_items, max_items = struct.unpack_from("<ii", d, h)
    items = h + 8
    trailer = items + ITEM * (num_items + 1)
    return h, num_items, max_items, items, trailer


def classify(d, off):
    h, n, _, items, trailer = gpm_layout(d)
    if off < h:
        return "header"
    if off < items:
        return "counts"
    if off < trailer:
        k, r = divmod(off - items, ITEM)
        name = next(f for a, b, f in FIELDS if a <= r < b)
        return ("item0:" if k == 0 else "item:") + name
    rel = off - trailer
    if rel < 24:
        return "trailer:bound"
    if rel < 24 + 128:
        return "trailer:matrices"
    return "trailer:maxPower"


def run(exe, rib, wd, b, opts):
    t0 = time.monotonic()
    r = subprocess.run([exe] + opts + [rib], cwd=wd, env=env_for(b), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return r, time.monotonic() - t0


def mode_repeat(b, rib, output, runs, dirmode, opts):
    exe = os.path.join(b, "src", "orender", "orender")
    rib = os.path.abspath(rib)
    base = tempfile.mkdtemp(prefix="m040-")
    hist = collections.Counter()
    counts = collections.Counter()
    failures = []
    samples = {}
    for i in range(runs):
        wd = os.path.join(base, "run%02d" % i if dirmode == "fresh" else "shared")
        os.makedirs(wd, exist_ok=True)
        r, _ = run(exe, rib, wd, b, opts)
        out = os.path.join(wd, output)
        if r.returncode != 0 or not os.path.exists(out):
            failures.append((i, r.returncode))
            continue
        d = open(out, "rb").read()
        md5 = hashlib.md5(d).hexdigest()
        hist[md5] += 1
        if output.endswith(".gpm"):
            counts[gpm_layout(d)[1]] += 1
        if md5 not in samples:
            samples[md5] = d
            shutil.copy(out, os.path.join(base, "sample-%s-%s" % (md5[:8], output)))
    print("scene %s, %d runs, dirmode %s, options %s, MallocPreScribble=%s" % (
        os.path.relpath(rib, ROOT), runs, dirmode, " ".join(opts) or "(none)", os.environ.get("MallocPreScribble", "unset")))
    print("  failed runs: %d %s" % (len(failures), failures))
    for md5, c in hist.most_common():
        print("  md5 %s: %d" % (md5, c))
    if counts:
        print("  photons per run (numItems): %s" % dict(sorted(counts.items())))
    if len(samples) >= 2 and output.endswith(".gpm"):
        (ma, a), (mb, bb) = list(samples.items())[:2]
        print("  byte comparison of %s and %s (sizes %d, %d)" % (ma[:8], mb[:8], len(a), len(bb)))
        if len(a) != len(bb):
            print("  sizes differ: numItems %d vs %d" % (gpm_layout(a)[1], gpm_layout(bb)[1]))
        else:
            where = collections.Counter()
            items = set()
            for off in range(len(a)):
                if a[off] != bb[off]:
                    c = classify(a, off)
                    where[c] += 1
                    if c.startswith("item"):
                        items.add((off - gpm_layout(a)[3]) // ITEM)
            print("  differing bytes by place: %s" % dict(sorted(where.items())))
            print("  items touched: %d (item 0 among them: %s)" % (len(items), 0 in items))
    print("  scratch: %s" % base)


def mode_stats(b, rib, opts):
    exe = os.path.join(b, "src", "orender", "orender")
    wd = tempfile.mkdtemp(prefix="m040s-")
    text = open(rib).read()
    lines = text.split("\n")
    at = 1 if lines and lines[0].startswith("##") else 0
    lines.insert(at, 'Option "statistics" "endofframe" [3]')
    opts = ["-x", "3"] + opts  # info() (CODE_STATS) prints only at log level 3
    scene = os.path.join(wd, os.path.basename(rib))
    open(scene, "w").write("\n".join(lines))
    r, dt = run(exe, scene, wd, b, opts)
    keep = ("Num Samples", "Num Rays", "Photonmap Lookups", "Avg. Shading", "Avg. Sampling", "Occlusion:", "Indirect Diffuse:",
            "Photons:", "Total Traced")
    print("scene %s, options %s: exit %d, %.2f s" % (os.path.basename(rib), " ".join(opts) or "(none)", r.returncode, dt))
    for line in r.stdout.decode(errors="replace").splitlines():
        if any(k in line for k in keep):
            print("  " + line.strip())


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    mode, b = sys.argv[1], os.path.abspath(sys.argv[2])
    a = sys.argv[3:]
    if mode == "repeat":
        mode_repeat(b, a[0], a[1], int(a[2]), a[3], a[4:])
    elif mode == "stats":
        mode_stats(b, a[0], a[1:])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
