#!/usr/bin/env python3
"""T014: master baselines for spec 021 -- throttled render timings and the
planned Phase 3 metric checks run against master's renders.

Usage: python3 baseline.py <output-dir>   (run from any directory; renders with
the worktree's build/ and writes <output-dir>/baseline.json)."""
import json, math, os, re, shutil, subprocess, sys, time

W = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)
shutil.copytree(f"{W}/examples/rib/tests/archives", f"{OUT}/archives", dirs_exist_ok=True)
ENV = dict(os.environ, SHADERS=f"{W}/build/shaders", ORENDERHOME=f"{W}/build/tests/visual/_home",
           DISPLAYS=f"{W}/build/src/display/file", GEOMETRIES=f"{W}/geometry",
           TEXTURES=f"{W}/examples/rib/tests")
ENV.pop("ORENDER_INSTR_LEVEL", None)
ORENDER = f"{W}/build/src/orender/orender"
METRIC = f"{W}/build/tests/visual/test_csg_seam_metric"
results = {"timings": {}, "metrics": [], "logs": {}}

def render(name):
    rib = f"{W}/examples/rib/tests/{name}.rib"
    t0 = time.time()
    p = subprocess.run(["nice", "-n", "15", "taskpolicy", "-b", ORENDER, "-x", "3", rib], cwd=OUT, env=ENV,
                       capture_output=True, text=True)
    dt = time.time() - t0
    results["timings"][name] = round(dt, 2)
    results["logs"][name] = (p.returncode, (p.stdout + p.stderr)[-4000:])
    print(f"render {name}: {dt:.2f}s rc={p.returncode}", flush=True)

def metric(test, args):
    p = subprocess.run([METRIC] + args, cwd=OUT, capture_output=True, text=True)
    line = (p.stdout + p.stderr).strip()
    results["metrics"].append({"test": test, "args": " ".join(args), "out": line, "rc": p.returncode})
    print(f"  {test}: {line}", flush=True)
    return line

def header_value(name, pattern):
    text = open(f"{W}/examples/rib/tests/{name}.rib").read()
    m = re.search(pattern, text)
    return m.group(1).strip() if m else None

# ---------------------------------------------------------------------------
EXISTING = ["csg-sphere-cube-union-raytrace", "csg-sphere-cube-union-reyes", "csg-sphere-cube-union-zbuffer",
            "csg-sphere-cube-difference-raytrace", "csg-sphere-cube-difference-reyes",
            "csg-sphere-cube-difference-zbuffer", "csg-sphere-cube-intersection-raytrace",
            "csg-sphere-cube-intersection-reyes", "csg-sphere-cube-intersection-zbuffer",
            "csg-sphere-sphere-tight-tolerance-raytrace", "csg-sphere-sphere-loose-tolerance-raytrace",
            "csg-subdivision-sphere-union-raytrace", "csg-nurbs-sphere-union-raytrace",
            "csg-nested-tree-4-level-raytrace", "csg-object-instance-reuse-raytrace",
            "csg-sphere-sphere-difference-interior-raytrace", "csg-sphere-sphere-difference-interior-raytrace-inside",
            "blobby-csg-difference-raytrace", "blobby-csg-difference-reyes", "blobby-csg-difference-zbuffer"]
HERO = [f"csg-hero-{h}-{r}" for h in ("raytrace", "reyes", "zbuffer") for r in (800, 1600)]
SPHERECUBE = [f"csg-sphere-cube-difference-{h}-{r}" for h in ("raytrace", "reyes", "zbuffer") for r in (800, 1600)]
COINC = [f"csg-coincident-{k}-{h}" for k in ("union", "difference") for h in ("raytrace", "reyes", "zbuffer")]
OTHER = ["csg-coincident-union-expected-raytrace", "csg-coincident-difference-expected-raytrace",
         "csg-open-quadric-caps-raytrace", "csg-open-quadric-caps-reyes", "csg-open-quadric-caps-zbuffer",
         "csg-nurbs-rational-sphere-union-raytrace", "csg-nurbs-rational-sphere-union-reyes",
         "csg-nurbs-rational-sphere-alone-raytrace", "csg-subdivision-sphere-union-reyes",
         "csg-subdivision-sphere-alone-raytrace", "csg-polymesh-box-difference-raytrace",
         "csg-polymesh-box-difference-reyes", "csg-polymesh-open-raytrace", "csg-nurbs-sheet-expected-raytrace",
         "csg-nurbs-sheet-instanced-raytrace", "csg-hero-two-shaders-raytrace", "csg-hero-two-shaders-reyes",
         "csg-hero-displaced-raytrace", "csg-hero-stats-800-reyes", "csg-hero-single-operand-stats-800-reyes",
         "csg-hero-instanced-outside-world-reyes"]
for n in EXISTING + HERO + SPHERECUBE + COINC + OTHER:
    render(n)

# --- T034 hero ---------------------------------------------------------------
for n in HERO:
    k = 2 if n.endswith("1600") else 1
    cx, cy = 400 * k, 250 * k
    hole, seam, sil = 45.6 * k, 61.7 * k, 122.7 * k
    metric("T034 " + n, ["--annuli", n + ".tif", "--center", f"{cx},{cy}", "--clear", f"disc:{cx},{cy},{hole}",
                         "--solid", f"annulus:{cx},{cy},{hole},{sil}", "--seam", f"{hole}", "--seam",
                         f"{seam}:rgb", "--seam", f"{sil}", "--tolerance", "1", "--margin", "2"])
    metric("T034 " + n, ["--enclosed-holes", n + ".tif", "--exclude", f"disc:{cx},{cy},{hole}"])

# --- T035 sphere-cube --------------------------------------------------------
for n in SPHERECUBE:
    cam = header_value(n, r"--camera ([^\n]+)").split()
    circles = re.findall(r"(--circle \S+ --clip-box \S+ --facing \S+)", open(f"{W}/examples/rib/tests/{n}.rib").read())
    args = ["--projected-curve", n + ".tif", "--camera"] + cam
    for c in circles:
        args += c.split()
    metric("T035 " + n, args + ["--tolerance", "1", "--edge-mode", "rgb"])
    metric("T035 " + n, ["--enclosed-holes", n + ".tif"])

# --- T036 coincident ---------------------------------------------------------
for n in COINC:
    kind = "union" if "union" in n else "difference"
    metric("T036 " + n, ["--enclosed-holes", n + ".tif"])
    metric("T036 " + n, ["--region-compare", n + ".tif", f"csg-coincident-{kind}-expected-raytrace.tif",
                         "--include", "frame", "--threshold", "20"])

# --- T037 per-operand shader (guard) -------------------------------------------
for n in ("csg-hero-two-shaders-raytrace", "csg-hero-two-shaders-reyes"):
    metric("T037 " + n, ["--region-chroma", n + ".tif", "--include", "annulus:400,250,45.6,61.7",
                         "--min-share", "b=0.4", "--max-share", "r=0.3"])
    metric("T037 " + n, ["--region-chroma", n + ".tif", "--include", "annulus:400,250,62,122",
                         "--min-share", "r=0.45", "--max-share", "b=0.15"])

# --- T038 free-form / mesh -----------------------------------------------------
metric("T038 rational union rt", ["--enclosed-holes", "csg-nurbs-rational-sphere-union-raytrace.tif"])
metric("T038 rational union reyes", ["--enclosed-holes", "csg-nurbs-rational-sphere-union-reyes.tif"])
metric("T038 rational edge", ["--edge-compare", "csg-nurbs-rational-sphere-union-reyes.tif",
                              "csg-nurbs-rational-sphere-union-raytrace.tif", "--tolerance", "1"])
for n in ("csg-nurbs-rational-sphere-union-raytrace", "csg-nurbs-rational-sphere-union-reyes"):
    cam = header_value(n, r"--camera ([^\n]+)").split()
    circ = header_value(n, r"(--circle [^\n]+)").split()
    metric("T038 projected-curve " + n, ["--projected-curve", n + ".tif", "--camera"] + cam + circ + ["--tolerance", "1"])
metric("T038 subdiv union reyes holes", ["--enclosed-holes", "csg-subdivision-sphere-union-reyes.tif"])
metric("T038 subdiv edge", ["--edge-compare", "csg-subdivision-sphere-union-reyes.tif",
                            "csg-subdivision-sphere-union-raytrace.tif", "--tolerance", "1"])
metric("T038 polymesh holes rt", ["--enclosed-holes", "csg-polymesh-box-difference-raytrace.tif"])
metric("T038 polymesh holes reyes", ["--enclosed-holes", "csg-polymesh-box-difference-reyes.tif"])
metric("T038 polymesh edge", ["--edge-compare", "csg-polymesh-box-difference-reyes.tif",
                              "csg-polymesh-box-difference-raytrace.tif", "--tolerance", "1"])
for union, alone, mask in (("csg-nurbs-rational-sphere-union-raytrace", "csg-nurbs-rational-sphere-alone-raytrace",
                            "csg-nurbs-rational-untouched.png"),
                           ("csg-subdivision-sphere-union-raytrace", "csg-subdivision-sphere-alone-raytrace",
                            "csg-subdivision-untouched.png")):
    metric("T038 untouched " + union, ["--region-compare", union + ".tif", alone + ".tif", "--margin", "0",
                                       "--include", f"mask:{W}/examples/rib/tests/masks/{mask}"])
metric("T038 nurbs_open region", ["--region-compare", "csg-nurbs-sphere-union-raytrace.tif",
                                  "csg-nurbs-sheet-expected-raytrace.tif", "--include", "frame"])

# --- T039 displaced -------------------------------------------------------------
d = 4.7
metric("T039 displaced", ["--enclosed-holes", "csg-hero-displaced-raytrace.tif", "--exclude",
                          f"disc:400,250,45.6"])
metric("T039 displaced", ["--annuli", "csg-hero-displaced-raytrace.tif", "--center", "400,250", "--clear",
                          f"disc:400,250,{45.6 - d:.1f}", "--seam", "45.6", "--seam", "122.7",
                          "--tolerance", f"{1 + d:.1f}"])

# --- T041 instances (discs well inside each instance's projected silhouette) ----
for dsc in ("disc:184.6,110.0,22.9", "disc:300.0,110.0,21.8", "disc:392.5,110.0,18.7"):
    metric("T041 instance", ["--annuli", "csg-object-instance-reuse-raytrace.tif", "--center", "0,0", "--solid", dsc])
metric("T041 instance", ["--enclosed-holes", "csg-object-instance-reuse-raytrace.tif"])

# --- T042 caps ------------------------------------------------------------------
discs = re.findall(r"(disc:[0-9.,]+)", open(f"{W}/examples/rib/tests/csg-open-quadric-caps-raytrace.rib").read())
for h in ("raytrace", "reyes", "zbuffer"):
    n = f"csg-open-quadric-caps-{h}"
    metric("T042 " + n, ["--enclosed-holes", n + ".tif"])
    for dsc in discs:
        metric("T042 " + n, ["--region-chroma", n + ".tif", "--include", dsc, "--min-share", "b=0.5"])

json.dump(results, open(f"{OUT}/baseline.json", "w"), indent=1)
print("done")
