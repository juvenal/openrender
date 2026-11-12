import sys; sys.path.insert(0,'build-logs/p10')
import p10lib as L
R = L.R
print('## E6 the tracked scenes at full size (800x600, PixelSamples 3 3), default filter (catmull-rom 2) and gaussian 2 2 / 3 3')
for nm in ('camera-dof-raytrace', 'camera-dof-reyes'):
    base = open(R + '/examples/rib/tests/%s.rib' % nm).read()
    for tag, flt in (('default', None), ('gaussian 2 2', 'PixelFilter "gaussian" 2 2'), ('gaussian 3 3', 'PixelFilter "gaussian" 3 3')):
        def mk(seed):
            t = base.replace('Format 800 600 1', 'Format 800 600 1' + ('\n' + flt if flt else ''))
            if seed: t = t.replace('Projection', 'Option "sampling" "seed" [%d]\nProjection' % seed, 1)
            return t
        a = L.render(mk(0), 'a6'); b = L.render(mk(1), 'b6')
        s = L.diff_stats(a, b); bl = L.block_stats(a, b)
        print('%-22s %-13s noise rmse %6.2f p99.9 %3d frac>64 %.4f blockmax %5.1f blockmean %5.2f' % (nm, tag, s['rmse'], s['p999'], s['frac64'], bl['blockmax'], bl['blockmean']), flush=True)
