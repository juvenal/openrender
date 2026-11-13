import sys; sys.path.insert(0,'build-logs/p10')
import p10lib as L
print('## E2e noise vs samples per pixel for three filters (4 seed pairs, noise rmse, 0..255)')
for f in ('"box" 1 1', '"gaussian" 2 2', '"catmull-rom" 2 2'):
    for h in ('raytrace', 'reyes'):
        row = []
        for n in (2, 3, 4, 5, 6):
            ns = []
            for k in range(2):
                a = L.render(L.scene(h, n, seed=2 * k, filt=f, name='x'), 'a2e'); b = L.render(L.scene(h, n, seed=2 * k + 1, filt=f, name='x'), 'b2e')
                ns.append(L.diff_stats(a, b)['rmse'])
            row.append('%dx%d %6.2f' % (n, n, sum(ns) / 2))
        print('%-18s %-9s ' % (f, h) + ' | '.join(row), flush=True)
