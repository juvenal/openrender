import sys; sys.path.insert(0,'build-logs/p10')
import p10lib as L
exec(open('build-logs/p10/e5.py').read().split("scenes = {")[0].split("import p10lib as L")[1])
FGS = LIGHT + '  AttributeBegin\n    Surface "constant"\n    Color 1 1 1\n    Translate 0 0 5\n    Polygon "P" [-6 -4 0  6 -4 0  6 4 0  -6 4 0]\n  AttributeEnd\n' \
      '  AttributeBegin\n    Surface "constant"\n    Color 0 0 0\n    Translate 0 0 5\n    Disk 0 0.02 360\n  AttributeEnd\n'
print('## E7 what a wider filter costs in focus: a 2.3-pixel-wide black dot at the focal distance (converged, 16x16 raytrace), min R at the dot and the 20/80 edge width of the dot')
for f in (None, '"gaussian" 2 2', '"gaussian" 3 3'):
    t = sc('raytrace', 16, FGS, name='x')
    if f: t = t.replace('Format 400 300 1', 'Format 400 300 1\nPixelFilter %s' % f)
    img = L.render(t, 'e7')
    w = img['w']; p = img['px']
    row = [p[(150 * w + x) * 4 + 2] for x in range(190, 211)]  # R channel across the dot (BGRA: index 2)
    print('%-16s min R at dot %3d  row %s' % (f or 'default (catmull-rom 2)', min(row), row), flush=True)
