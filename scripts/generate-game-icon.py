#!/usr/bin/env python3
"""Rebuild the flat FC controller icon in the official LVGL RGB565A8 format."""
import struct
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SCALE = 4
image = Image.new("RGBA", (188 * SCALE, 150 * SCALE))
draw = ImageDraw.Draw(image)


def rectangle(box, fill, radius=0, outline=None, width=1):
    draw.rounded_rectangle(
        [v * SCALE for v in box], radius=radius * SCALE, fill=fill,
        outline=outline, width=width * SCALE,
    )


black, white, red = "#000000", "#E8E8E5", "#F23348"
rectangle((8, 27, 180, 123), black, 13)
rectangle((17, 36, 171, 114), white, 5)
rectangle((24, 49, 164, 101), "#802C3C", 2)
rectangle((31, 56, 73, 98), black, 3)
rectangle((45, 56, 59, 98), "#CBCBC8")
rectangle((31, 70, 73, 84), "#CBCBC8")
for x in (116, 144):
    draw.ellipse((x*SCALE, 68*SCALE, (x+19)*SCALE, 87*SCALE), fill=black)
    draw.ellipse(((x+4)*SCALE, 72*SCALE, (x+15)*SCALE, 83*SCALE), fill=red)
rectangle((82, 83, 94, 89), black, 3)
rectangle((97, 83, 109, 89), black, 3)
image = image.resize((188, 150), Image.Resampling.LANCZOS)
preview = ROOT / "docs/design/game-icon.png"
preview.parent.mkdir(parents=True, exist_ok=True)
image.save(preview)
colors = bytearray()
alpha = bytearray()
for r, g, b, a in image.getdata():
    colors.extend(struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))
    alpha.append(a)
header = struct.pack("<IHHHH", 0x1419, 188, 150, 188 * 2, 0)
(ROOT / "firmware/main/assets/assets_bin/icon_game.bin").write_bytes(header + colors + alpha)
print(preview)
