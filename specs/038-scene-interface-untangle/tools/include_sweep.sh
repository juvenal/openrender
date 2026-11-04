#!/bin/sh
# The include sweep of spec 038 (tasks T018/T019): every #include of the sources that may not reach the
# renderer core, resolved through the target's include path and classified by tests/architecture/domain-map.txt.
#
#   include_sweep.sh <build-dir> <source-dir>
#
# Sources: src/ri/parse/*, src/ri/output/*, the generated parsers (rib_generated.cpp, ifexpr_generated.cpp,
# lex.rib.cpp, lex.if.cpp) and every file under src/preview/libribpreview. Include directories come from the
# generated flags.make of ribOut_obj (the RIB writer's include path) and of the ribpreview target, so the
# resolution is the compiler's own: the including file's directory first, then the -I / -isystem list in order.
# A "name" or <name> that resolves to nothing under the source or build tree (a system header) is not listed.
#
# Output: one "INCLUDE <file> -> <resolved path> [<class>]" line per resolved include, then
# "CORE <file> -> <resolved path>" for every one classified Core, then a count.
# Read-only; run through cpujob like every tool run.
BUILD="${1:?usage: include_sweep.sh <build-dir> <source-dir>}"
SRC="${2:?usage: include_sweep.sh <build-dir> <source-dir>}"
exec python3 -I - "$BUILD" "$SRC" <<'PY'
import os, re, sys

build, src = os.path.realpath(sys.argv[1]), os.path.realpath(sys.argv[2])

def load_map(path):
    files, prefixes = {}, []
    for line in open(path):
        line = line.split('#', 1)[0].split()
        if len(line) == 3:
            kind, pat, cls = line
            (files.__setitem__(pat, cls) if kind == 'file' else prefixes.append((pat, cls)))
    return files, prefixes

files, prefixes = load_map(os.path.join(src, 'tests/architecture/domain-map.txt'))

def classify(rel):
    if rel in files:
        return files[rel]
    best = None
    for pat, cls in prefixes:
        if rel.startswith(pat) and (best is None or len(pat) > len(best[0])):
            best = (pat, cls)
    return best[1] if best else '?'

def mapped(path):
    path = os.path.realpath(path)
    if path.startswith(src + '/'):
        return path[len(src) + 1:]
    if path.startswith(build + '/'):
        return '@build/' + path[len(build) + 1:]
    return None

def include_dirs(flags_make):
    dirs = []
    try:
        text = open(flags_make).read()
    except OSError:
        return dirs
    m = re.search(r'^CXX_INCLUDES = (.*)$', text, re.M)
    if not m:
        return dirs
    words = m.group(1).split()
    i = 0
    while i < len(words):
        w = words[i]
        if w.startswith('-I') and len(w) > 2:
            dirs.append(w[2:])
        elif w in ('-I', '-isystem') and i + 1 < len(words):
            dirs.append(words[i + 1]); i += 1
        i += 1
    return dirs

ri_flags = include_dirs(os.path.join(build, 'src/ri/CMakeFiles/ribOut_obj.dir/flags.make'))
pre_flags = include_dirs(os.path.join(build, 'src/preview/libribpreview/CMakeFiles/ribpreview.dir/flags.make'))

todo = []
for d in ('src/ri/parse', 'src/ri/output'):
    for root, _, names in os.walk(os.path.join(src, d)):
        for n in sorted(names):
            if n.endswith(('.h', '.hpp', '.cpp', '.y', '.l')):
                todo.append((os.path.join(root, n), ri_flags))
for n in ('rib_generated.cpp', 'ifexpr_generated.cpp', 'lex.rib.cpp', 'lex.if.cpp'):
    p = os.path.join(build, 'src/ri', n)
    if os.path.exists(p):
        todo.append((p, ri_flags))
for root, _, names in os.walk(os.path.join(src, 'src/preview/libribpreview')):
    for n in sorted(names):
        if n.endswith(('.h', '.hpp', '.cpp')):
            todo.append((os.path.join(root, n), pre_flags))

inc_re = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
core, total = [], 0
for path, dirs in sorted(todo):
    here = mapped(path)
    for line in open(path, errors='replace'):
        m = inc_re.match(line)
        if not m:
            continue
        name = m.group(2)
        found = None
        cands = ([os.path.dirname(path)] if m.group(1) == '"' else []) + dirs
        for d in cands:
            p = os.path.join(d, name)
            if os.path.isfile(p):
                found = p
                break
        if not found:
            continue
        rel = mapped(found)
        if rel is None:
            continue
        cls = classify(rel)
        total += 1
        print('INCLUDE %s -> %s [%s]' % (here, rel, cls))
        if cls == 'Core':
            core.append((here, rel))
for here, rel in sorted(set(core)):
    print('CORE %s -> %s' % (here, rel))
print('%d resolved includes, %d of them Core' % (total, len(core)))
PY
