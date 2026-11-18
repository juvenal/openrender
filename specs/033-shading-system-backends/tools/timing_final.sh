#!/usr/bin/env bash
# timing_final.sh: the R12 loading measurement on the finished tree (spec 033 T146, FR-024, P-Q8), interleaved: every series runs the three
# figures back to back, so each sees the same machine load (the fanless M2's wall time follows load; the method of 031's timing_sc010.sh).
#
#   timing_final.sh <build-dir> [series]      run ONE job under the lock: ~/.claude/bin/cpujob 033-final-timing specs/.../timing_final.sh build
#
# Per series: (1) sloinfo, 100 loads of every shipped object (tests/benchmark_loading.sh), (2) the in-process program through the loader
# entry points (bench_shader_load, as the Phase 1 record), (3) the same program through ShadingSystem::find() (bench_shader_load_system,
# OR033_BENCH_USE_SHADING_SYSTEM). The first series is discarded. The load average (1 minute) is printed before and after each series.
# The Phase 1 record is tools/timing-base-macos.txt; the pass rule per figure: after <= before + max(5% of before, 2 x the Phase 1 spread).
set -u
BUILD="${1:?usage: timing_final.sh <build-dir> [series]}"
SERIES="${2:-5}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
export BUILD ROOT SERIES
exec python3 - <<'PY'
import os, re, statistics, subprocess, sys, time

root = os.environ["ROOT"]; build = os.path.abspath(os.environ["BUILD"]); series = int(os.environ["SERIES"])
sloinfo = os.path.join(build, "src", "sloinfo", "sloinfo")
bench = os.path.join(build, "tests", "shading_system", "bench_shader_load")
bench_sys = os.path.join(build, "tests", "shading_system", "bench_shader_load_system")
shaders = os.path.join(build, "shaders")
script = os.path.join(root, "tests", "benchmark_loading.sh")
for p in (sloinfo, bench, bench_sys, shaders, script):
    if not os.path.exists(p):
        sys.exit("missing " + p)
names = sorted(f[:-5] for f in os.listdir(shaders) if f.endswith(".rslo"))
objects = [(n, f) for n in names for f in ("rslo", "slo")]

# the Phase 1 record
base = open(os.path.join(root, "specs", "033-shading-system-backends", "tools", "timing-base-macos.txt")).read()
def record(label):
    m = re.search(re.escape(label) + r"\s+median\s+([\d.]+)\s+min\s+([\d.]+)\s+max\s+([\d.]+)\s+spread\s+([\d.]+)", base)
    return float(m.group(1)), float(m.group(4))
rec = {"sloinfo": record("sloinfo, 100 loads x all objects (s)"), "rslo": record("in-process .rslo loads (ms)"), "slo": record("in-process .slo loads (ms)")}

def load():
    return os.getloadavg()[0]

rows = {"sloinfo": [], "rslo_old": [], "slo_old": [], "rslo_sys": [], "slo_sys": []}
loads = []
for i in range(series):
    l0 = load()
    t0 = time.monotonic()
    for n, f in objects:
        r = subprocess.run(["bash", script, sloinfo, os.path.join(shaders, n + "." + f)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if r.returncode != 0:
            sys.exit("benchmark_loading.sh failed on %s.%s" % (n, f))
    rows["sloinfo"].append(time.monotonic() - t0)
    l1 = load()
    r = subprocess.run([bench, shaders], capture_output=True, text=True)
    if r.returncode != 0: sys.exit("bench_shader_load failed: " + r.stderr)
    v = dict(x.split() for x in r.stdout.splitlines()); rows["rslo_old"].append(float(v["rslo_ms"])); rows["slo_old"].append(float(v["slo_ms"]))
    r = subprocess.run([bench_sys, shaders], capture_output=True, text=True)
    if r.returncode != 0: sys.exit("bench_shader_load_system failed: " + r.stderr)
    v = dict(x.split() for x in r.stdout.splitlines()); rows["rslo_sys"].append(float(v["rslo_ms"])); rows["slo_sys"].append(float(v["slo_ms"]))
    l2 = load()
    loads.append((l0, l1, l2))
    print("series %d of %d: sloinfo %.3f s | loader entry points rslo %.3f ms slo %.3f ms | ShadingSystem rslo %.3f ms slo %.3f ms | load1 before %.2f, after sloinfo %.2f, after in-process %.2f"
          % (i + 1, series, rows["sloinfo"][-1], rows["rslo_old"][-1], rows["slo_old"][-1], rows["rslo_sys"][-1], rows["slo_sys"][-1], l0, l1, l2), flush=True)

def stats(label, values, base_key=None):
    kept = values[1:] if len(values) > 1 else values
    med, lo, hi = statistics.median(kept), min(kept), max(kept)
    line = "%-44s median %9.3f  min %9.3f  max %9.3f  spread %8.3f" % (label, med, lo, hi, hi - lo)
    if base_key:
        b, sp = rec[base_key]
        limit = b + max(0.05 * b, 2 * sp)
        line += "   | Phase 1 median %.3f spread %.3f, limit %.3f: %s (%+.1f%%)" % (b, sp, limit, "PASS" if med <= limit else "FAIL", 100 * (med - b) / b)
    print(line, flush=True)
    return med <= (rec[base_key][0] + max(0.05 * rec[base_key][0], 2 * rec[base_key][1])) if base_key else True
print()
ok = True
ok &= stats("sloinfo, 100 loads x all objects (s)", rows["sloinfo"], "sloinfo")
ok &= stats("in-process .rslo, loader entry points (ms)", rows["rslo_old"], "rslo")
ok &= stats("in-process .slo, loader entry points (ms)", rows["slo_old"], "slo")
ok &= stats("in-process .rslo, ShadingSystem::find (ms)", rows["rslo_sys"], "rslo")
ok &= stats("in-process .slo, ShadingSystem::find (ms)", rows["slo_sys"], "slo")
print("load average (1 min) at each series, [before, after sloinfo, after in-process]: " + "; ".join("%.2f/%.2f/%.2f" % l for l in loads))
print("RESULT " + ("PASS" if ok else "FAIL"))
sys.exit(0 if ok else 1)
PY
