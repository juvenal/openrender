# Minimal 24/32-bit BMP reader (no third-party modules). Convert a TIFF first with
# `sips -s format bmp in.tif --out in.bmp` (macOS). load(path) -> (width, height, get(x, y) -> (r, g, b)).
import struct


def load(path):
    b = open(path, 'rb').read()
    off = struct.unpack_from('<I', b, 10)[0]
    w, h = struct.unpack_from('<ii', b, 18)
    bpp = struct.unpack_from('<H', b, 28)[0] // 8
    row = ((w * bpp + 3) // 4) * 4
    flip = h > 0
    h = abs(h)

    def get(x, y):
        yy = (h - 1 - y) if flip else y
        o = off + yy * row + x * bpp
        return b[o + 2], b[o + 1], b[o]

    return w, h, get
