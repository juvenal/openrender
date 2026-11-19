#!/usr/bin/env python3
"""files_multiset_check.py (spec 040, T045): compare two GiFiles_Unchanged reference files (tests/gi_split/reference/<platform>/files.txt).

  files_multiset_check.py <old files.txt> <new files.txt>

For every [section] the photon items (`item k P (..) N (..) flags f C (..) theta t phi p`) are compared as MULTISETS of their defined fields
P, N, C, theta, phi (flags and the item order are what the canonical sort changes on purpose). Every other line (headers, numItems, maxItems,
the cache sections' SHA-256) is compared as it is, except the per-field `field <name> sha256` lines of a photon section, which hash the item order
and so change with it. Prints one line per section and exits 1 when a multiset, a count or an unrelated line differs.
"""
import re
import sys
from collections import Counter

ITEM = re.compile(r"^item (\d+) P \((.*?)\) N \((.*?)\) flags (\d+) C \((.*?)\) theta (\d+) phi (\d+)$")


def parse(path):
    sections, cur = {}, None
    for line in open(path):
        line = line.rstrip("\n")
        m = re.match(r"^\[(.*)\]$", line)
        if m:
            cur = {"items": [], "other": []}
            sections[m.group(1)] = cur
            continue
        if cur is None:
            continue
        m = ITEM.match(line)
        if m:
            cur["items"].append((m.group(2), m.group(3), m.group(5), m.group(6), m.group(7), int(m.group(4))))
        else:
            cur["other"].append(line)
    return sections


def main():
    old, new = parse(sys.argv[1]), parse(sys.argv[2])
    bad = 0
    for name in sorted(set(old) | set(new)):
        if name not in old or name not in new:
            print("%-40s only in %s" % (name, "old" if name in old else "new"))
            bad += 1
            continue
        a, b = old[name], new[name]
        ms_a = Counter(i[:5] for i in a["items"])
        ms_b = Counter(i[:5] for i in b["items"])
        photon = bool(a["items"]) or bool(b["items"])
        keep = (lambda l: not l.startswith("field ")) if photon else (lambda l: True)
        oa = [l for l in a["other"] if keep(l)]
        ob = [l for l in b["other"] if keep(l)]
        same_ms = ms_a == ms_b
        same_other = oa == ob
        order_changed = [i[:5] for i in a["items"]] != [i[:5] for i in b["items"]]
        flags_changed = sum(1 for x, y in zip(a["items"], b["items"]) if x[5] != y[5])
        print("%-40s items %d/%d multiset %s other-lines %s order-changed %s flags-differ-at-same-index %d" % (
            name, len(a["items"]), len(b["items"]), "SAME" if same_ms else "DIFFERENT", "SAME" if same_other else "DIFFERENT",
            order_changed, flags_changed))
        if not same_ms or not same_other or len(a["items"]) != len(b["items"]):
            bad += 1
            if not same_other:
                for l in sorted(set(oa) ^ set(ob))[:6]:
                    print("    differing line: " + l[:150])
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
