import sys; sys.path.insert(0,'build-logs/p10')
import p10lib as L
gt = L.render(L.scene('raytrace', 12, name='x'), 'g2d')
print('## E2d sampler backend (Option "sampling" "sampler"), mean over 4 seed pairs')
for h in ('raytrace', 'reyes'):
    for n in (3, 4):
        for s in ('sobol', 'pmj02', 'independent'):
            ns = []; es = []
            for k in range(4):
                ex = 'Option "sampling" "sampler" ["%s"]' % s
                a = L.render(L.scene(h, n, seed=2 * k, extra=ex, name='x'), 'a2d'); b = L.render(L.scene(h, n, seed=2 * k + 1, extra=ex, name='x'), 'b2d')
                ns.append(L.diff_stats(a, b)['rmse']); es.append(L.diff_stats(a, gt)['rmse'])
            print('%-9s spp %dx%d %-12s noise %6.2f err vs gt %6.2f' % (h, n, n, s, sum(ns) / 4, sum(es) / 4), flush=True)
