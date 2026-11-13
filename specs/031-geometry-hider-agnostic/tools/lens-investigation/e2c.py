import sys; sys.path.insert(0,'build-logs/p10')
import p10lib as L
gt = L.render(L.scene('raytrace', 12, name='x'), 'g2c')
print('## E2c raytrace/reyes noise and error vs 12x12 ground truth, mean over 4 seed pairs (rmse, 0..255 scale)')
for h in ('raytrace', 'reyes'):
    for n in (2, 3, 4, 5, 6, 7, 8):
        ns = []; es = []
        for k in range(4):
            a = L.render(L.scene(h, n, seed=2 * k, name='x'), 'a2c'); b = L.render(L.scene(h, n, seed=2 * k + 1, name='x'), 'b2c')
            ns.append(L.diff_stats(a, b)['rmse']); es.append(L.diff_stats(a, gt)['rmse'])
        print('%-9s spp %dx%d (%2d samples, pow2 %-5s) noise %6.2f (min %5.2f max %5.2f) err vs gt %6.2f' % (h, n, n, n * n, 'yes' if (n * n & (n * n - 1)) == 0 else 'no', sum(ns) / 4, min(ns), max(ns), sum(es) / 4), flush=True)
