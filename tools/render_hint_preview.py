#!/usr/bin/env python3
import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "release"

SCREEN_W = 256
SCREEN_H = 192
BOARD_X = 8
BOARD_Y = 40
BOARD_ATTR_COL = BOARD_X // 8
BOARD_ATTR_ROW = BOARD_Y // 8
BOARD_THEMES = (
    (0x38, 0x07),
    (0x31, 0x0E),
    (0x3A, 0x17),
    (0x29, 0x0D),
    (0x14, 0x22),
)
HINT_INKS = (0x03, 0x02, 0x01, 0x06, 0x07)
ATTR_SELECTED_LIGHT = 0x7D
ATTR_SELECTED_DARK = 0x45

ZX_COLORS = {
    0: (0, 0, 0),
    1: (0, 0, 205),
    2: (205, 0, 0),
    3: (205, 0, 205),
    4: (0, 205, 0),
    5: (0, 205, 205),
    6: (205, 205, 0),
    7: (205, 205, 205),
    8: (0, 0, 0),
    9: (0, 0, 255),
    10: (255, 0, 0),
    11: (255, 0, 255),
    12: (0, 255, 0),
    13: (0, 255, 255),
    14: (255, 255, 0),
    15: (255, 255, 255),
}

INITIAL_BOARD = "rnbqkbnrpppppppp................................PPPPPPPPRNBQKBNR"
PREVIEW_BOARD = "r...k..rppp..ppp...b.......p.n......P.....N..N..PPPP.PPPRNBQK..R"
PIECE_ORDER = "KQRBNP"


def parse_defb_from_label(path, label):
    text = path.read_text(encoding="ascii")
    start = re.search(rf"(?m)^{re.escape(label)}:\s*$", text)
    if not start:
        raise SystemExit(f"label not found: {label}")
    data = bytearray()
    for raw in text[start.end() :].splitlines():
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue
        if re.match(r"^[A-Za-z_][A-Za-z0-9_]*:", line):
            continue
        if not line.upper().startswith("DEFB"):
            continue
        for token in line[4:].split(","):
            token = token.strip()
            if token:
                data.append(int(token, 0) & 0xFF)
    return bytes(data)


def set_attr(attrs, row, col, attr):
    attr_row = BOARD_ATTR_ROW + row * 2
    attr_col = BOARD_ATTR_COL + col * 2
    attrs[attr_row * 32 + attr_col] = attr
    attrs[attr_row * 32 + attr_col + 1] = attr
    attrs[(attr_row + 1) * 32 + attr_col] = attr
    attrs[(attr_row + 1) * 32 + attr_col + 1] = attr


def hint_attr_for(attr, hint_ink):
    return (attr & 0x78) | hint_ink


def set_hint_attr(attrs, row, col, base_attr, hint_ink):
    set_attr(attrs, row, col, hint_attr_for(base_attr, hint_ink))


def screen_offset(x, y):
    return ((y & 0xC0) << 5) | ((y & 0x07) << 8) | ((y & 0x38) << 2) | (x >> 3)


def put_byte(pixels, x_byte, y, value):
    pixels[screen_offset(x_byte * 8, y)] = value & 0xFF


def or_byte(pixels, x_byte, y, value):
    idx = screen_offset(x_byte * 8, y)
    pixels[idx] |= value & 0xFF


def xor_byte(pixels, x_byte, y, value):
    idx = screen_offset(x_byte * 8, y)
    pixels[idx] ^= value & 0xFF


def draw_sprite(pixels, row, col, piece, sprites):
    upper = piece.upper()
    piece_index = PIECE_ORDER.index(upper)
    variant = 1 if piece.islower() else 0
    if (row + col) & 1:
        variant ^= 1
    base = piece_index * 64 + variant * 32
    x_byte = (BOARD_X + col * 16) // 8
    y0 = BOARD_Y + row * 16
    for scan in range(16):
        put_byte(pixels, x_byte, y0 + scan, sprites[base + scan * 2])
        put_byte(pixels, x_byte + 1, y0 + scan, sprites[base + scan * 2 + 1])


def draw_hint(pixels, attrs, row, col, base_attr, hint_ink):
    x_byte = (BOARD_X + col * 16) // 8
    y0 = BOARD_Y + row * 16
    set_hint_attr(attrs, row, col, base_attr, hint_ink)
    or_byte(pixels, x_byte, y0 + 6, 0x01)
    or_byte(pixels, x_byte + 1, y0 + 6, 0x80)
    for scan in range(7, 9):
        or_byte(pixels, x_byte, y0 + scan, 0x03)
        or_byte(pixels, x_byte + 1, y0 + scan, 0xC0)
    or_byte(pixels, x_byte, y0 + 9, 0x01)
    or_byte(pixels, x_byte + 1, y0 + 9, 0x80)


def draw_capture_hint(attrs, row, col, base_attr, hint_ink):
    set_hint_attr(attrs, row, col, base_attr, hint_ink)


def draw_selected(pixels, attrs, row, col):
    x_byte = (BOARD_X + col * 16) // 8
    y0 = BOARD_Y + row * 16
    set_attr(
        attrs,
        row,
        col,
        ATTR_SELECTED_DARK if ((row + col) & 1) else ATTR_SELECTED_LIGHT,
    )
    put_byte(pixels, x_byte, y0, 0xFF)
    put_byte(pixels, x_byte + 1, y0, 0xFF)
    put_byte(pixels, x_byte, y0 + 15, 0xFF)
    put_byte(pixels, x_byte + 1, y0 + 15, 0xFF)
    for scan in range(16):
        or_byte(pixels, x_byte, y0 + scan, 0x80)
        or_byte(pixels, x_byte + 1, y0 + scan, 0x01)
    put_byte(pixels, x_byte, y0 + 1, 0xFF)
    put_byte(pixels, x_byte + 1, y0 + 1, 0xFF)
    put_byte(pixels, x_byte, y0 + 14, 0xFF)
    put_byte(pixels, x_byte + 1, y0 + 14, 0xFF)
    for scan in range(1, 15):
        or_byte(pixels, x_byte, y0 + scan, 0x40)
        or_byte(pixels, x_byte + 1, y0 + scan, 0x02)


def board_attr(row, col, light_attr, dark_attr):
    return dark_attr if ((row + col) & 1) else light_attr


def render_theme(sprites, light_attr, dark_attr, hint_ink):
    pixels = bytearray(6144)
    attrs = bytearray([0x07] * 768)

    for row in range(8):
        for col in range(8):
            base_attr = board_attr(row, col, light_attr, dark_attr)
            set_attr(attrs, row, col, base_attr)
            piece = PREVIEW_BOARD[row * 8 + col]
            if piece != ".":
                draw_sprite(pixels, row, col, piece, sprites)

    draw_hint(
        pixels, attrs, 3, 4, board_attr(3, 4, light_attr, dark_attr), hint_ink
    )  # e5
    draw_capture_hint(
        attrs, 3, 3, board_attr(3, 3, light_attr, dark_attr), hint_ink
    )  # d5
    draw_capture_hint(
        attrs, 3, 5, board_attr(3, 5, light_attr, dark_attr), hint_ink
    )  # f5
    draw_selected(pixels, attrs, 4, 4)  # e4
    return pixels, attrs


def tap_block(flag, payload):
    data = bytes([flag]) + payload
    checksum = 0
    for b in data:
        checksum ^= b
    block = data + bytes([checksum])
    return struct.pack("<H", len(block)) + block


def basic_line(number, body):
    content = bytes(body) + b"\r"
    return struct.pack(">H", number) + struct.pack("<H", len(content)) + content


def load_screen_line(number):
    return basic_line(number, [0xEF, ord('"'), ord('"'), ord(" "), 0xAA])


def pause_zero_line(number):
    return basic_line(number, [0xF2, ord(" "), ord("0"), 0x0E, 0, 0, 0, 0, 0])


def make_loader_program(screen_count):
    loader = bytearray()
    line = 10
    for index in range(screen_count):
        loader.extend(load_screen_line(line))
        line += 10
        if index + 1 < screen_count:
            loader.extend(pause_zero_line(line))
            line += 10
    return bytes(loader)


def write_tap(path, screens):
    loader = make_loader_program(len(screens))
    program_header = (
        bytes([0]) + b"HINTCOL   " + struct.pack("<HHH", len(loader), 10, len(loader))
    )
    data = bytearray(tap_block(0x00, program_header) + tap_block(0xFF, loader))
    for index, screen in enumerate(screens, 1):
        screen_name = f"HINTC{index}".ljust(10).encode("ascii")
        screen_header = (
            bytes([3]) + screen_name + struct.pack("<HHH", len(screen), 0x4000, 0x8000)
        )
        data.extend(tap_block(0x00, screen_header))
        data.extend(tap_block(0xFF, screen))
    path.write_bytes(data)


def rgb_row(pixels, attrs, y):
    raw = bytearray()
    for x in range(SCREEN_W):
        attr = attrs[(y // 8) * 32 + (x // 8)]
        bright = 8 if (attr & 0x40) else 0
        ink = attr & 7
        paper = (attr >> 3) & 7
        bit = pixels[screen_offset(x, y)] & (0x80 >> (x & 7))
        raw.extend(ZX_COLORS[(ink if bit else paper) + bright])
    return bytes(raw)


def write_png_rows(path, width, height, rows):
    data = b"".join(rows)

    def chunk(kind, payload):
        return (
            struct.pack(">I", len(payload))
            + kind
            + payload
            + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
        )

    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(data, 9))
        + chunk(b"IEND", b"")
    )
    path.write_bytes(png)


def write_png(path, pixels, attrs):
    rows = []
    for y in range(SCREEN_H):
        rows.append(b"\x00" + rgb_row(pixels, attrs, y))
    write_png_rows(path, SCREEN_W, SCREEN_H, rows)


def write_sheet_png(path, rendered):
    rows = []
    for y in range(SCREEN_H):
        rows.append(
            b"\x00" + b"".join(rgb_row(pixels, attrs, y) for pixels, attrs in rendered)
        )
    write_png_rows(path, SCREEN_W * len(rendered), SCREEN_H, rows)


def main():
    sprites = parse_defb_from_label(
        ROOT / "assets/spectrum/chess_pieces_16x16.asm",
        "netchesszx_piece_sprites_16x16",
    )
    if len(sprites) < 384:
        raise SystemExit("piece sprite block too small")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    rendered = [
        render_theme(sprites, light, dark, hint_ink)
        for (light, dark), hint_ink in zip(BOARD_THEMES, HINT_INKS)
    ]
    screens = [bytes(pixels + attrs) for pixels, attrs in rendered]

    for index, (screen, (pixels, attrs)) in enumerate(zip(screens, rendered), 1):
        stem = f"SHATRANJ-hints-theme-color-preview-{index}"
        (OUT_DIR / f"{stem}.scr").write_bytes(screen)
        write_png(OUT_DIR / f"{stem}.png", pixels, attrs)

    write_tap(OUT_DIR / "SHATRANJ-hints-theme-color-preview.tap", screens)
    write_sheet_png(OUT_DIR / "SHATRANJ-hints-theme-color-preview-sheet.png", rendered)
    print("[OK] release/SHATRANJ-hints-theme-color-preview.tap")
    print("[OK] release/SHATRANJ-hints-theme-color-preview-sheet.png")


if __name__ == "__main__":
    main()
