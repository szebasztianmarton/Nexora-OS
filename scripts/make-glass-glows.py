#!/usr/bin/env python3
"""Regenerates the soft light glows of the glass theme's backdrop (Backdrop.qml).

Each PNG is a single colour whose alpha falls off smoothly from the centre, so it can be
scaled up a lot without visible steps. Pure standard library (zlib), deterministic output:

    scripts/make-glass-glows.py apps/console-ui/qml/images
"""
import struct
import sys
import zlib

SIZE = 128
GLOWS = {
    "glow-blue.png": (79, 124, 255),
    "glow-violet.png": (155, 92, 255),
    "glow-teal.png": (34, 195, 195),
}


def png(width, height, rows):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + row for row in rows)
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)  # 8-bit RGBA
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def glow(rgb):
    centre = (SIZE - 1) / 2
    rows = []
    for y in range(SIZE):
        row = bytearray()
        for x in range(SIZE):
            d = min(1.0, (((x - centre) ** 2 + (y - centre) ** 2) ** 0.5) / centre)
            t = 1.0 - d
            alpha = round(255 * t * t * (3 - 2 * t))  # smoothstep falloff, 0 at the edge
            row += bytes((*rgb, alpha))
        rows.append(bytes(row))
    return png(SIZE, SIZE, rows)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    for name, rgb in GLOWS.items():
        with open(f"{out}/{name}", "wb") as f:
            f.write(glow(rgb))


if __name__ == "__main__":
    main()
