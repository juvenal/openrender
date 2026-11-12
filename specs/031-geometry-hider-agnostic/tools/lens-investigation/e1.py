import sys; sys.path.insert(0,'.')
import p10lib as L
def pair(**kw):
    a=L.render(L.scene(name='x',**kw),'e'); b=L.render(L.scene(name='x',seed=1,**kw),'e')
    return a,b
def line(tag,a,b,gt=None):
    s=L.diff_stats(a,b); bl=L.block_stats(a,b)
    e=''
    if gt is not None:
        ea=L.diff_stats(a,gt); e=' err_vs_gt rmse %.2f' % ea['rmse']
    print('%-34s noise rmse %6.2f p99.9 %3d frac>64 %.4f blockmax %5.1f blockmean %5.2f%s' % (tag,s['rmse'],s['p999'],s['frac64'],bl['blockmax'],bl['blockmean'],e),flush=True)
gt=L.render(L.scene('raytrace',12,name='x'),'gt')
print('## E1 surface (spp 3, fstop 2)')
for h in ('raytrace','reyes'):
    for surf in (None,'matte','constant'):
        a,b=pair(hider=h,spp=3,surface=surf); line('%s %s'%(h,surf or 'default'),a,b,gt)
print('## E2 samples per pixel (default surface)')
for h in ('raytrace','reyes'):
    for n in (2,3,4,6,8):
        a,b=pair(hider=h,spp=n); line('%s spp %dx%d'%(h,n,n),a,b,gt)
print('## E2b blur size (fstop), spp 3; max radius at z=10 in pixels = 36.2*A, A=0.5/fstop')
for h in ('raytrace','reyes'):
    for fs in (16,8,4,2,1):
        a,b=pair(hider=h,spp=3,fstop=fs); g=L.render(L.scene('raytrace',12,fstop=fs,name='x'),'gt2')
        line('%s fstop %d (r10=%.1fpx)'%(h,fs,36.2*0.5/fs/ (1)),a,b,g)
print('## E4 pixel filter, spp 3')
for h in ('raytrace','reyes'):
    for f in ('"box" 1 1','"triangle" 2 2','"gaussian" 2 2','"gaussian" 3 3','"catmull-rom" 4 4','"sinc" 4 4'):
        a,b=pair(hider=h,spp=3,filt=f); line('%s filter %s'%(h,f),a,b,gt)
