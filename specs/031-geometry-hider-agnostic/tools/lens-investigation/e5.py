# the scratch scenes of US7 AS4: a small bright defocused highlight, and a defocused foreground over a sharp background
import sys; sys.path.insert(0,'.')
import p10lib as L
LIGHT = '  LightSource "distantlight" 1 "from" [0 0 0] "to" [0.3 0.3 1] "intensity" 1.5\n'
# the camera looks down +z through the three-line transform of the tracked scenes; this scene uses its own: no rotation
def sc(hider, spp, world, seed=0, fstop=2, name='x', dist=5):
    lines = ['##RenderMan RIB-Structure 1.1', 'version 3.03', 'Hider "%s"' % hider, 'Display "%s.tif" "file" "rgba"' % name]
    if seed: lines.append('Option "sampling" "seed" [%d]' % seed)
    lines += ['Projection "perspective" "fov" 45', 'PixelSamples %d %d' % (spp, spp), 'Format 400 300 1',
              'DepthOfField %g 1 %g' % (fstop, dist), 'Translate 0 0 0', 'WorldBegin', world, 'WorldEnd']
    return '\n'.join(lines) + '\n'
# A: a small bright sphere (a specular highlight source) far behind the focal plane, on black: the bokeh disk
BOKEH_CONST = '  Surface "constant"\n  Color 1 1 1\n  Translate 0 0 12\n  Sphere 0.15 -0.15 0.15 360\n'
# B: a glossy sphere with a tight highlight, defocused (z 12), lit: the highlight moves with the lens point in the raytracer
BOKEH_GLOSSY = LIGHT + '  Surface "plastic" "Ka" 0.1 "Kd" 0.5 "Ks" 1 "roughness" 0.03\n  Color 0.8 0.2 0.2\n  Translate 0 0 12\n  Sphere 1.0 -1.0 1.0 360\n'
# C: a defocused foreground (z 2.5) cone over a sharp background plane at the focal distance (z 5)
FG = LIGHT + '  AttributeBegin\n    Surface "matte"\n    Color 0.9 0.9 0.2\n    Translate 0 0 5\n    Polygon "P" [-6 -4 0  6 -4 0  6 4 0  -6 4 0]\n  AttributeEnd\n' \
     '  AttributeBegin\n    Surface "matte"\n    Color 0.1 0.2 0.8\n    Translate 0.2 0 2.5\n    Rotate -90 1 0 0\n    Cone 1.2 0.5 360\n  AttributeEnd\n' \
     '  AttributeBegin\n    Surface "constant"\n    Color 0 0 0\n    Translate 0 0.8 5\n    Disk 0 0.08 360\n    Translate 0 -1.6 0\n    Disk 0 0.08 360\n  AttributeEnd\n'
scenes = {'bokeh-constant': BOKEH_CONST, 'bokeh-glossy': BOKEH_GLOSSY, 'foreground': FG}
for nm, w in scenes.items():
    gt = L.render(sc('raytrace', 16, w, name='x'), 'g5')
    for h in ('raytrace', 'reyes'):
        a = L.render(sc(h, 3, w, name='x'), 'a5'); b = L.render(sc(h, 3, w, seed=1, name='x'), 'b5')
        s = L.diff_stats(a, b); e = L.diff_stats(a, gt); bl = L.block_stats(a, b)
        print('%-15s %-9s noise rmse %6.2f p99.9 %3d frac>64 %.4f blockmax %5.1f | err vs 16x16 raytrace gt: rmse %6.2f' % (nm, h, s['rmse'], s['p999'], s['frac64'], bl['blockmax'], e['rmse']), flush=True)
    # keep the images for inspection
    import shutil, os
    for tag, hider in (('rt', 'raytrace'), ('ry', 'reyes')):
        L.render(sc(hider, 3, w, name='x'), 'keep-%s-%s' % (nm, tag))
    L.render(sc('raytrace', 16, w, name='x'), 'keep-%s-gt' % nm)
