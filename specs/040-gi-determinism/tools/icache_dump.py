#!/usr/bin/env python3
"""icache_dump.py (spec 040, T012): parse an irradiance cache file (.icache) and print its record count and per-node counts.

  icache_dump.py [--records] [--nodes] <file.icache>

Layout (contracts/files-and-wire.md; src/ri/texture/irradianceCache.cpp writeNode/readNode, portable_io.h little-endian):
  ropen header   magic (4), version[4] (16), strlen(name) (4), the type name and its NUL
  int32 maxDepth
  node, depth-first: float centre[3], float side, int32 count, count x record, int32 hasChild[8], then the children of the
  non-zero hasChild entries in index order
  record, 56 floats (224 bytes) field by field: P[3], N[3], irradiance[3], coverage, envdir[3], gP[21], gR[21], dP
No key is stored (FR-022). Python 3 standard library only (there is no numpy).
  --records   print every record's P, N and dP in file order
  --nodes     print every node's depth, centre, side and count
"""
import struct
import sys

REC = 56 * 4


def parse(path):
    d = open(path, "rb").read()
    n = struct.unpack_from("<i", d, 20)[0]
    off = 24 + n + 1
    max_depth = struct.unpack_from("<i", d, off)[0]
    off += 4
    nodes, records = [], []

    def node(off, depth):
        centre = struct.unpack_from("<3f", d, off)
        side = struct.unpack_from("<f", d, off + 12)[0]
        count = struct.unpack_from("<i", d, off + 16)[0]
        off += 20
        nodes.append((depth, centre, side, count))
        for _ in range(count):
            f = struct.unpack_from("<56f", d, off)
            records.append((f[0:3], f[3:6], f[55]))
            off += REC
        has = struct.unpack_from("<8i", d, off)
        off += 32
        for h in has:
            if h:
                off = node(off, depth + 1)
        return off

    end = node(off, 0)
    return d, max_depth, nodes, records, end


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    if len(args) != 1:
        sys.exit(__doc__)
    d, max_depth, nodes, records, end = parse(args[0])
    print("file %s: %d bytes, maxDepth %d, %d nodes, %d records%s" % (
        args[0], len(d), max_depth, len(nodes), len(records), "" if end == len(d) else " (parse ended at %d of %d bytes)" % (end, len(d))))
    if "--nodes" in flags:
        for depth, centre, side, count in nodes:
            print("node depth %d centre (%g %g %g) side %g count %d" % ((depth,) + centre + (side, count)))
    if "--records" in flags:
        for p, nrm, dp in records:
            print("P (%.9g %.9g %.9g) N (%.9g %.9g %.9g) dP %.9g" % (p + nrm + (dp,)))


if __name__ == "__main__":
    main()
