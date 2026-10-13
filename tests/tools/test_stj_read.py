#!/usr/bin/env python3
"""stj_read.py decodes STJ1, 60-byte legacy snapshots, and headerless TMP."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.stj_read import (  # noqa: E402
    CRC_INIT,
    STJ_HEADER_SIZE,
    UNDO_ONE,
    b64_encode,
    crc16_ccitt,
    crc8,
    format_report,
    parse_bytes,
    record_text,
)

START = (
    "rnbqkbnr"
    "pppppppp"
    "........"
    "........"
    "........"
    "........"
    "PPPPPPPP"
    "RNBQKBNR"
)
PIECE_TABLE = ".PNBRQK?pnbrqk"


def pack_wire(cells: str = START, ply: int = 0, flags: int = 1) -> bytes:
    wire = bytearray(45)
    for i in range(32):
        hi = PIECE_TABLE.index(cells[i * 2])
        lo = PIECE_TABLE.index(cells[i * 2 + 1])
        wire[i] = (hi << 4) | lo
    wire[32] = (ply & 1) | (0x0F << 2)
    wire[33] = 0xFF
    wire[34] = ply & 0xFF
    wire[35] = ply >> 8
    wire[36] = flags
    wire[43] = 1
    wire[44] = crc8(wire[:44])
    return bytes(wire)


def pack_stj1(records: list[int], complete: bool = True) -> bytes:
    body = bytearray()
    for rec in records:
        body.append(rec & 0xFF)
        body.append(rec >> 8)
    header = bytearray(STJ_HEADER_SIZE)
    header[0:4] = b"STJ1"
    header[4] = 1
    header[5] = 1 if complete else 0
    header[6] = STJ_HEADER_SIZE
    header[8] = len(records) & 0xFF
    header[9] = len(records) >> 8
    jcrc = crc16_ccitt(body)
    header[10] = jcrc & 0xFF
    header[11] = jcrc >> 8
    header[16:76] = b64_encode(pack_wire(ply=len(records)))
    hcrc = crc16_ccitt(header)
    header[14] = hcrc & 0xFF
    header[15] = hcrc >> 8
    return bytes(header) + bytes(body)


def pack_move(frm: int, to: int, promo: int = 0) -> int:
    return (frm & 63) | ((to & 63) << 6) | ((promo & 7) << 12)


def test_empty_crc():
    assert crc16_ccitt(b"") == CRC_INIT


def test_e2e4_text():
    rec = pack_move(52, 36)
    assert record_text(rec) == "e2e4"
    assert record_text(UNDO_ONE) == "TAKEBACK"
    assert record_text(pack_move(12, 4, 1)) == "e7e8q"


def test_stj1_three_moves_and_takeback():
    records = [
        pack_move(52, 36),
        pack_move(12, 28),
        pack_move(53, 45),
        UNDO_ONE,
    ]
    data = pack_stj1(records)
    assert len(data) == 84
    parsed = parse_bytes(data)
    assert parsed["kind"] == "STJ1"
    assert parsed["n"] == 4
    assert parsed["complete"] is True
    assert parsed["hdr_crc_ok"] and parsed["body_crc_ok"]
    assert parsed["issues"] == []
    assert [record_text(r) for r in parsed["records"]] == [
        "e2e4",
        "e7e5",
        "f2f3",
        "TAKEBACK",
    ]
    report = format_report("demo.STJ", parsed)
    assert "n=4" in report
    assert "completo" in report
    assert "e2e4" in report
    assert "TAKEBACK" in report


def test_tmp_and_legacy_and_bad_crc():
    rec = pack_move(52, 36)
    tmp = bytes((rec & 0xFF, rec >> 8))
    parsed = parse_bytes(tmp)
    assert parsed["kind"] == "TMP"
    assert parsed["records"] == [rec]

    parsed = parse_bytes(tmp * 30, "SHATJRNL.TMP")
    assert parsed["kind"] == "TMP"
    assert parsed["records"] == [rec] * 30

    snap = b64_encode(pack_wire())
    parsed = parse_bytes(snap)
    assert parsed["kind"] == "LEGACY"
    assert parsed["snapshot"]["ply"] == 0
    assert parsed["snapshot"]["cells"] == START

    broken = bytearray(pack_stj1([rec]))
    broken[10] ^= 1
    parsed = parse_bytes(bytes(broken))
    assert "CRC del diario" in parsed["issues"]


def test_cli_roundtrip_file():
    data = pack_stj1([pack_move(52, 36, 0), pack_move(12, 4, 1)])
    with tempfile.NamedTemporaryFile(suffix=".STJ", delete=False) as fh:
        fh.write(data)
        name = fh.name
    parsed = parse_bytes(Path(name).read_bytes())
    Path(name).unlink()
    assert [record_text(r) for r in parsed["records"]] == ["e2e4", "e7e8q"]
    assert "dama" in format_report(name, parsed)


if __name__ == "__main__":
    test_empty_crc()
    test_e2e4_text()
    test_stj1_three_moves_and_takeback()
    test_tmp_and_legacy_and_bad_crc()
    test_cli_roundtrip_file()
    print("stj_read tests ok")
