#!/usr/bin/env python3
"""Build the resource-pack test fixtures (our own content; no Mojang files).

Writes tests/data/testpack/ (folder pack) and tests/data/testpack.zip (same files,
mixed stored/deflated entries) used by tests and visual checks:
  stone.png      32x32 "HD" blue/white checker      -> HD cell size + upscaling
  sand.png       16x64 strip, 4 flat frames          -> animation (with .mcmeta,
                 red, green, blue, yellow               frametime 5)
  dirt.png       16x16 flat orange                   -> plain override
  hello.txt      "hello resource pack"               -> zip text round-trip
Run: tools/make_test_pack.py
"""
import struct
import zipfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "tests" / "data"
TEX = "assets/minecraft/textures/block/"


def png(width, height, pixel):
    raw = b"".join(b"\x00" + b"".join(bytes(pixel(x, y)) for x in range(width))
                   for y in range(height))

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload)))

    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


FRAMES = [(220, 40, 40, 255), (40, 200, 40, 255), (40, 60, 220, 255), (230, 210, 40, 255)]

FILES = {
    "pack.mcmeta": b'{"pack": {"pack_format": 34, "description": "MinecraftClone test pack"}}\n',
    TEX + "stone.png": png(32, 32, lambda x, y: (40, 80, 220, 255) if (x // 4 + y // 4) % 2
                           else (240, 240, 240, 255)),
    TEX + "sand.png": png(16, 64, lambda x, y: FRAMES[y // 16]),
    TEX + "sand.png.mcmeta": b'{\n  "animation": {\n    "frametime": 5\n  }\n}\n',
    TEX + "dirt.png": png(16, 16, lambda x, y: (240, 130, 30, 255)),
    "hello.txt": b"hello resource pack\n" * 20,
}


def main():
    folder = ROOT / "testpack"
    for name, data in FILES.items():
        path = folder / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    with zipfile.ZipFile(ROOT / "testpack.zip", "w") as z:
        for name, data in FILES.items():
            # pack.mcmeta stored, everything else deflated: covers both methods.
            method = zipfile.ZIP_STORED if name == "pack.mcmeta" else zipfile.ZIP_DEFLATED
            info = zipfile.ZipInfo(name, date_time=(2026, 10, 6, 0, 0, 0))
            info.compress_type = method
            z.writestr(info, data)
    print(f"wrote {folder} and {ROOT / 'testpack.zip'}")


if __name__ == "__main__":
    main()
