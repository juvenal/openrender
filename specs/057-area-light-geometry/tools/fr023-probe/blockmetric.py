# blockmetric.py A.bmp B.bmp: the visual suite's metric (tests/visual/test_visual_render.cpp): the largest
# per-channel difference of 8x8 block averages.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bmpload import load

w, h, a = load(sys.argv[1])
_, _, b = load(sys.argv[2])
mx = 0.0
for by in range(0, h, 8):
    for bx in range(0, w, 8):
        sa = [0, 0, 0]; sb = [0, 0, 0]; n = 0
        for y in range(by, min(by + 8, h)):
            for x in range(bx, min(bx + 8, w)):
                ca, cb = a(x, y), b(x, y); n += 1
                for c in range(3):
                    sa[c] += ca[c]; sb[c] += cb[c]
        for c in range(3):
            mx = max(mx, abs(sa[c] - sb[c]) / n)
print(f"blockAvgMaxDiff {mx:.2f}")
