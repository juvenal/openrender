#!/usr/bin/env python3
"""Spec 056 (tasks.md T137): see compare_corpus.sh. Usage: compare_corpus.py A_BUILD B_BUILD B_SRC OUT [tier1|all]."""
import json
import os
import re
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

A_BUILD, B_BUILD, B_SRC, OUT = (os.path.abspath(x) for x in sys.argv[1:5])
TIER = sys.argv[5] if len(sys.argv) > 5 else "all"
TIMEOUT = 1800
JOBS = 2
PREFIXES = ("Visual_", "Parity_", "Determinism")
CALLS = re.compile(r"\b(trace|transmission|shadow|gather|visibility|environment|occlusion|indirectdiffuse|photonmap|incident|opposite)\s*\(")
VOLUME = re.compile(r"^\s*(Interior|Exterior)\b", re.M)


def selection():
    tests = json.load(open(os.path.join(OUT, "ctest.json")))["tests"]
    ribs = {}
    for t in tests:
        if not t["name"].startswith(PREFIXES):
            continue
        for a in t["command"]:
            if a.endswith(".rib") and os.path.isfile(a):
                ribs.setdefault(os.path.realpath(a), []).append(t["name"])
    return ribs


def archives(path, text, seen):
    """The text of the RIB and of the archives it reads (relative to it or to examples/rib)."""
    out = [text]
    for m in re.finditer(r'ReadArchive\s+"([^"]+)"', text):
        for base in (os.path.dirname(path), os.path.join(B_SRC, "examples", "rib"), os.path.join(B_SRC, "examples", "rib", "tests")):
            p = os.path.join(base, m.group(1))
            if os.path.isfile(p) and p not in seen:
                seen.add(p)
                out += archives(p, open(p, errors="replace").read(), seen)
                break
    return out


def shader_calls():
    """Shader names whose source calls a ray-tracing builtin."""
    hot = set()
    d = os.path.join(B_SRC, "shaders")
    for f in os.listdir(d):
        if f.endswith(".sl"):
            if CALLS.search(open(os.path.join(d, f), errors="replace").read()):
                hot.add(f[:-3])
    return hot


def tier_of(texts, hot):
    t = "\n".join(texts)
    if re.search(r'Hider\s+"raytrace"', t):
        return 1
    for m in re.finditer(r'\b(?:Surface|LightSource|AreaLightSource|Displacement|Atmosphere|Imager)\s+"([^"]+)"', t):
        if m.group(1) in hot:
            return 1
    return 2


def env_for(build, other_build):
    shaders = os.path.join(build, "shaders")
    if other_build:   # the shaders only the branch has (the new probes) are taken from the branch's build
        shaders += ":" + os.path.join(other_build, "shaders")
    disp = ":".join(os.path.join(build, "src", "display", n) for n in ("file", "rgbe", "framebuffer", "openexr")
                    if os.path.isdir(os.path.join(build, "src", "display", n)))
    e = dict(os.environ)
    e.update({"SHADERS": shaders,
              "ORENDERHOME": os.path.join(build, "tests", "visual", "_home"),
              "DISPLAYS": disp,
              "GEOMETRIES": os.path.join(B_SRC, "geometry"),
              "TEXTURES": ":".join([os.path.join(B_SRC, "examples", "rib", "tests"),
                                    os.path.join(B_SRC, "tests", "unit", "image_input", "fixtures"),
                                    os.path.join(B_SRC, "tests", "unit", "texture_tile", "fixtures")])})
    return e


def run_one(tag, orender, env, rib, scratch):
    if os.path.isdir(scratch):
        shutil.rmtree(scratch)
    os.makedirs(scratch)
    t0 = time.time()
    if os.environ.get("COMPARE_PREPEND"):   # a copy of the scene with one RIB line first, as test_depth_determinism --prepend does
        copy = os.path.join(scratch, "_scene.rib")
        open(copy, "w").write(os.environ["COMPARE_PREPEND"] + "\n" + open(rib, errors="replace").read())
        rib = copy
    try:
        p = subprocess.run([orender, "-t:1", rib], cwd=scratch, env=env, stdin=subprocess.DEVNULL,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=TIMEOUT)
        rc, log = p.returncode, p.stdout
    except subprocess.TimeoutExpired:
        rc, log = "timeout", b""
    open(os.path.join(scratch, "..", os.path.basename(scratch) + ".log"), "wb").write(log)
    return rc, time.time() - t0


def files_of(d):
    r = {}
    for root, _, names in os.walk(d):
        for n in names:
            p = os.path.join(root, n)
            if n == "_scene.rib":
                continue
            r[os.path.relpath(p, d)] = open(p, "rb").read()
    return r


def job(item):
    rib, name = item
    a_dir = os.path.join(OUT, "master", name)
    b_dir = os.path.join(OUT, "branch", name)
    os.makedirs(os.path.dirname(a_dir), exist_ok=True)
    os.makedirs(os.path.dirname(b_dir), exist_ok=True)
    ra, ta = run_one("master", os.path.join(A_BUILD, "src", "orender", "orender"), env_for(A_BUILD, B_BUILD), rib, a_dir)
    rb, tb = run_one("branch", os.path.join(B_BUILD, "src", "orender", "orender"), env_for(B_BUILD, None), rib, b_dir)
    fa, fb = files_of(a_dir), files_of(b_dir)
    self_check = None
    if os.environ.get("COMPARE_SELF"):   # a second run of each tree: a scene that differs from itself is not deterministic at -t:1
        a2, b2 = a_dir + "~2", b_dir + "~2"
        run_one("master", os.path.join(A_BUILD, "src", "orender", "orender"), env_for(A_BUILD, B_BUILD), rib, a2)
        run_one("branch", os.path.join(B_BUILD, "src", "orender", "orender"), env_for(B_BUILD, None), rib, b2)
        fa2, fb2 = files_of(a2), files_of(b2)
        self_check = {"master_vs_master": sorted(k for k in set(fa) | set(fa2) if fa.get(k) != fa2.get(k)),
                      "branch_vs_branch": sorted(k for k in set(fb) | set(fb2) if fb.get(k) != fb2.get(k))}   # the files that differ between two runs of one tree
        shutil.rmtree(a2, ignore_errors=True)
        shutil.rmtree(b2, ignore_errors=True)
    if ra != rb:
        verdict = "DIFF-EXIT"
    elif set(fa) != set(fb):
        verdict = "DIFF-FILESET"
    elif any(fa[k] != fb[k] for k in fa):
        verdict = "DIFF-BYTES"
    elif not fa:
        verdict = "NO-OUTPUT-SAME"   # both produced nothing (and the same exit code): nothing to compare
    else:
        verdict = "IDENT"
    # a byte-identical scene's outputs are not kept (disk); a differing scene's stay for inspection
    if verdict in ("IDENT", "NO-OUTPUT-SAME"):
        shutil.rmtree(a_dir, ignore_errors=True)
        shutil.rmtree(b_dir, ignore_errors=True)
    return {"scene": name, "rib": rib, "verdict": verdict, "exit": [ra, rb], "files": sorted(fa), "secs": [round(ta, 1), round(tb, 1)], "self": self_check,
            "differing": sorted(k for k in set(fa) | set(fb) if fa.get(k) != fb.get(k))}


def main():
    ribs = selection()
    hot = shader_calls()
    skipped_volume, chosen = [], []
    for rib in sorted(ribs):
        texts = archives(rib, open(rib, errors="replace").read(), set())
        if any(VOLUME.search(t) for t in texts):
            skipped_volume.append(rib)
            continue
        chosen.append((tier_of(texts, hot), rib))
    chosen.sort()
    if os.environ.get("COMPARE_ONLY"):   # only the scenes whose name contains one of these (comma separated)
        keys = os.environ["COMPARE_ONLY"].split(",")
        chosen = [c for c in chosen if any(k in c[1] for k in keys)]
    if TIER == "tier1":
        chosen = [c for c in chosen if c[0] == 1]
    if os.environ.get("COMPARE_STRIDE"):   # a smoke run: every n-th scene only
        chosen = chosen[::int(os.environ["COMPARE_STRIDE"])]
    rel = lambda p: os.path.relpath(p, B_SRC)
    with open(os.path.join(OUT, "selection.txt"), "w") as f:
        f.write("registered RIBs: %d; skipped (bind Interior/Exterior): %d; compared: %d (tier 1: %d, tier 2: %d)\n" % (
            len(ribs), len(skipped_volume), len(chosen), sum(1 for c in chosen if c[0] == 1), sum(1 for c in chosen if c[0] == 2)))
        f.write("\n# skipped (bind Interior/Exterior)\n" + "\n".join(rel(p) for p in skipped_volume) + "\n")
        f.write("\n# compared\n" + "\n".join("tier%d %s" % (t, rel(p)) for t, p in chosen) + "\n")
    items = [(p, rel(p).replace("/", "__")[:-4]) for _, p in chosen]
    # positive controls (COMPARE_CONTROL=<rib>[:<rib>...]): scenes that bind an Interior/Exterior and so MUST differ, proving
    # the comparison can see a difference; they are reported apart and are not part of the verdict
    controls = [os.path.join(B_SRC, c) if not os.path.isabs(c) else c for c in os.environ.get("COMPARE_CONTROL", "").split(":") if c]
    control_items = [(c, "control__" + os.path.basename(c)[:-4]) for c in controls]
    tier_of_name = {rel(p).replace("/", "__")[:-4]: t for t, p in chosen}
    results = []
    with ThreadPoolExecutor(max_workers=JOBS) as ex:
        for r in ex.map(job, items):
            r["tier"] = tier_of_name[r["scene"]]
            results.append(r)
            print("%-16s tier%d %s  %s %s" % (r["verdict"], r["tier"], r["scene"], r["secs"], r["self"] or ""), flush=True)
    control_results = []
    for it in control_items:
        r = job(it)
        control_results.append(r)
        print("CONTROL %-16s %s  (must differ)" % (r["verdict"], r["scene"]), flush=True)
    json.dump({"results": results, "controls": control_results}, open(os.path.join(OUT, "results.json"), "w"), indent=1)
    counts = {}
    for r in results:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
    with open(os.path.join(OUT, "summary.txt"), "w") as f:
        f.write("compared %d scenes (tier 1: %d); verdicts: %s\n" % (len(results), sum(1 for r in results if r["tier"] == 1), counts))
        for r in results:
            if r["verdict"] not in ("IDENT", "NO-OUTPUT-SAME"):
                f.write("%s %s exit=%s differing=%s self-check(master twice, branch twice)=%s\n" % (r["verdict"], r["scene"], r["exit"], r["differing"], r["self"]))
        f.write("both failed alike or produced nothing: %s\n" % [r["scene"] for r in results if r["verdict"] == "NO-OUTPUT-SAME"])
        for r in control_results:
            f.write("control (must differ) %s: %s\n" % (r["scene"], r["verdict"]))
    print(open(os.path.join(OUT, "summary.txt")).read())
    sys.exit(0 if all(r["verdict"] in ("IDENT", "NO-OUTPUT-SAME") for r in results) else 1)


main()
