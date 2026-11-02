#!/bin/sh
# The construction-window sweep of spec 038 (tasks T018, T072; research R7, decision Q5): the diagnostics
# (error / warning / info / CODE_* reports) that can be raised while a CRendererContext is constructed.
#
#   ctor_reports_sweep.sh <source-dir>
#
# CRendererContext's constructor runs CRenderer::beginRenderer(), which calls, in this order, the functions
# listed in FUNCTIONS below (the list is the call sequence of beginRenderer() at the time of writing; re-read
# beginRenderer() when it changes). The script prints every line of those bodies that contains a report call,
# as "<file>:<line>: <text>". It is a static listing of the bodies themselves: a report raised inside a
# function they call (for example the shader-services registration's own callees) is not followed.
# Read-only; run through cpujob like every tool run.
SRC="${1:?usage: ctor_reports_sweep.sh <source-dir>}"
exec python3 -I - "$SRC" <<'PY'
import os, re, sys

src = sys.argv[1]
ri = os.path.join(src, 'src/ri/render')
# (file, qualified function name as defined)
FUNCTIONS = [
    ('rendererContext.cpp', 'CRendererContext::CRendererContext'),
    ('renderer.cpp', 'CRenderer::beginRenderer'),
    ('rendererMutexes.cpp', 'CRenderer::initMutexes'),
    ('rendererFiles.cpp', 'CRenderer::initFiles'),
    ('rendererDeclarations.cpp', 'CRenderer::initDeclarations'),
    ('rendererShadingServices.cpp', 'rendererShadingServicesInit'),
    ('rendererProcedures.cpp', 'rendererProceduresInit'),
    ('rendererNetwork.cpp', 'CRenderer::initNetwork'),
]
report = re.compile(r'\b(error|warning|info|CODE_[A-Z_]+)\b')
total = 0
for fname, func in FUNCTIONS:
    path = os.path.join(ri, fname)
    try:
        lines = open(path, errors='replace').read().split('\n')
    except OSError:
        print('MISSING %s' % path)
        continue
    start = None
    for i, line in enumerate(lines):
        if re.match(r'^(?:.*[\s*&])?%s\s*\(' % re.escape(func), line):
            start = i
            break
    if start is None:
        print('NOT-FOUND %s in %s' % (func, fname))
        continue
    depth, seen_open, hits = 0, False, 0
    print('== %s (%s:%d)' % (func, fname, start + 1))
    for i in range(start, len(lines)):
        line = lines[i]
        code = line.split('//', 1)[0]
        if seen_open and report.search(code):
            print('%s:%d: %s' % (fname, i + 1, line.strip()))
            hits += 1
        depth += code.count('{') - code.count('}')
        if '{' in code:
            seen_open = True
        if seen_open and depth <= 0:
            break
    total += hits
    if hits == 0:
        print('(no report call in the body)')
print('%d report line(s) in the listed bodies' % total)
PY
