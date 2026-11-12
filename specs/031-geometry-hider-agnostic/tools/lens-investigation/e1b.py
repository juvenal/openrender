import sys; sys.path.insert(0,'.')
import p10lib as L
LIT = '  LightSource "distantlight" 1 "from" [0 0 0] "to" [0.3 0.3 1] "intensity" 1.5\n'
def w(surface, light):
    return (LIT if light else '') + (('  Surface "%s"\n' % surface) if surface else '') + L.CONES
print('## E1b noise relative to the contrast of the scene (own 12x12 raytrace ground truth per surface), spp 3')
for name, surface, light in (('default', None, False), ('constant', 'constant', False), ('matte lit', 'matte', True), ('plastic lit', 'plastic', True)):
    world = w(surface, light)
    gt = L.render(L.scene('raytrace', 12, world=world, name='x'), 'g1')
    c = L.contrast(gt)
    for h in ('raytrace', 'reyes'):
        a = L.render(L.scene(h, 3, world=world, name='x'), 'a1'); b = L.render(L.scene(h, 3, world=world, seed=1, name='x'), 'b1')
        s = L.diff_stats(a, b); e = L.diff_stats(a, gt)
        print('%-12s %-9s contrast %6.1f noise rmse %6.2f (%.3f of contrast) err vs gt %6.2f (%.3f) p99.9 %3d' % (name, h, c, s['rmse'], s['rmse'] / c, e['rmse'], e['rmse'] / c, s['p999']), flush=True)
