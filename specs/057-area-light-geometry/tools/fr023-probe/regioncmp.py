# regioncmp.py A.bmp B.bmp x0 y0 x1 y1: mean luminance of a pixel region in two images and their mean/max abs difference.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bmpload import load

_, _, a = load(sys.argv[1])
_, _, b = load(sys.argv[2])
x0, y0, x1, y1 = map(int, sys.argv[3:7])
s = n = mx = sa = sb = 0.0
for y in range(y0, y1):
    for x in range(x0, x1):
        la, lb = sum(a(x, y)) / 3.0, sum(b(x, y)) / 3.0
        d = abs(la - lb)
        s += d; n += 1; mx = max(mx, d); sa += la; sb += lb
print(f"region {x0},{y0}-{x1},{y1}: meanA {sa/n:.2f} meanB {sb/n:.2f} meanAbsDiff {s/n:.3f} maxDiff {mx:.1f}")
