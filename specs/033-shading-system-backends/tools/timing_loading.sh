#!/usr/bin/env bash
# timing_loading.sh: the loading measurement of research R12 (FR-024, P-Q8), taken on the starting tree in Phase 1 and on the
# finished tree in Phase 9. The method follows specs/031-geometry-hider-agnostic/tools/timing_sc010.sh (statistics over
# repeated runs on an idle machine, the spread reported beside the median); the measured thing is different.
#
#   timing_loading.sh <build-dir> [series]
#
# Run it as ONE job under the lock:
#   ~/.claude/bin/cpujob 033-timing specs/033-shading-system-backends/tools/timing_loading.sh build
#
# Two measurements, `series` runs each (default 5); the first run of each is discarded (cold file cache), the median,
# min, max and spread (max - min) of the rest are printed:
#   (1) tests/benchmark_loading.sh (100 loads of one shader by sloinfo) over all 137 shipped shaders in both formats,
#       each run being one pass over the 274 objects; the figure is the wall time of the whole pass;
#   (2) the in-process program bench_shader_load: 274 loads through the loader entry points (the starting tree) or
#       through ShadingSystem (the finished tree), in one process, split into the .rslo and the .slo half.
# The pass rule of Phase 9: after <= before + max(5% of before, 2 x the spread recorded before), per figure.

set -u
BUILD="${1:?usage: timing_loading.sh <build-dir> [series]}"
SERIES="${2:-5}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
export BUILD ROOT SERIES

exec python3 - <<'PY'
import os, statistics, subprocess, sys, time

root = os.environ["ROOT"]
build = os.path.abspath(os.environ["BUILD"])
series = int(os.environ["SERIES"])
sloinfo = os.path.join(build, "src", "sloinfo", "sloinfo")
bench = os.path.join(build, "tests", "shading_system", "bench_shader_load")
shaders = os.path.join(build, "shaders")
script = os.path.join(root, "tests", "benchmark_loading.sh")
for p in (sloinfo, bench, shaders, script):
    if not os.path.exists(p):
        sys.exit("missing " + p)

names = sorted(f[:-5] for f in os.listdir(shaders) if f.endswith(".rslo"))
formats = [("rslo", "rslo")]
if all(os.path.exists(os.path.join(shaders, n + ".slo")) for n in names):
    formats.append(("slo", "slo"))
objects = [(n, f) for n in names for f, _ in formats]
print("shaders %d, formats %s, objects %d, series %d" % (len(names), ",".join(f for f, _ in formats), len(objects), series), flush=True)

def stats(label, values):
    kept = values[1:] if len(values) > 1 else values
    med, lo, hi = statistics.median(kept), min(kept), max(kept)
    print("%-34s median %9.3f  min %9.3f  max %9.3f  spread %8.3f   runs %s (first discarded)" %
          (label, med, lo, hi, hi - lo, ", ".join("%.3f" % v for v in values)), flush=True)

# (1) sloinfo, 100 loads per object, one pass over every object per run
passes = []
for i in range(series):
    t0 = time.monotonic()
    for n, f in objects:
        r = subprocess.run(["bash", script, sloinfo, os.path.join(shaders, n + "." + f)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if r.returncode != 0:
            sys.exit("benchmark_loading.sh failed on %s.%s" % (n, f))
    passes.append(time.monotonic() - t0)
    print("sloinfo pass %d of %d: %.3f s" % (i + 1, series, passes[-1]), flush=True)
print()
stats("sloinfo, 100 loads x all objects (s)", passes)

# (2) in process
rs, ss = [], []
for i in range(series):
    r = subprocess.run([bench, shaders], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("bench_shader_load failed: " + r.stderr)
    vals = dict(line.split() for line in r.stdout.splitlines())
    rs.append(float(vals["rslo_ms"]))
    ss.append(float(vals.get("slo_ms", "0")))
    print("in-process run %d of %d: objects %s, rslo %.3f ms, slo %.3f ms" % (i + 1, series, vals["objects"], rs[-1], ss[-1]), flush=True)
print()
stats("in-process .rslo loads (ms)", rs)
stats("in-process .slo loads (ms)", ss)
PY
