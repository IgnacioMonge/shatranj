#!/usr/bin/env python3
import re
import sys
from pathlib import Path

EXPECTED_UI_BYTES = 639
RUNTIME_PIECE_BYTES = 384
EXPECTED_PIECE_BYTES = RUNTIME_PIECE_BYTES * 3
EXPECTED_ABOUT_BOARD_BYTES = 2916
EXPECTED_UI_OFFSETS = {
    "expand_2x": 0,
    "font_lut": 16,
    "chat_icon_white": 26,
    "chat_icon_black": 32,
    "timer_ikkle_packed": 38,
    "font_packed": 166,
    "badge_pattern": 454,
    "conn_pattern": 462,
    "rank_digit_patterns": 470,
    "file_letter_patterns": 510,
    "psfc_piece_chars": 550,
    "title_msg": 556,
    "chat_msg": 567,
    "session_setup_msg": 572,
    "game_setup_msg": 589,
    "preflight_setup_msg": 600,
    "input_prompt_msg": 622,
    "white_turn_msg": 625,
    "piece_sprites_16x16": EXPECTED_UI_BYTES,
}


def parse_defb(text):
    data = bytearray()
    offsets = {}
    for raw in text.splitlines():
        line = raw.split(";", 1)[0].strip()
        label = re.match(r"^([A-Za-z_][A-Za-z0-9_]*):$", line)
        if label:
            offsets[label.group(1)] = len(data)
            continue
        if not line.upper().startswith("DEFB"):
            continue
        body = line[4:].strip()
        for token in body.split(","):
            token = token.strip()
            if not token:
                continue
            if token.startswith('"') and token.endswith('"'):
                data.extend(token[1:-1].encode("ascii"))
            else:
                data.append(int(token, 0) & 0xFF)
    return bytes(data), offsets


def parse_screen_asset_equ(text):
    offsets = {}
    for raw in text.splitlines():
        line = raw.split(";", 1)[0].strip()
        match = re.match(
            r"^([A-Za-z_][A-Za-z0-9_]*)\s+EQU\s+NETCHESSZX_ASSET_BASE(?:\s+\+\s+([0-9]+))?$",
            line,
        )
        if match:
            offsets[match.group(1)] = int(match.group(2) or "0", 10)
    return offsets


def parse_loader_asset_size(text):
    match = re.search(r"(?m)^asset_load_size\s+EQU\s+([0-9]+)\s*$", text)
    if not match:
        raise SystemExit("asset_load_size not found in overlay loader")
    return int(match.group(1), 10)


def validate_offsets(label, got, expected):
    for name, offset in expected.items():
        if got.get(name) != offset:
            raise SystemExit(
                f"{label}: {name} offset got {got.get(name)}, expected {offset}"
            )


def block_from_label(text, label, end_label=None):
    start = re.search(rf"(?m)^{re.escape(label)}:\s*$", text)
    if not start:
        raise SystemExit(f"label not found: {label}")
    body = text[start.end() :]
    if end_label is not None:
        end = re.search(rf"(?m)^{re.escape(end_label)}:\s*$", body)
        if not end:
            raise SystemExit(f"end label not found: {end_label}")
        body = body[: end.start()]
    return body


def main(argv):
    if len(argv) != 7:
        raise SystemExit(
            "usage: gen_assets.py <ui_assets.asm> <pieces.asm> "
            "<screen.asm> <overlay_loader.asm> <about_board.bin> <out.dat>"
        )

    ui_path = Path(argv[1])
    pieces_path = Path(argv[2])
    screen_path = Path(argv[3])
    loader_path = Path(argv[4])
    about_path = Path(argv[5])
    out_path = Path(argv[6])

    ui, ui_offsets = parse_defb(ui_path.read_text(encoding="ascii"))
    if len(ui) != EXPECTED_UI_BYTES:
        raise SystemExit(
            f"{ui_path}: got {len(ui)} UI bytes, expected {EXPECTED_UI_BYTES}"
        )
    validate_offsets(
        str(ui_path),
        ui_offsets,
        {k: v for k, v in EXPECTED_UI_OFFSETS.items() if k != "piece_sprites_16x16"},
    )

    screen_offsets = parse_screen_asset_equ(screen_path.read_text(encoding="ascii"))
    validate_offsets(str(screen_path), screen_offsets, EXPECTED_UI_OFFSETS)

    pieces_text = pieces_path.read_text(encoding="ascii")
    pieces, _ = parse_defb(
        block_from_label(pieces_text, "netchesszx_piece_sprites_16x16")
    )
    if len(pieces) != EXPECTED_PIECE_BYTES:
        raise SystemExit(
            f"{pieces_path}: got {len(pieces)} piece bytes, expected {EXPECTED_PIECE_BYTES}"
        )

    about = about_path.read_bytes()
    if len(about) != EXPECTED_ABOUT_BOARD_BYTES:
        raise SystemExit(
            f"{about_path}: got {len(about)} About bytes, "
            f"expected {EXPECTED_ABOUT_BOARD_BYTES}"
        )

    runtime_pieces = pieces[:RUNTIME_PIECE_BYTES]
    extra_pieces = pieces[RUNTIME_PIECE_BYTES:]
    runtime_total = len(ui) + len(runtime_pieces)
    total = runtime_total + len(about) + len(extra_pieces)
    loader_size = parse_loader_asset_size(loader_path.read_text(encoding="ascii"))
    if loader_size != runtime_total:
        raise SystemExit(
            f"{loader_path}: asset_load_size got {loader_size}, "
            f"expected runtime load {runtime_total}"
        )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(ui + runtime_pieces + about + extra_pieces)
    print(f"[OK] {out_path}: {total} bytes")


if __name__ == "__main__":
    main(sys.argv)
