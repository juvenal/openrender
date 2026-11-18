#!/usr/bin/env python3
"""gpm_multiset_check.py (spec 040, T045): compare two photon map files (.gpm) as MULTISETS of their photons' defined fields.

  gpm_multiset_check.py <old.gpm> <new.gpm>

The items 1..numItems are compared by (P, N, C, theta, phi) as raw bytes, ignoring `flags` and the order (what the canonical sort and the
balanced kd-tree change on purpose); numItems, the bound, the two matrices and maxPower are compared as they are. Also reports whether the
new file's item 0 and padding bytes are zero and how many items changed position and flags. Exit 1 on any difference of the compared parts.
"""
import struct
import sys
from collections import Counter

ITEM = 44


def load(path):
    d = open(path, "rb").read()
    n = struct.unpack_from("<i", d, 20)[0]
    h = 24 + n + 1
    num, mx = struct.unpack_from("<ii", d, h)
    items = h + 8
    tr = items + ITEM * (num + 1)
    return d, num, mx, items, tr


def main():
    a, na, ma, ia, ta = load(sys.argv[1])
    b, nb, mb, ib, tb = load(sys.argv[2])
    bad = 0

    def item(d, base, i):
        o = base + ITEM * i
        return d[o:o + 24] + d[o + 28:o + 42]  # P N | C theta phi (flags 24-25 and the padding left out)

    print("numItems %d / %d, maxItems %d / %d" % (na, nb, ma, mb))
    if na != nb:
        bad += 1
    msa = Counter(item(a, ia, i) for i in range(1, na + 1))
    msb = Counter(item(b, ib, i) for i in range(1, nb + 1))
    print("multiset of (P, N, C, theta, phi): %s" % ("SAME" if msa == msb else "DIFFERENT"))
    bad += msa != msb
    print("trailer (bound, matrices, maxPower): %s" % ("SAME" if a[ta:] == b[tb:] else "DIFFERENT"))
    bad += a[ta:] != b[tb:]
    moved = sum(1 for i in range(1, min(na, nb) + 1) if item(a, ia, i) != item(b, ib, i))
    flags = sum(1 for i in range(1, min(na, nb) + 1) if a[ia + ITEM * i + 24:ia + ITEM * i + 26] != b[ib + ITEM * i + 24:ib + ITEM * i + 26])
    print("items at a different position: %d, with a different flags value at the same index: %d" % (moved, flags))
    pads = [k for k in range(ITEM) if b[ib + k] != 0] + [k for i in range(1, nb + 1) for k in (26, 27, 42, 43) if b[ib + ITEM * i + k] != 0]
    print("new file: item 0 and the padding bytes non-zero: %d" % len(pads))
    bad += len(pads) != 0
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
