#!/usr/bin/env python3
"""Render baseline.json (from tools/baseline.py) as the tables in baseline-timings.md.

Usage: python3 baseline_report.py <baseline.json>   (prints markdown to stdout)
"""
import json, sys

r = json.load(open(sys.argv[1]))

print("| Scene | Master time (s) |")
print("|---|---:|")
for name, t in r["timings"].items():
    print(f"| `{name}` | {t:.2f} |")

print()
print("| Planned check | Scene / leg | Master result | Key values |")
print("|---|---|---|---|")
for m in r["metrics"]:
    out = m["out"]
    fields = dict(f.split("=", 1) for f in out.split(",") if "=" in f)
    result = fields.pop("result", "ERROR")
    mode = fields.pop("mode", "?")
    for k in ("tolerance_px", "opaque", "threshold", "min", "max"):
        fields.pop(k, None)
    test = m["test"].split(" ", 1)
    task, leg = test[0], (test[1] if len(test) > 1 else "")
    values = ", ".join(f"{k}={v}" for k, v in fields.items())
    print(f"| {task} `{mode}` | {leg} | {result} | {values} |")
