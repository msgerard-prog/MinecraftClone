#!/usr/bin/env python3
"""Place RGB/RGBA PNGs side by side (same height) for one-look visual comparisons.

Usage: tools/stitch_png.py out.png a.png b.png [c.png ...]
Standard library only (zlib + struct).
"""
import struct
import sys
import zlib


def read_png(path):
    data = open(path, "rb").read()
    i, idat = 8, b""
    while i < len(data):
        n, kind = struct.unpack(">I4s", data[i:i + 8])
        if kind == b"IHDR":
            w, h, _, color = struct.unpack(">IIBB", data[i + 8:i + 18])
        elif kind == b"IDAT":
            idat += data[i + 8:i + 8 + n]
        i += 12 + n
    bpp = 4 if color == 6 else 3
    raw, stride = zlib.decompress(idat), w * bpp
    rows, prev, o = [], bytearray(stride), 0
    for _ in range(h):
        f, line = raw[o], bytearray(raw[o + 1:o + 1 + stride])
        o += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        # Drop alpha so every image is RGB.
        rows.append(bytes(line) if bpp == 3 else bytes(v for k, v in enumerate(line) if k % 4 != 3))
    return w, h, rows


def main():
    out, inputs = sys.argv[1], [read_png(p) for p in sys.argv[2:]]
    h = min(img[1] for img in inputs)
    sep = b"\xff\xff\xff" * 4
    rows = [sep.join(img[2][y] for img in inputs) for y in range(h)]
    width = sum(img[0] for img in inputs) + 4 * (len(inputs) - 1)

    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))

    raw = b"".join(b"\x00" + r for r in rows)
    with open(out, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw)))
        f.write(chunk(b"IEND", b""))
    print(out)


if __name__ == "__main__":
    main()
