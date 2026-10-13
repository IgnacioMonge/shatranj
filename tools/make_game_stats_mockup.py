#!/usr/bin/env python3
"""Render a deterministic Classic ULA GAME STATS proposal from game assets."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

if __package__:
    from .asm_data import parse_defb_with_offsets
    from .make_about_board import put_ikkle_text
else:
    from asm_data import parse_defb_with_offsets
    from make_about_board import put_ikkle_text


ROOT = Path(__file__).resolve().parents[1]
THEMES = ("mono", "blue", "red", "cyan", "bright")
SETS = ("std", "spcy", "pixl")
PAGE = (8, 40, 136, 168)  # FILE interior: byte columns 1..16, rows 5..20.
WHITE_NAME, BLACK_NAME = "NACHO", "PEPE"
CAPTURED_BY_WHITE = "QRPP"
CAPTURED_BY_BLACK = "RBNPP"
ALL_CAPTURED = "QRRBBNNPPPPPPPP"
VALUE_X = (32, 84)  # ikkle half-columns 8 / 21
# Pixel row, label, white, black: 751 s vs 545 s over 31/30 moves.
METRICS = ((91, "TIME", "12:31", "09:05"),
           (97, "AVG", "0:24", "0:18"),
           (103, "MAX", "3:10", "1:48"),
           (109, "AT", "23", "17"),
           (115, "MIN", "0:02", "0:01"),
           (121, "AT", "5", "9"))
SHARE = (58, 42)


class Screen:
    def __init__(self, theme: int, piece_set: int):
        self.pixels = bytearray(32 * 192)
        self.attrs = bytearray([0x07] * (32 * 24))
        self.ui, self.offsets = parse_defb_with_offsets(
            (ROOT / "assets/spectrum/ui_runtime_assets.asm").read_text(encoding="ascii")
        )
        self.font = self.ui[self.offsets["timer_ikkle_packed"]:self.offsets["font_packed"]]
        sprites, offsets = parse_defb_with_offsets(
            (ROOT / "assets/spectrum/chess_pieces_16x16.asm").read_text(encoding="ascii")
        )
        start = offsets[f"netchesszx_piece_set_{SETS[piece_set]}"]
        self.sprites = sprites[start:start + 384]
        self.light = self.ui[self.offsets["board_theme_light_attrs"] + theme]

    def attr(self, row: int, col: int, width: int, value: int):
        self.attrs[row * 32 + col:row * 32 + col + width] = bytes([value] * width)

    def bit(self, x: int, y: int):
        assert 0 <= x < 256 and 0 <= y < 192
        self.pixels[y * 32 + x // 8] |= 0x80 >> (x & 7)

    def pattern(self, x: int, y: int, rows: bytes, width: int = 8):
        for scan, bits in enumerate(rows):
            for bit in range(width):
                if bits & (1 << (width - 1 - bit)):
                    self.bit(x + bit, y + scan)

    def small(self, text: str, x: int, y: int, attr: int = 0x07):
        assert x % 4 == 0 and x + len(text) * 4 <= 256
        glyphs = bytearray(32 * 4)
        put_ikkle_text(glyphs, self.font, 0, x, text)
        for scan in range(4):
            for col, bits in enumerate(glyphs[scan * 32:(scan + 1) * 32]):
                self.pixels[(y + scan) * 32 + col] |= bits
        for row in range(y // 8, (y + 3) // 8 + 1):
            self.attr(row, x // 8, (x + len(text) * 4 + 7) // 8 - x // 8, attr)

    def scaled(self, text: str, x: int, row: int):
        # Same path as font_scanline + draw_scaled_glyph: six rows at y+2,
        # font_lut's high nibble expanded horizontally, every row doubled.
        for ch in text:
            for scan in range(6):
                packed = self.ui[self.offsets["font_packed"] + (ord(ch) - 32) * 3 + scan // 2]
                nibble = packed & 15 if scan & 1 else packed >> 4
                glyph = self.ui[self.offsets["font_lut"] + nibble] >> 4
                expanded = self.ui[self.offsets["expand_2x"] + glyph]
                self.pattern(x, row * 8 + 2 + scan * 2, bytes([expanded] * 2))
            x += 8

    def disc(self, x: int, row: int, black: bool):
        # Same six-line shape as the MOVES header, with blank top/bottom.
        rows = bytes([0x3c, *([0x7e if black else 0x42] * 4), 0x3c])
        self.pattern(x, row * 8 + 1, rows)

    def capture(self, piece: str, x: int, row: int, black: bool):
        # Dark paper: outlined black pieces, filled white pieces. These are
        # the game's black/dark and white/dark masks, not light-square masks.
        offset = "KQRBNP".index(piece) * 64 + (0 if black else 32)
        source = self.sprites[offset:offset + 32]
        rows = [int.from_bytes(source[y:y + 2], "big") for y in range(0, 32, 2)]
        # Keep thin outlines in dark pieces; use majority sampling for filled
        # light pieces. Both miniatures are derived from the active 16x16 set.
        threshold = 1 if black else 2
        miniature = []
        for y in range(0, 16, 2):
            bits = 0
            for col in range(8):
                mask = 0xc000 >> (col * 2)
                area = ((rows[y] & mask).bit_count() +
                        (rows[y + 1] & mask).bit_count())
                if area >= threshold:
                    bits |= 0x80 >> col
            miniature.append(bits)
        self.pattern(x, row * 8, miniature)

    def image(self) -> Image.Image:
        palette = [(0, 0, 0), (0, 0, 192), (192, 0, 0), (192, 0, 192),
                   (0, 192, 0), (0, 192, 192), (192, 192, 0), (192, 192, 192)]
        palette += [tuple(255 if channel else 0 for channel in color) for color in palette]
        ink, paper = Image.new("RGB", (32, 24)), Image.new("RGB", (32, 24))
        for cell, attr in enumerate(self.attrs):
            bright = 8 if attr & 0x40 else 0
            ink.putpixel((cell % 32, cell // 32), palette[(attr & 7) + bright])
            paper.putpixel((cell % 32, cell // 32), palette[((attr >> 3) & 7) + bright])
        mask = Image.frombytes("1", (256, 192), bytes(self.pixels))
        return Image.composite(ink.resize(mask.size, Image.Resampling.NEAREST),
                               paper.resize(mask.size, Image.Resampling.NEAREST), mask)


def draw_context(screen: Screen):
    screen.attr(0, 0, 32, 0x47)
    screen.scaled("SHATRANJ", 0, 0)
    screen.small("STATS PREVIEW", 72, 4)
    screen.small("ONLINE CHESS FOR ZX SPECTRUM", 72, 10)
    screen.attr(2, 0, 20, 0x38)
    screen.small("FILE NTWRK OFFER FLIP THEME ABOUT", 4, 18, 0x38)
    screen.small("12:31 / 09:05", 164, 18)
    screen.small("GAME FINISHED", 204, 26)
    # Actual board frame and board_light_line_attr, with coordinates cleared.
    frame_attr = (screen.light & 0x40) | ((screen.light >> 3) & 7)
    for row in (4, 21):
        screen.attr(row, 0, 18, frame_attr)
    for row in range(5, 21):
        screen.attr(row, 0, 1, frame_attr)
        screen.attr(row, 17, 1, frame_attr)
    for x in range(7, 137):
        screen.bit(x, 39)
        screen.bit(x, 168)
    for y in range(40, 168):
        screen.bit(7, y)
        screen.bit(136, y)
    screen.attr(5, 18, 14, 0x38)
    screen.disc(146, 5, False)
    screen.disc(202, 5, True)
    screen.small(WHITE_NAME, 156, 42, 0x38)
    screen.small(BLACK_NAME, 212, 42, 0x38)
    for y in range(40, 48):
        screen.bit(200, y)
    for i, (white, black) in enumerate((("33.NF7+", "KG8"), ("34.RC8+", "KH7"), ("35.RG8", "KXG8"), ("36.H8Q#", "1-0"))):
        screen.small(white, 148, 51 + i * 6)
        screen.small(black, 204, 51 + i * 6)
    screen.small("CHAT", 148, 98, 0x06)
    for x in range(144, 256):
        screen.bit(x, 104)
    for i, (black, time, text) in enumerate(((False, "20:42", "NICE GAME!"), (True, "20:43", "GG!"), (False, "20:43", "THANK YOU"), (True, "20:44", "PLAY AGAIN?"))):
        screen.pattern(144, 110 + i * 6,
                       bytes([0x3c, *([0x7e if black else 0x42] * 3), 0x3c, 0]))
        screen.small(time, 156, 110 + i * 6)
        screen.small(text, 180, 110 + i * 6)
    screen.attr(22, 0, 32, 0x38)
    screen.small("MQTT ROOM ZX42 (HOST) VS ZX", 4, 178, 0x38)
    screen.small("20:44", 220, 178, 0x38)
    start = screen.offsets["conn_pattern"]
    screen.pattern(248, 176, screen.ui[start:start + 8])
    screen.attr(22, 31, 1, 0x3e)
    screen.small("> GG!", 4, 186)


def build_mockup(theme: int = 0, piece_set: int = 0,
                 kings_only: bool = False,
                 names: tuple[str, str] = (WHITE_NAME, BLACK_NAME)) -> Image.Image:
    assert max(len(names[0]), len(names[1])) <= 10
    screen = Screen(theme, piece_set)
    white_captures = ALL_CAPTURED if kings_only else CAPTURED_BY_WHITE
    black_captures = ALL_CAPTURED if kings_only else CAPTURED_BY_BLACK
    draw_context(screen)
    screen.attr(5, 1, 16, 0x47)
    screen.scaled("GAME STATS", 32, 5)
    # Result bar one row below the title (row 8); FILE's rule is cleared.
    screen.attr(8, 1, 16, screen.light)
    screen.small("DRAW AGREED" if kings_only else "CHECKMATE:YOU WON",
                 48 if kings_only else 40, 66, screen.light)
    # Discs start at the value columns (ink from x32 / x84); names follow
    # two half-columns later, so ten characters stay 4 px off the frame.
    screen.attr(10, 1, 16, screen.light)
    screen.disc(VALUE_X[0] - 1, 10, False)
    screen.disc(VALUE_X[1] - 1, 10, True)
    screen.small(names[0], VALUE_X[0] + 8, 82, screen.light)
    screen.small(names[1], VALUE_X[1] + 8, 82, screen.light)
    for y, label, white, black in METRICS:
        screen.small(label, 12, y)
        screen.small(white, VALUE_X[0], y)
        screen.small(black, VALUE_X[1], y)
    # TIME share, right-aligned to end 4 px before the next column / frame.
    for side, share in enumerate(SHARE):
        text = f"{share}%"
        screen.small(text, VALUE_X[side] + (12 - len(text)) * 4, METRICS[0][0])
    values = dict(P=1, N=3, B=3, R=5, Q=9)
    screen.small("CAPTURED", 12, 130, 0x06)
    for side, captured in enumerate((white_captures, black_captures)):
        text = str(sum(values[p] for p in captured))
        screen.small(text, VALUE_X[side] + (12 - len(text)) * 4, 130, 0x06)
    for captured, x, black in ((white_captures, 32, True), (black_captures, 80, False)):
        assert len(captured) <= 15
        # One sprite per capture; 3 rows x 5 pieces covers king against king.
        for i, piece in enumerate(captured):
            screen.capture(piece, x + i % 5 * 8, 17 + i // 5, black)
    screen.small("ANY KEY: BACK", 48, 162, 0x06)
    return screen.image()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--theme", choices=THEMES, default="mono")
    parser.add_argument("--piece-set", choices=SETS, default="std")
    parser.add_argument("--kings-only", action="store_true",
                        help="Show all 15 non-king pieces captured by each side")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/assets/game-stats.png")
    args = parser.parse_args()
    image = build_mockup(THEMES.index(args.theme), SETS.index(args.piece_set),
                         args.kings_only)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output)
    for suffix, preview in (("-4x", image), ("-panel-4x", image.crop(PAGE))):
        path = args.output.with_name(args.output.stem + suffix + ".png")
        preview.resize((preview.width * 4, preview.height * 4), Image.Resampling.NEAREST).save(path)
        print(f"[OK] {path}")
    print(f"[OK] {args.output}: 256x192; FILE interior 128x128")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
