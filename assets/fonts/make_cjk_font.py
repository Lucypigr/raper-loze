#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Packs a pixel TrueType font into cubic11.cjk, the bitmap glyphs d2d draws
# for characters its D2 fonts lack (Chinese text; D2's own fonts stop at
# 0xFF). Pillow renders each glyph at its pixel size with antialiasing off.
#
#   fontforge -lang=py -c 'import fontforge,sys; fontforge.open(sys.argv[1]).generate("cubic11.ttf")' \
#       Cubic-11/source/Cubic_11_1.500_R.sfd        # https://github.com/ACh-K/Cubic-11 (OFL)
#   python3 make_cjk_font.py cubic11.ttf cubic11.cjk
#
# Format (little-endian): "D2CJK1\0\0", u8 height, u8 row_bytes, u32 count,
# then count records sorted by code: u16 code, u8 advance, height * row_bytes
# bitmap bytes (rows top-down, MSB = leftmost pixel).
import struct
import sys

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

SIZE = 12        # Cubic 11's grid: 1200 units per em, 100 per pixel
HEIGHT = 12
ROW_BYTES = 2    # glyphs are at most 16 pixels wide


def main(ttf_path, out_path):
    codes = sorted(code for code in TTFont(ttf_path).getBestCmap() if 0xFF < code <= 0xFFFF)
    font = ImageFont.truetype(ttf_path, SIZE)
    records = []
    for code in codes:
        char = chr(code)
        advance = int(round(font.getlength(char)))
        image = Image.new("1", (ROW_BYTES * 8, HEIGHT), 0)
        draw = ImageDraw.Draw(image)
        draw.fontmode = "1"
        draw.text((0, 0), char, font=font, fill=1)
        bitmap = bytearray()
        for y in range(HEIGHT):
            row = 0
            for x in range(ROW_BYTES * 8):
                if image.getpixel((x, y)):
                    row |= 0x8000 >> x
            bitmap += row.to_bytes(ROW_BYTES, "big")
        if not any(bitmap) and char.strip():
            continue   # a code point the font maps but draws nothing for
        records.append(struct.pack("<HB", code, min(advance, 255)) + bytes(bitmap))
    with open(out_path, "wb") as out:
        out.write(b"D2CJK1\0\0" + struct.pack("<BBI", HEIGHT, ROW_BYTES, len(records)))
        out.writelines(records)
    print(f"{out_path}: {len(records)} glyphs")


if __name__ == "__main__":
    main(*sys.argv[1:3])
