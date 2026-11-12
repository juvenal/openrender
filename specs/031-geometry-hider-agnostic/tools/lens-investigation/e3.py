# hypothesis 3: stratification and correlation of the camera slots (film, lens) per pixel, from the sampler's own draws
import subprocess, math, sys
D = 'slots/b/dump'
def run(backend, n, pix):
    out = subprocess.run([D, str(backend), str(n), str(pix)], capture_output=True, text=True).stdout.split('\n')
    px = {}
    for l in out:
        if not l: continue
        a = l.split(); key = (int(a[0]), int(a[1]))
        px.setdefault(key, []).append(tuple(float(x) for x in a[3:7]))
    return px
def pearson(x, y):
    n = len(x); mx = sum(x)/n; my = sum(y)/n
    sxy = sum((a-mx)*(b-my) for a, b in zip(x, y)); sx = math.sqrt(sum((a-mx)**2 for a in x)); sy = math.sqrt(sum((b-my)**2 for b in y))
    return sxy/(sx*sy)
def occ(points, g):
    cells = set((min(int(u*g), g-1), min(int(v*g), g-1)) for u, v in points); return len(cells)
names = ['sobol', 'pmj02', 'independent']
for n in (3, 4):
    print('## %dx%d samples per pixel (%d per pixel), 64x64 pixels' % (n, n, n*n))
    for b in range(3):
        px = run(b, n, 64)
        N = n*n
        occf = []; occl = []; occj = []; mind_l = []
        fu = []; lu = []; fv = []; lv = []
        xc = []
        for k, pts in px.items():
            f = [(p[0], p[1]) for p in pts]; l = [(p[2], p[3]) for p in pts]
            occf.append(occ(f, n)); occl.append(occ(l, n))
            # joint film.u x lens.u stratification (n x n)
            occj.append(occ([(p[0], p[2]) for p in pts], n))
            md = min(math.hypot(l[i][0]-l[j][0], l[i][1]-l[j][1]) for i in range(N) for j in range(i+1, N)); mind_l.append(md)
            fu += [p[0] for p in pts]; lu += [p[2] for p in pts]; fv += [p[1] for p in pts]; lv += [p[3] for p in pts]
        m = len(occf)
        print('%-12s occupied %dx%d cells of film %.2f lens %.2f film.u x lens.u %.2f (perfect %d, random %.2f) | lens min dist %.3f | corr(film.u,lens.u) %+.4f corr(film.v,lens.v) %+.4f corr(film.u,lens.v) %+.4f' %
              (names[b], n, n, sum(occf)/m, sum(occl)/m, sum(occj)/m, N, N*(1-(1-1/N)**N), sum(mind_l)/m,
               pearson(fu, lu), pearson(fv, lv), pearson(fu, lv)))
