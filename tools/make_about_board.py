#!/usr/bin/env python3
"""Build the Classic About ULA band from a SCREEN$ plus credit lines."""

from __future__ import annotations

import argparse
from pathlib import Path

if __package__:
    from .asm_data import parse_defb_block
else:
    from asm_data import parse_defb_block

try:
    from PIL import Image
except ModuleNotFoundError:
    Image = None


SCREEN_W_BYTES = 32
SCREEN_W = SCREEN_W_BYTES * 8
SCREEN_H = 192
# One attribute row above the previous crop so the scene sits tighter
# under the live banner. Turban tips in source y=16-23 are clipped.
SOURCE_TOP = 24
BAND_TOP = 32
BAND_H = 144
ATTR_ROWS = BAND_H // 8
ABOUT_BYTES = (SCREEN_W_BYTES * BAND_H) + (SCREEN_W_BYTES * ATTR_ROWS)
ATTR_WHITE = 0x07
ATTR_YELLOW = 0x06
# Inclusive columns of the black gap between the two players. 13 cells = 104px.
TEXT_COL0 = 10
TEXT_COL1 = 22
TEXT_X0 = TEXT_COL0 * 8
TEXT_W = (TEXT_COL1 - TEXT_COL0 + 1) * 8
# (text, attr, band_y, scale_x, scale_y)
TEXT_LINES = [
    ("SHATRANJ", ATTR_WHITE, 16, 2, 2),
    ("A CHESS GAME FOR ZX", ATTR_WHITE, 32, 1, 1),
    ("(C) 2026 M.I. MONGE", ATTR_WHITE, 40, 1, 1),
    ("GH: IGNACIOMONGE/SHATRANJ", ATTR_YELLOW, 48, 1, 1),
    ("LICENSE: GNU GPL V2.0", ATTR_YELLOW, 56, 1, 1),
]


def scr_bitmap_offset(y: int, col: int) -> int:
    third = y // 64
    y8 = y % 8
    row = (y // 8) % 8
    return (third * 2048) + (y8 * 256) + (row * 32) + col


def extract_band(scr: bytes) -> tuple[bytearray, bytearray]:
    if len(scr) != 6912:
        raise SystemExit(f"SCREEN$ must be 6912 bytes, got {len(scr)}")
    pixels = bytearray()
    for y in range(SOURCE_TOP, SOURCE_TOP + BAND_H):
        for col in range(SCREEN_W_BYTES):
            pixels.append(scr[scr_bitmap_offset(y, col)])
    attr_top = SOURCE_TOP // 8
    attrs = bytearray(
        scr[6144 + (attr_top * SCREEN_W_BYTES) : 6144 + ((attr_top + ATTR_ROWS) * SCREEN_W_BYTES)]
    )
    return pixels, attrs


def line_width(text: str, scale_x: int) -> int:
    return len(text) * 4 * scale_x


def text_origin_x(text: str, scale_x: int) -> int:
    width = line_width(text, scale_x)
    if width > TEXT_W:
        raise SystemExit(f"credit line '{text}' is {width}px, gap is {TEXT_W}px")
    return TEXT_X0 + (TEXT_W - width) // 2


def clear_text_cells(pixels: bytearray, y: int, x: int, width: int) -> None:
    row = y // 8
    col0 = x // 8
    col1 = (x + width - 1) // 8
    for col in range(col0, col1 + 1):
        if col < TEXT_COL0 or col > TEXT_COL1:
            raise SystemExit(f"credit glyphs left the player gap at col {col}")
        for scan in range(8):
            pixels[(row * 8 + scan) * SCREEN_W_BYTES + col] = 0


def put_ikkle_text(
    pixels: bytearray,
    font: bytes,
    y: int,
    x: int,
    text: str,
    scale_x: int = 1,
    scale_y: int = 1,
) -> None:
    for ch in text.upper():
        code = ord(ch)
        if 33 <= code < 128:
            if code >= 96:
                code -= 32
            index = (code - 32) * 2
            if index + 1 < len(font):
                rows = (
                    font[index] >> 4,
                    font[index] & 0x0F,
                    font[index + 1] >> 4,
                    font[index + 1] & 0x0F,
                )
                for row, bits in enumerate(rows):
                    for yoff in range(scale_y):
                        py = y + row * scale_y + yoff
                        if py < 0 or py >= BAND_H:
                            continue
                        for bit in range(4):
                            if bits & (0x08 >> bit):
                                for xoff in range(scale_x):
                                    px = x + bit * scale_x + xoff
                                    if 0 <= px < SCREEN_W:
                                        offset = py * SCREEN_W_BYTES + (px // 8)
                                        pixels[offset] |= 0x80 >> (px & 7)
        x += 4 * scale_x


def apply_text_attrs(attrs: bytearray, y: int, x: int, width: int, color: int) -> None:
    row = y // 8
    if row < 0 or row >= ATTR_ROWS:
        raise SystemExit(f"credit row {row} is outside the About band")
    col0 = x // 8
    col1 = (x + width - 1) // 8
    start = row * SCREEN_W_BYTES
    for col in range(col0, col1 + 1):
        if col < TEXT_COL0 or col > TEXT_COL1:
            raise SystemExit(f"credit attribute left the player gap at col {col}")
        attrs[start + col] = color


def build_board(source: Path, ui_assets: Path) -> bytes:
    pixels, attrs = extract_band(source.read_bytes())
    font = parse_defb_block(
        ui_assets.read_text(encoding="ascii"),
        "timer_ikkle_packed",
        "font_packed",
    )
    for line, color, y, scale_x, scale_y in TEXT_LINES:
        x = text_origin_x(line, scale_x)
        width = line_width(line, scale_x)
        clear_text_cells(pixels, y, x, width)
        put_ikkle_text(pixels, font, y, x, line, scale_x, scale_y)
        apply_text_attrs(attrs, y, x, width, color)
    out = bytes(pixels + attrs)
    if len(out) != ABOUT_BYTES:
        raise SystemExit(f"bad board payload size: {len(out)}")
    return out


def band_preview(raw: bytes) -> Image.Image:
    pixels = raw[: SCREEN_W_BYTES * BAND_H]
    attrs = raw[SCREEN_W_BYTES * BAND_H :]
    colors = [
        (0, 0, 0),
        (0, 0, 192),
        (192, 0, 0),
        (192, 0, 192),
        (0, 192, 0),
        (0, 192, 192),
        (192, 192, 0),
        (192, 192, 192),
        (0, 0, 0),
        (0, 0, 255),
        (255, 0, 0),
        (255, 0, 255),
        (0, 255, 0),
        (0, 255, 255),
        (255, 255, 0),
        (255, 255, 255),
    ]
    img = Image.new("RGB", (SCREEN_W, BAND_H))
    pix = img.load()
    for y in range(BAND_H):
        attr_row = y // 8
        for col in range(SCREEN_W_BYTES):
            bits = pixels[y * SCREEN_W_BYTES + col]
            attr = attrs[attr_row * SCREEN_W_BYTES + col]
            ink = colors[(attr & 7) + (8 if attr & 0x40 else 0)]
            paper = colors[((attr >> 3) & 7) + (8 if attr & 0x40 else 0)]
            for bit in range(8):
                pix[col * 8 + bit, y] = ink if bits & (0x80 >> bit) else paper
    return img


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("ui_assets", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()

    payload = build_board(args.source, args.ui_assets)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    print(f"[OK] {args.output}: {ABOUT_BYTES} bytes")
    if args.preview:
        if Image is None:
            raise SystemExit("--preview requires Pillow")
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        band = band_preview(payload)
        preview = band.resize((SCREEN_W * 2, BAND_H * 2), Image.Resampling.NEAREST)
        preview.save(args.preview)
        print(f"[OK] {args.preview}")
        screen = Image.new("RGB", (SCREEN_W, SCREEN_H), (0, 0, 0))
        screen.paste(band, (0, BAND_TOP))
        screen_path = args.preview.with_name(args.preview.stem + "_screen.png")
        screen.resize((SCREEN_W * 2, SCREEN_H * 2), Image.Resampling.NEAREST).save(
            screen_path
        )
        print(f"[OK] {screen_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
