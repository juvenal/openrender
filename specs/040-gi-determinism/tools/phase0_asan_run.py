#!/usr/bin/env python3
"""T015: run photon-parity at -t:4 up to N times with build-asan's orender (fresh directory per run) until ASan reports; keep the
first report. With ASAN_ALL=1 it does not stop at the first report (a tally of the report kinds). One job under the lock: cpujob 040-asan-run python3 build-logs/040-asan-run.py build-asan <rib> <runs> <report-prefix> [opts]"""
import os, subprocess, sys, tempfile, time
W = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import measure_gi
b = os.path.join(W, sys.argv[1]); rib = sys.argv[2]; runs = int(sys.argv[3]); prefix = sys.argv[4]; opts = sys.argv[5:]
env = measure_gi.env_for(b)
env.setdefault("ASAN_OPTIONS", "detect_leaks=0:abort_on_error=0:halt_on_error=1:symbolize=1:fast_unwind_on_malloc=0")
base = tempfile.mkdtemp(prefix="m040a-")
fails = 0
for i in range(runs):
    wd = os.path.join(base, "run%02d" % i); os.makedirs(wd)
    t0 = time.monotonic()
    r = subprocess.run([os.path.join(b, "src", "orender", "orender")] + opts + [rib], cwd=wd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    text = r.stdout.decode(errors="replace")
    dt = time.monotonic() - t0
    print("run %d: exit %d, %.1f s, %s" % (i, r.returncode, dt, "ASan report" if "AddressSanitizer" in text else ""), flush=True)
    if r.returncode != 0 or "AddressSanitizer" in text:
        fails += 1
        open("%s-%02d.txt" % (prefix, i), "w").write(text)
        if "AddressSanitizer" in text and not os.environ.get("ASAN_ALL"):
            break
print("failing runs: %d of %d started; scratch %s" % (fails, i + 1, base))
