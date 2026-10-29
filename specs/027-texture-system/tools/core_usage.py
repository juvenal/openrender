#!/usr/bin/env python3
# core_usage.py <source-root> <dir>: lists CRenderer::/CRendererContext:: references per member in <dir>,
# with comments and string literals blanked exactly as archcheck metrics.cpp does (spec 027 plan stage).
# archcheck reports only the per-class totals; this prints the members behind them.
import re,sys,os,collections
def strip(s):
    out=list(s);i=0;n=len(s)
    def blank(a,b):
        for k in range(a,min(b,n)):
            if out[k]!='\n': out[k]=' '
    while i<n:
        c=s[i]
        if c=='/' and i+1<n and s[i+1]=='/':
            e=s.find('\n',i); e=n if e<0 else e; blank(i,e); i=e
        elif c=='/' and i+1<n and s[i+1]=='*':
            e=s.find('*/',i+2); e=n if e<0 else e+2; blank(i,e); i=e
        elif (c in '"\'') and i>0 and s[i-1]=='\\': i+=1
        elif c in '"\'':
            j=i+1
            while j<n and s[j]!=c and s[j]!='\n':
                if s[j]=='\\' and j+1<n: j+=1
                j+=1
            blank(i+1,j); i=j+1
        else: i+=1
    return ''.join(out)
root=sys.argv[1]; d=sys.argv[2]
refs=0; members=collections.Counter(); perfile=collections.Counter()
for f in sorted(os.listdir(os.path.join(root,d))):
    p=os.path.join(root,d,f)
    if not f.endswith(('.cpp','.h')): continue
    t=strip(open(p,errors='replace').read())
    for m in re.finditer(r'(?<![A-Za-z0-9_:])(CRendererContext::|CRenderer::)([A-Za-z0-9_]+)',t):
        refs+=1; members[m.group(1)+m.group(2)]+=1; perfile[f]+=1
print("refs",refs,"distinct",len(members))
for k,v in sorted(members.items()): print(f"  {v:3d} {k}")
for k,v in sorted(perfile.items()): print(f"  file {k}: {v}")
