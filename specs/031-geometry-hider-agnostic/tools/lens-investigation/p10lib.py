import os, subprocess, struct, math, sys
R = '/Volumes/Projects/Development/CLI/openrender-worktrees/031-geometry-hider-agnostic'
B = R + '/build'
W = R + '/build-logs/p10/work'
os.makedirs(W, exist_ok=True)
ENV = dict(os.environ, SHADERS=B + '/shaders', ORENDERHOME=B + '/tests/visual/_home',
           DISPLAYS=':'.join(B + '/src/display/' + d for d in ('file', 'rgbe', 'framebuffer', 'openexr')),
           GEOMETRIES=R + '/geometry', TEXTURES=R + '/examples/rib/tests')

CONES = '''
  Color 0 1 0
  Cone 1.5 0.75 360
  Color 1 0 0
  Translate -4 0 0
  Cone 1.5 1 360
  Color 0 0 1
  Translate 7 1 0
  Cone 1.5 0.25 360
'''

def scene(hider='raytrace', spp=3, fstop=2, focal=1, dist=5, surface=None, filt=None, seed=0, fmt=(400, 300),
          world=CONES, extra='', name='s'):
    lines = ['##RenderMan RIB-Structure 1.1', 'version 3.03', 'Hider "%s"' % hider,
             'Display "%s.tif" "file" "rgba"' % name]
    if seed:
        lines.append('Option "sampling" "seed" [%d]' % seed)
    if extra:
        lines.append(extra)
    lines += ['Projection "perspective" "fov" 45', 'PixelSamples %d %d' % (spp, spp), 'Format %d %d 1' % fmt]
    if filt:
        lines.append('PixelFilter %s' % filt)
    lines += ['DepthOfField %g %g %g' % (fstop, focal, dist), '', 'Translate 0 0 5', 'Rotate -110 1 0 0', 'Rotate 70 0 0 1', '',
              'WorldBegin']
    if surface:
        lines.append('  Surface "%s"' % surface)
    lines.append(world)
    lines.append('WorldEnd')
    return '\n'.join(lines) + '\n'

def render(rib, name):
    d = W + '/' + name
    os.makedirs(d, exist_ok=True)
    open(d + '/s.rib', 'w').write(rib)
    r = subprocess.run([B + '/src/orender/orender', 's.rib'], cwd=d, env=ENV, capture_output=True, text=True)
    tif = d + '/' + name_of(rib) + '.tif'
    if not os.path.exists(tif):
        raise RuntimeError('no image: ' + r.stdout[-300:] + r.stderr[-300:])
    bmp = d + '/o.bmp'
    subprocess.run(['sips', '-s', 'format', 'bmp', tif, '--out', bmp], capture_output=True)
    return read_bmp(bmp)

def name_of(rib):
    for l in rib.splitlines():
        if l.startswith('Display'):
            return l.split('"')[1][:-4]

def read_bmp(path):
    d = open(path, 'rb').read()
    off = struct.unpack('<I', d[10:14])[0]
    w, h = struct.unpack('<ii', d[18:26])
    top_down = h < 0
    h = abs(h)
    px = d[off:off + w * h * 4]
    # rows top-down; BGRA
    return {'w': w, 'h': h, 'px': px if top_down else None}

def rgba(img):
    return img['px']

def diff_stats(a, b, chans=(0, 1, 2, 3)):
    """RMSE, MAD-based robust sigma, p99.9 of |diff|, and the fraction of |diff| > 64 (0..255 scale) over all pixels"""
    pa, pb = a['px'], b['px']
    n = a['w'] * a['h']
    ds = []
    for i in range(n):
        o = i * 4
        for c in chans:
            ds.append(pa[o + c] - pb[o + c])
    m = len(ds)
    rmse = math.sqrt(sum(x * x for x in ds) / m)
    ad = sorted(abs(x) for x in ds)
    p999 = ad[int(0.999 * (m - 1))]
    big = sum(1 for x in ad if x > 64) / m
    return {'rmse': rmse, 'p999': p999, 'frac64': big}

def block_stats(a, b, bs=8):
    pa, pb = a['px'], b['px']
    w, h = a['w'], a['h']
    vals = []
    for by in range(0, h - bs + 1, bs):
        for bx in range(0, w - bs + 1, bs):
            s = 0
            for y in range(by, by + bs):
                o = (y * w + bx) * 4
                for x in range(bs * 4):
                    s += abs(pa[o + x] - pb[o + x])
            vals.append(s / (bs * bs * 4))
    vals.sort()
    return {'blockmax': vals[-1], 'blockmean': sum(vals) / len(vals)}

def contrast(img):
    p = img['px']; n = img['w'] * img['h']
    m = [sum(p[c::4]) / n for c in range(4)]
    return math.sqrt(sum(sum((v - m[c]) ** 2 for v in p[c::4]) for c in range(4)) / (4 * n))
