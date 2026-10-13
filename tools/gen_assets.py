#!/usr/bin/env python3
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

if __package__:
    from .asm_data import label_block as block_from_label
    from .asm_data import parse_defb_with_offsets as parse_defb
    from .next_bundle_codec import CodecError, decompress_classic_zx0
else:
    from asm_data import label_block as block_from_label
    from asm_data import parse_defb_with_offsets as parse_defb
    from next_bundle_codec import CodecError, decompress_classic_zx0

EXPECTED_UI_BYTES = 889
BANNER_INFO_SLOT = 34
VERSION_SLOT = 16
SPECTRANEXT_BANNER_INFO = "ONLINE CHESS FOR SPECTRANEXT"
# Next board sprite themes: black&white, blue3, green, brown, wood.
# ULA attrs only tint the board frame; squares are covered by sprites.
NEXT_UI_THEME = Path(__file__).resolve().parents[1] / "assets/next/ui_theme.asm"
RUNTIME_PIECE_BYTES = 384
EXPECTED_PIECE_BYTES = RUNTIME_PIECE_BYTES * 3
ABOUT_BOARD_WIDTH = 32
ABOUT_PIXEL_ROWS = 152
ABOUT_ATTR_ROWS = 19
ABOUT_PIXEL_BYTES = ABOUT_BOARD_WIDTH * ABOUT_PIXEL_ROWS
EXPECTED_ABOUT_BOARD_BYTES = ABOUT_PIXEL_BYTES + (ABOUT_BOARD_WIDTH * ABOUT_ATTR_ROWS)
ABOUT_CHUNK_BYTES = (448,) * 10 + (384, 448, 160)
ABOUT_DIRECTORY_BYTES = 2 * len(ABOUT_CHUNK_BYTES)
ABOUT_ZX0_DELTA_LIMIT = 3
BASE_RUNTIME_BYTES = EXPECTED_UI_BYTES + RUNTIME_PIECE_BYTES
ZX_EXTRA_PIECE_OFFSET = BASE_RUNTIME_BYTES
ABOUT_PAYLOAD_OFFSET = ZX_EXTRA_PIECE_OFFSET + (
    EXPECTED_PIECE_BYTES - RUNTIME_PIECE_BYTES
)
SPECTRANEXT_ABOUT_PAYLOAD_OFFSET = BASE_RUNTIME_BYTES
SPECTRANEXT_EXTRA_PIECE_OFFSET = (
    SPECTRANEXT_ABOUT_PAYLOAD_OFFSET + EXPECTED_ABOUT_BOARD_BYTES
)
NEXT_EXTRA_PIECE_OFFSET = BASE_RUNTIME_BYTES
EXPECTED_NEXT_DAT_BYTES = EXPECTED_UI_BYTES + EXPECTED_PIECE_BYTES
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
    "black_turn_msg": 639,
    "white_check_msg": 653,
    "black_check_msg": 665,
    "menu_cursor_masks": 677,
    "tab_label_file": 701,
    "tab_label_link": 706,
    "tab_label_game": 712,
    "tab_label_flip": 718,
    "tab_label_theme": 723,
    "tab_label_about": 729,
    "moves_white_msg": 735,
    "moves_black_msg": 741,
    "banner_info_top_msg": 747,
    "board_theme_mark_inks": 781,
    "board_theme_light_attrs": 786,
    "board_theme_dark_attrs": 791,
    "version_banner_msg": 796,
    "taboption_labels": 812,
    "taboption_transitions": 867,
    "taboption_actions": 878,
    "piece_sprites_16x16": EXPECTED_UI_BYTES,
}


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


def parse_loader_about_size(text):
    match = re.search(r"(?m)^about_board_size\s+EQU\s+([0-9]+)\s*$", text)
    if not match:
        raise SystemExit("about_board_size not found in overlay loader")
    return int(match.group(1), 10)


def _compress_about_chunk(raw, executable, work_dir, index):
    source = work_dir / f"about_{index:02d}.bin"
    packed = work_dir / f"about_{index:02d}.zx0"
    source.write_bytes(raw)
    result = subprocess.run(
        [executable, "-f", str(source), str(packed)],
        capture_output=True,
        text=True,
        check=False,
    )
    detail = (result.stdout + result.stderr).strip()
    if result.returncode != 0:
        raise ValueError(f"ZX0 compressor failed for About chunk {index}: {detail}")
    match = re.search(r"\(delta ([0-9]+)\)", detail)
    if match is None:
        raise ValueError(f"ZX0 compressor did not report delta for About chunk {index}")
    stream = packed.read_bytes()
    try:
        restored, consumed = decompress_classic_zx0(stream)
    except CodecError as exc:
        raise ValueError(f"invalid ZX0 About chunk {index}: {exc}") from exc
    if restored != raw or consumed != len(stream):
        raise ValueError(f"ZX0 round-trip mismatch for About chunk {index}")
    return stream, int(match.group(1), 10)


def unpack_about_payload(payload):
    if len(payload) < ABOUT_DIRECTORY_BYTES:
        raise ValueError("truncated About ZX0 directory")
    offset = ABOUT_DIRECTORY_BYTES
    raw = bytearray()
    for index, expected_size in enumerate(ABOUT_CHUNK_BYTES):
        directory = 2 * index
        packed_size = int.from_bytes(payload[directory : directory + 2], "little")
        end = offset + packed_size
        if packed_size == 0 or end > len(payload):
            raise ValueError(f"invalid packed size for About chunk {index}")
        try:
            restored, consumed = decompress_classic_zx0(payload, offset, end)
        except CodecError as exc:
            raise ValueError(f"invalid ZX0 About chunk {index}: {exc}") from exc
        if consumed != end or len(restored) != expected_size:
            raise ValueError(f"bad decoded size for About chunk {index}")
        raw.extend(restored)
        offset = end
    if offset != len(payload):
        raise ValueError("trailing bytes after About ZX0 streams")
    return bytes(raw)


def build_about_payload(raw, zx0_executable="z88dk-zx0"):
    if len(raw) != EXPECTED_ABOUT_BOARD_BYTES:
        raise ValueError(
            f"About asset has {len(raw)} bytes, expected {EXPECTED_ABOUT_BOARD_BYTES}"
        )
    executable = shutil.which(zx0_executable)
    if executable is None:
        raise ValueError(f"ZX0 compressor not found: {zx0_executable}")
    streams = []
    deltas = []
    offset = 0
    with tempfile.TemporaryDirectory(prefix="shatranj-about-zx0-") as temp:
        work_dir = Path(temp)
        for index, size in enumerate(ABOUT_CHUNK_BYTES):
            stream, delta = _compress_about_chunk(
                raw[offset : offset + size], executable, work_dir, index
            )
            if len(stream) > size:
                raise ValueError(f"ZX0 About chunk {index} does not fit in-place")
            streams.append(stream)
            deltas.append(delta)
            offset += size
    if max(deltas) > ABOUT_ZX0_DELTA_LIMIT:
        raise ValueError(
            f"About ZX0 needs delta {max(deltas)}, limit is {ABOUT_ZX0_DELTA_LIMIT}"
        )
    directory = b"".join(len(stream).to_bytes(2, "little") for stream in streams)
    payload = directory + b"".join(streams)
    if unpack_about_payload(payload) != raw:
        raise ValueError("About ZX0 payload round-trip mismatch")
    return payload


def validate_offsets(label, got, expected):
    for name, offset in expected.items():
        if got.get(name) != offset:
            raise SystemExit(
                f"{label}: {name} offset got {got.get(name)}, expected {offset}"
            )


def patch_slot(ui, offset, slot, text, label):
    data = text.encode("ascii") + b"\0"
    if len(data) > slot:
        raise SystemExit(f"{label}: '{text}' needs {len(data)} bytes, slot is {slot}")
    ui[offset : offset + slot] = data.ljust(slot, b"\0")


def main(argv):
    version = None
    zx0_executable = "z88dk-zx0"
    is_next = False
    is_spectranext = False
    args = [argv[0]]
    it = iter(argv[1:])
    for arg in it:
        if arg == "--version":
            version = next(it, None)
            if version is None:
                raise SystemExit("--version requires a value")
        elif arg == "--zx0":
            zx0_executable = next(it, None)
            if zx0_executable is None:
                raise SystemExit("--zx0 requires a value")
        elif arg == "--next":
            is_next = True
        elif arg == "--spectranext":
            is_spectranext = True
        else:
            args.append(arg)
    argv = args
    if len(argv) != 7 or version is None:
        raise SystemExit(
            "usage: gen_assets.py <ui_assets.asm> <pieces.asm> "
            "<screen.asm> <overlay_loader.asm> <about_board.bin> <out.dat> "
            "--version <version> [--zx0 <executable>] [--next|--spectranext]"
        )
    if is_next and is_spectranext:
        raise SystemExit("--next and --spectranext are mutually exclusive")
    if (
        re.fullmatch(r"[0-9]+\.[0-9]+(?:\.[0-9]+)?(?:-dev(?:[0-9]{3}|ESP))?", version)
        is None
    ):
        raise SystemExit(
            "version must contain major.minor[-devNNN|-devESP] or "
            "major.minor.patch[-devNNN|-devESP]"
        )

    ui_path = Path(argv[1])
    pieces_path = Path(argv[2])
    screen_path = Path(argv[3])
    loader_path = Path(argv[4])
    about_path = Path(argv[5])
    out_path = Path(argv[6])

    ui, ui_offsets = parse_defb(ui_path.read_text(encoding="ascii"))
    ui = bytearray(ui)
    screen_offsets = parse_screen_asset_equ(screen_path.read_text(encoding="ascii"))
    expected_offsets = screen_offsets if is_next else EXPECTED_UI_OFFSETS
    expected_ui_bytes = expected_offsets["piece_sprites_16x16"]
    if len(ui) != expected_ui_bytes:
        raise SystemExit(
            f"{ui_path}: got {len(ui)} UI bytes, expected {expected_ui_bytes}"
        )
    validate_offsets(
        str(ui_path),
        ui_offsets,
        {k: v for k, v in expected_offsets.items() if k != "piece_sprites_16x16"},
    )

    validate_offsets(str(screen_path), screen_offsets, expected_offsets)

    if is_spectranext:
        patch_slot(
            ui,
            EXPECTED_UI_OFFSETS["banner_info_top_msg"],
            BANNER_INFO_SLOT,
            SPECTRANEXT_BANNER_INFO,
            "banner_info_top_msg",
        )
    if is_next:
        theme, offsets = parse_defb(NEXT_UI_THEME.read_text(encoding="ascii"))
        names = ("banner_info_top_msg", "board_theme_mark_inks",
                 "board_theme_light_attrs", "board_theme_dark_attrs", "ui_theme_end")
        for name, end in zip(names, names[1:]):
            values = theme[offsets[name] : offsets[end]]
            if name == "banner_info_top_msg":
                patch_slot(ui, expected_offsets[name], BANNER_INFO_SLOT,
                           values.rstrip(b"\0").decode("ascii"), name)
                continue
            if len(values) != 5:
                raise SystemExit(f"{NEXT_UI_THEME}: {name} must contain 5 bytes")
            off = expected_offsets[name]
            ui[off : off + len(values)] = values
    banner_version = version if "-dev" in version else f"version {version}"
    patch_slot(
        ui,
        expected_offsets["version_banner_msg"],
        VERSION_SLOT,
        banner_version,
        "version_banner_msg",
    )

    pieces_text = pieces_path.read_text(encoding="ascii")
    pieces, _ = parse_defb(
        block_from_label(pieces_text, "netchesszx_piece_sprites_16x16")
    )
    if len(pieces) != EXPECTED_PIECE_BYTES:
        raise SystemExit(
            f"{pieces_path}: got {len(pieces)} piece bytes, expected {EXPECTED_PIECE_BYTES}"
        )

    runtime_pieces = pieces[:RUNTIME_PIECE_BYTES]
    extra_pieces = pieces[RUNTIME_PIECE_BYTES:]
    loader_text = loader_path.read_text(encoding="ascii")
    loader_size = parse_loader_asset_size(loader_text)
    runtime_total = len(ui) + len(runtime_pieces)
    if loader_size != runtime_total:
        raise SystemExit(
            f"{loader_path}: asset_load_size got {loader_size}, "
            f"expected runtime load {runtime_total}"
        )
    loader_about_size = parse_loader_about_size(loader_text)
    if loader_about_size == EXPECTED_ABOUT_BOARD_BYTES:
        about_raw = about_path.read_bytes()
        try:
            about = (
                about_raw
                if is_spectranext
                else build_about_payload(about_raw, zx0_executable)
            )
        except ValueError as exc:
            raise SystemExit(f"{about_path}: {exc}") from exc
    elif loader_about_size == 0:
        about_raw = b""
        about = b""
    else:
        raise SystemExit(
            f"{loader_path}: about_board_size got {loader_about_size}, "
            f"expected {EXPECTED_ABOUT_BOARD_BYTES} or 0"
        )

    if is_spectranext:
        output = ui + runtime_pieces + about + extra_pieces
    else:
        output = ui + runtime_pieces + extra_pieces + about
    total = len(output)
    expected_total = expected_ui_bytes + EXPECTED_PIECE_BYTES + len(about)
    if total != expected_total:
        raise AssertionError(f"bad DAT size {total}, expected {expected_total}")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(output)
    print(f"[OK] {out_path}: {total} bytes (About {len(about_raw)} -> {len(about)})")


if __name__ == "__main__":
    main(sys.argv)
