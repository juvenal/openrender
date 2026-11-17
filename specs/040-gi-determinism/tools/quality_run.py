#!/usr/bin/env python3
"""T029: starting-tree cached images against uncached high-sample truths. One job under the lock:
   cpujob 040-quality-run python3 specs/040-gi-determinism/tools/quality_run.py (needs build-logs/040-quality/gi_quality and shaders/)"""
import glob, os, re, subprocess, sys, time
W = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
Q = W + "/build-logs/040-quality"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import measure_gi
B = W + "/" + os.environ.get("QTREE", "build-base")      # QTREE=build QOUT=build-logs/040-quality-p5 measures the new tree (Phase 5)
OUT = W + "/" + os.environ.get("QOUT", "build-logs/040-quality")
os.makedirs(OUT, exist_ok=True)
env = measure_gi.env_for(B)
env["SHADERS"] = Q + "/shaders:" + B + "/shaders"
exe = B + "/src/orender/orender"
GQ = Q + "/gi_quality"
scenes = []
for n in ["sphere-occlusion-reyes", "sphere-occlusion-reyes-slo", "sphere-indirectdiffuse-reyes", "sphere-indirectdiffuse-reyes-slo"]:
    scenes.append((n, W + "/examples/rib/tests/" + n + ".rib"))
for p in sorted(glob.glob(W + "/tests/gi_determinism/scenes/gi-*.rib")):
    scenes.append((os.path.basename(p)[:-4], p))
def variant(text, kind):
    if kind != "cached":
        text = text.replace('Surface "occlusion_probe"', 'Surface "occlusion_probe_256"').replace('Surface "indirectdiffuse_probe"', 'Surface "indirectdiffuse_probe_256"')
        text = re.sub(r'("float maxerror"\s*)\[[^\]]*\]', r'\1[0]', text)
        text = re.sub(r'\["slo"\]', '["rslo"]', text)
    head = 'Quantize "rgba" 0 0 0 0\n'
    if kind == "truth3":
        head += 'Option "sampling" "seed" [1]\n'
    return head + text
def render(name, rib, kind):
    wd = "%s/%s/%s" % (OUT, name, kind)
    os.makedirs(wd, exist_ok=True)
    for f in os.listdir(wd):
        os.remove(os.path.join(wd, f))
    open(wd + "/" + name + ".rib", "w").write(variant(open(rib).read(), kind))
    t0 = time.monotonic()
    r = subprocess.run([exe, "-t:1", name + ".rib"], cwd=wd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    open(wd + "/render.log", "wb").write(r.stdout)
    return r.returncode, time.monotonic() - t0, wd
def gq(a, b):
    r = subprocess.run([GQ, a, b], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return r.stdout.decode().strip()
out = []
for name, rib in scenes:
    res = {}
    for kind in ("cached", "truth1", "truth2", "truth3"):
        rc, dt, wd = render(name, rib, kind)
        res[kind] = wd
        print("%s %s exit %d %.1fs" % (name, kind, rc, dt), flush=True)
        if rc != 0:
            out.append("%s %s RENDER FAILED exit %d" % (name, kind, rc))
    for tif in sorted(os.path.basename(p) for p in glob.glob(res["cached"] + "/*.tif")):
        if not all(os.path.exists("%s/%s" % (res[k], tif)) for k in res):
            out.append("%s %s: image missing in a variant" % (name, tif)); continue
        out.append("%s %s cached-vs-truth: %s" % (name, tif, gq(res["cached"] + "/" + tif, res["truth1"] + "/" + tif)))
        out.append("%s %s truth-vs-truth(same seed): %s" % (name, tif, gq(res["truth1"] + "/" + tif, res["truth2"] + "/" + tif)))
        out.append("%s %s truth-vs-truth(seed 1): %s" % (name, tif, gq(res["truth1"] + "/" + tif, res["truth3"] + "/" + tif)))
open(OUT + "/results.txt", "w").write("\n".join(out) + "\n")
print("\n".join(out))
