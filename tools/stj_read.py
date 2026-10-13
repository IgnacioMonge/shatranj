#!/usr/bin/env python3
"""Read Classic Shatranj saves: STJ1, 60-byte legacy snapshots, and SHATJRNL.TMP."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

STJ_MAGIC = b"STJ1"
STJ_HEADER_SIZE = 76
STJ_VERSION = 1
STJ_FLAG_COMPLETE = 0x01
CRC_INIT = 0xFFFF
WIRE_SIZE = 45
B64_SIZE = 60
UNDO_ONE = 0x8000
PROMO = ("", "q", "r", "b", "n")
PROMO_ES = ("", "dama", "torre", "alfil", "caballo")
PIECE_TABLE = ".PNBRQK?pnbrqk"
B64_ALPHABET = (
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
)


def crc16_ccitt(data: bytes, crc: int = CRC_INIT) -> int:
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def crc8(data: bytes) -> int:
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def square_name(index: int) -> str:
    return "abcdefgh"[index & 7] + "87654321"[index >> 3]


def record_text(rec: int) -> str:
    if rec & 0x8000:
        if rec == UNDO_ONE:
            return "TAKEBACK"
        return f"CONTROL 0x{rec:04x}"
    frm = rec & 63
    to = (rec >> 6) & 63
    promo = (rec >> 12) & 7
    move = square_name(frm) + square_name(to)
    if 1 <= promo <= 4:
        return move + PROMO[promo]
    if promo:
        return f"{move}?promo={promo}"
    return move


def decode_records(body: bytes) -> tuple[list[int], str | None]:
    if len(body) & 1:
        return [], "longitud impar"
    out: list[int] = []
    for i in range(0, len(body), 2):
        rec = body[i] | (body[i + 1] << 8)
        if rec & 0x8000:
            if rec != UNDO_ONE:
                return out, f"registro {len(out)}: control 0x{rec:04x}"
        else:
            promo = (rec >> 12) & 7
            if promo > 4:
                return out, f"registro {len(out)}: promoción {promo}"
        out.append(rec)
    return out, None


def b64_encode(wire: bytes) -> bytes:
    if len(wire) != WIRE_SIZE:
        raise ValueError("wire")
    out = bytearray()
    for i in range(0, WIRE_SIZE, 3):
        block = (wire[i] << 16) | (wire[i + 1] << 8) | wire[i + 2]
        for shift in (18, 12, 6, 0):
            out.append(ord(B64_ALPHABET[(block >> shift) & 63]))
    return bytes(out)


def b64_decode(text: bytes) -> bytes | None:
    if len(text) != B64_SIZE:
        return None
    try:
        chars = text.decode("ascii")
    except UnicodeDecodeError:
        return None
    out = bytearray()
    for i in range(0, B64_SIZE, 4):
        try:
            a, b, c, d = (B64_ALPHABET.index(ch) for ch in chars[i : i + 4])
        except ValueError:
            return None
        out.append(((a << 2) | (b >> 4)) & 0xFF)
        out.append(((b << 4) | (c >> 2)) & 0xFF)
        out.append(((c << 6) | d) & 0xFF)
    return bytes(out)


def unpack_wire(wire: bytes) -> dict | str:
    if len(wire) != WIRE_SIZE:
        return "wire distinto de 45 B"
    if wire[43] != 1 or crc8(wire[:44]) != wire[44]:
        return "CRC o versión del snapshot"
    cells = []
    for packed in wire[:32]:
        hi, lo = packed >> 4, packed & 0x0F
        if hi >= len(PIECE_TABLE) or lo >= len(PIECE_TABLE) or hi == 7 or lo == 7:
            return "pieza reservada"
        cells.append(PIECE_TABLE[hi])
        cells.append(PIECE_TABLE[lo])
    meta = wire[32]
    if meta & 0x80:
        return "bit reservado en meta"
    castle = (meta >> 2) & 0x0F
    flags = wire[36]
    rights = ""
    rights += "K" if castle & 1 else ""
    rights += "Q" if castle & 2 else ""
    rights += "k" if castle & 4 else ""
    rights += "q" if castle & 8 else ""
    state_bits = []
    if flags & 1:
        state_bits.append("activa")
    if flags & 2:
        state_bits.append("terminada")
    if flags & 4:
        state_bits.append("jaque")
    ep = wire[33]
    return {
        "cells": "".join(cells),
        "side": "negras" if meta & 1 else "blancas",
        "host": "negras" if (meta >> 1) & 1 else "blancas",
        "castle": rights or "-",
        "flipped": bool(meta & 0x40),
        "ep": "-" if ep == 0xFF else square_name(ep),
        "ply": wire[34] | (wire[35] << 8),
        "flags": flags,
        "state": ", ".join(state_bits) if state_bits else "inactiva",
        "game_clock": f"{wire[37]:02d}:{wire[38]:02d}:{wire[39]:02d}",
        "move_clock": f"{wire[40]:02d}:{wire[41]:02d}:{wire[42]:02d}",
    }


def format_board(cells: str) -> list[str]:
    lines = ["    a b c d e f g h"]
    for rank in range(8):
        row = cells[rank * 8 : rank * 8 + 8]
        lines.append(f"  {8 - rank} " + " ".join(row))
    return lines


def parse_stj1(data: bytes) -> dict:
    header = bytearray(data[:STJ_HEADER_SIZE])
    stored_hdr_crc = header[14] | (header[15] << 8)
    header[14] = 0
    header[15] = 0
    hdr_crc = crc16_ccitt(header)
    flags = data[5]
    n = data[8] | (data[9] << 8)
    body_crc = data[10] | (data[11] << 8)
    reserved = data[12] | (data[13] << 8)
    need = STJ_HEADER_SIZE + n * 2
    body = data[STJ_HEADER_SIZE : need]
    extra = data[need:]
    records, rec_err = decode_records(body)
    got_crc = crc16_ccitt(body) if len(body) == n * 2 else None
    snap = unpack_wire(b64_decode(data[16:76]) or b"")
    issues = []
    if data[4] != STJ_VERSION:
        issues.append(f"versión {data[4]}")
    if flags & ~STJ_FLAG_COMPLETE:
        issues.append(f"flags 0x{flags:02x}")
    if (data[6] | (data[7] << 8)) != STJ_HEADER_SIZE:
        issues.append("tamaño de cabecera")
    if reserved:
        issues.append("reservado")
    if hdr_crc != stored_hdr_crc:
        issues.append("CRC de cabecera")
    if len(data) < need:
        issues.append(f"cuerpo corto ({len(body)} B, n={n})")
    elif extra:
        issues.append(f"{len(extra)} B de más")
    if rec_err:
        issues.append(rec_err)
    if got_crc is not None and got_crc != body_crc:
        issues.append("CRC del diario")
    return {
        "kind": "STJ1",
        "n": n,
        "complete": bool(flags & STJ_FLAG_COMPLETE),
        "hdr_crc_ok": hdr_crc == stored_hdr_crc,
        "body_crc_ok": got_crc == body_crc,
        "records": records,
        "snapshot": snap,
        "issues": issues,
        "size": len(data),
    }


def parse_bytes(data: bytes, name: str = "") -> dict:
    if len(data) >= 4 and data[:4] == STJ_MAGIC:
        if len(data) < STJ_HEADER_SIZE:
            return {
                "kind": "STJ1",
                "issues": ["cabecera truncada"],
                "size": len(data),
                "records": [],
                "n": 0,
                "complete": False,
                "hdr_crc_ok": False,
                "body_crc_ok": False,
                "snapshot": "cabecera truncada",
            }
        return parse_stj1(data)
    if len(data) == B64_SIZE and not name.upper().endswith(".TMP"):
        snap = unpack_wire(b64_decode(data) or b"")
        return {
            "kind": "LEGACY",
            "n": 0,
            "complete": False,
            "hdr_crc_ok": True,
            "body_crc_ok": True,
            "records": [],
            "snapshot": snap,
            "issues": [] if isinstance(snap, dict) else [str(snap)],
            "size": len(data),
        }
    records, rec_err = decode_records(data)
    return {
        "kind": "TMP",
        "n": len(records),
        "complete": rec_err is None and not (len(data) & 1),
        "hdr_crc_ok": True,
        "body_crc_ok": True,
        "records": records,
        "snapshot": None,
        "issues": [rec_err] if rec_err else [],
        "size": len(data),
    }


def format_report(path: str, parsed: dict) -> str:
    kind = parsed["kind"]
    lines = [f"{kind}  {path}  {parsed['size']} B"]
    if kind == "STJ1":
        hist = "completo" if parsed["complete"] else "parcial"
        lines.append(
            f"  historial    {hist}   n={parsed['n']}   "
            f"CRC cabecera {'ok' if parsed['hdr_crc_ok'] else 'MAL'}   "
            f"CRC diario {'ok' if parsed['body_crc_ok'] else 'MAL'}"
        )
    elif kind == "LEGACY":
        lines.append("  snapshot de 60 B, sin diario (carga parcial en Classic)")
    else:
        lines.append("  temporal sin cabecera (registros de 2 B)")
    for issue in parsed.get("issues") or []:
        lines.append(f"  AVISO        {issue}")
    snap = parsed.get("snapshot")
    if isinstance(snap, dict):
        lines.append(
            f"  instantánea  ply {snap['ply']}  turno {snap['side']}  "
            f"host {snap['host']}"
        )
        lines.append(
            f"  estado       {snap['state']}  enroque {snap['castle']}  "
            f"al paso {snap['ep']}  "
            f"vista {'volteada' if snap['flipped'] else 'normal'}"
        )
        lines.append(
            f"  relojes      partida {snap['game_clock']}  "
            f"turno {snap['move_clock']}"
        )
        lines.append("  tablero")
        lines.extend(format_board(snap["cells"]))
    elif isinstance(snap, str):
        lines.append(f"  instantánea  no se pudo leer ({snap})")
    records = parsed.get("records") or []
    if records or kind != "LEGACY":
        lines.append(f"  diario       {len(records)} registros")
        for i, rec in enumerate(records):
            label = record_text(rec)
            extra = ""
            if not (rec & 0x8000):
                promo = (rec >> 12) & 7
                if 1 <= promo <= 4:
                    extra = f"  ({PROMO_ES[promo]})"
            lines.append(f"    {i:3d}  {label:10s}  {rec:04x}{extra}")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Muestra cabecera, tablero y jugadas de un .STJ o SHATJRNL.TMP."
    )
    parser.add_argument("files", nargs="+", help="ficheros .STJ o .TMP")
    args = parser.parse_args(argv)
    rc = 0
    for i, name in enumerate(args.files):
        path = Path(name)
        if i:
            print()
        if not path.is_file():
            print(f"no existe  {path}", file=sys.stderr)
            rc = 1
            continue
        parsed = parse_bytes(path.read_bytes(), path.name)
        print(format_report(str(path), parsed))
        if parsed.get("issues"):
            rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
