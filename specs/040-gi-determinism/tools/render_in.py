#!/usr/bin/env python3
"""render_in.py (spec 040, Phase 0): one render with a given build tree's environment (the Visual_* tests' environment, as
measure_gi.py's env_for() builds it), in a scratch directory. Run as one job under the lock:
  ~/.claude/bin/cpujob 040-smoke python3 specs/040-gi-determinism/tools/render_in.py <build-dir> <rib> <workdir> [orender options...]
With LIBS=1 in the environment it prints the shared libraries the process loaded (DYLD_PRINT_LIBRARIES on macOS), to prove that a
frozen copy of a build tree resolves its libraries inside itself.
"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import measure_gi

b, rib, wd = (os.path.abspath(a) for a in sys.argv[1:4])
opts = sys.argv[4:]
os.makedirs(wd, exist_ok=True)
env = measure_gi.env_for(b)
if os.environ.get("LIBS"):
    env["DYLD_PRINT_LIBRARIES"] = "1"
r = subprocess.run([os.path.join(b, "src", "orender", "orender")] + opts + [rib], cwd=wd, env=env,
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
text = r.stdout.decode(errors="replace")
if os.environ.get("LIBS"):
    libs = [l for l in text.splitlines() if "dylib" in l and "/usr/lib" not in l and "/System" not in l]
    inside = [l for l in libs if b + "/" in l]
    print("loaded %d non-system libraries, %d inside %s" % (len(libs), len(inside), b))
    for l in libs:
        if b + "/" not in l:
            print("  OUTSIDE: " + l)
else:
    print(text[-2000:])
print("exit", r.returncode)
sys.exit(r.returncode)
