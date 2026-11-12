import sys; sys.path.insert(0,'build-logs/p10')
import p10lib as L
exec(open('build-logs/p10/e5.py').read().split("scenes = {")[0].split("import p10lib as L")[1])
TIGHT = '  LightSource "pointlight" 1 "from" [2 2 6] "intensity" 60\n  Surface "plastic" "Ka" 0.05 "Kd" 0.3 "Ks" 4 "roughness" 0.02\n  Color 0.8 0.2 0.2\n  Translate 0 0 12\n  Sphere 1.0 -1.0 1.0 360\n'
gt = L.render(sc('raytrace', 16, TIGHT, name='x'), 'g5b')
mx = lambda img: max(img['px'][c::4][i] for c in range(3) for i in range(0, img['w'] * img['h'], 1) if False) if False else max(max(img['px'][c::4]) for c in range(3))
for h in ('raytrace', 'reyes'):
    for spp in (3, 6):
        a = L.render(sc(h, spp, TIGHT, name='x'), 'a5b'); b = L.render(sc(h, spp, TIGHT, seed=1, name='x'), 'b5b')
        s = L.diff_stats(a, b); e = L.diff_stats(a, gt)
        print('tight-highlight %-9s spp %d noise rmse %6.2f p99.9 %3d frac>64 %.4f | err vs gt %6.2f' % (h, spp, s['rmse'], s['p999'], s['frac64'], e['rmse']), flush=True)
L.render(sc('raytrace', 3, TIGHT, name='x'), 'keep-tight-rt'); L.render(sc('reyes', 3, TIGHT, name='x'), 'keep-tight-ry'); L.render(sc('raytrace', 16, TIGHT, name='x'), 'keep-tight-gt')
