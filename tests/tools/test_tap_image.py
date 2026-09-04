#!/usr/bin/env python3
"""Focused tests for the TAP initialized-data/BSS packaging guard."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.check_tap_image import TapImageError, validate_image  # noqa: E402
from tools.gen_overlay_atlas import (  # noqa: E402
    FINGERPRINT_LEN,
    FINGERPRINT_OFFSET,
    atlas_fingerprint,
    binding_digest,
    build_header,
    write_table,
)


ORG = 0x7000
SYMBOLS = {
    "__data_compiler_tail": ORG + 2,
    "__DATA_END_tail": ORG + 3,
    "__BSS_head": ORG + 3,
    "__BSS_END_tail": ORG + 5,
}


def tap_block(flag: int, payload: bytes) -> bytes:
    body = bytes((flag,)) + payload
    checksum = 0
    for value in body:
        checksum ^= value
    block = body + bytes((checksum,))
    return len(block).to_bytes(2, "little") + block


def code_tap(code: bytes, org: int = ORG) -> bytes:
    header = (
        bytes((3,))
        + b"SHATRANJ  "
        + len(code).to_bytes(2, "little")
        + org.to_bytes(2, "little")
        + bytes(2)
    )
    return tap_block(0, header) + tap_block(0xFF, code)


def expect_rejected(code: bytes, tap: bytes, message: str) -> None:
    try:
        validate_image(SYMBOLS, code, tap, ORG)
    except TapImageError:
        return
    raise AssertionError(message)


def main() -> int:
    code = bytes((0xA1, 0xB2, 0x4B, 0, 0))
    image = validate_image(SYMBOLS, code, code_tap(code), ORG)
    assert len(image.payload) == 5
    assert image.initialized_bytes == 3
    assert image.bss_bytes == 2
    assert image.data_after_compiler == 1

    low_org = ORG - 8
    low_code = bytes((0x11, 0x22, 0x33))
    combined = low_code + bytes(5) + code
    low_image = validate_image(
        SYMBOLS, code, code_tap(combined, low_org), ORG, low_org, low_code
    )
    assert low_image.load_address == low_org
    assert low_image.payload == combined

    split_bss_symbols = dict(SYMBOLS)
    split_bss_symbols.update({
        "__BSS_END_tail": 0x5B4B,
        "__bss_compiler_tail": ORG + 5,
        "__bss_user_tail": 0x5B49,
    })
    split_image = validate_image(
        split_bss_symbols, code, code_tap(code), ORG)
    assert split_image.bss_bytes == 2

    expect_rejected(
        code,
        code_tap(code[:2]),
        "TAP trimmed at __data_compiler_tail was accepted",
    )

    wrong_data = bytearray(code)
    wrong_data[2] ^= 0xFF
    expect_rejected(
        code,
        code_tap(bytes(wrong_data)),
        "corrupt post-compiler data_user byte was accepted",
    )

    dirty_bss = bytearray(code)
    dirty_bss[-1] = 1
    expect_rejected(
        bytes(dirty_bss),
        code_tap(bytes(dirty_bss)),
        "non-zero BSS was accepted",
    )

    entries = [(100, 20), (120, 30)]
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        source = tmp / "source.c"
        source.write_text("first\n", encoding="ascii")
        binding = binding_digest([source], ["NET_BACKEND=spectranext"])
        fingerprint = atlas_fingerprint(entries, binding)
        header = build_header(entries, binding)
        assert int.from_bytes(
            header[FINGERPRINT_OFFSET:FINGERPRINT_OFFSET + FINGERPRINT_LEN],
            "little",
        ) == fingerprint

        table = tmp / "overlay_atlas_table.asm"
        assert write_table(table, entries, binding)
        table_text = table.read_text(encoding="ascii")
        for index, value in enumerate(fingerprint.to_bytes(4, "little")):
            assert f"ovl_atlas_fingerprint_{index} EQU {value}" in table_text

        source.write_text("second\n", encoding="ascii")
        changed_binding = binding_digest(
            [source], ["NET_BACKEND=spectranext"]
        )
        assert atlas_fingerprint(entries, changed_binding) != fingerprint
        assert atlas_fingerprint([(100, 21), (121, 29)], binding) != fingerprint

    print("TAP image guard tests ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
