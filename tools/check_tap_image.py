#!/usr/bin/env python3
"""Verify that a Spectrum TAP loads all initialized data and zeroed BSS."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


MAP_SYMBOL = re.compile(r"^(\w+)\s+=\s+\$([0-9A-Fa-f]+)\s+;", re.MULTILINE)


class TapImageError(ValueError):
    pass


@dataclass(frozen=True)
class TapImage:
    load_address: int
    payload: bytes
    initialized_bytes: int
    bss_bytes: int
    data_after_compiler: int


def parse_map(text: str) -> dict[str, int]:
    return {name: int(value, 16) for name, value in MAP_SYMBOL.findall(text)}


def required_symbol(symbols: dict[str, int], name: str) -> int:
    try:
        return symbols[name]
    except KeyError as exc:
        raise TapImageError(f"map symbol missing: {name}") from exc


def parse_blocks(blob: bytes) -> list[tuple[int, bytes]]:
    blocks: list[tuple[int, bytes]] = []
    offset = 0

    while offset < len(blob):
        if offset + 2 > len(blob):
            raise TapImageError("truncated TAP block length")
        size = int.from_bytes(blob[offset : offset + 2], "little")
        offset += 2
        if size < 2 or offset + size > len(blob):
            raise TapImageError("invalid TAP block size")
        block = blob[offset : offset + size]
        offset += size
        checksum = 0
        for value in block:
            checksum ^= value
        if checksum != 0:
            raise TapImageError("invalid TAP block checksum")
        blocks.append((block[0], block[1:-1]))

    return blocks


def find_code_block(blob: bytes, org: int) -> tuple[int, bytes]:
    blocks = parse_blocks(blob)
    matches: list[tuple[int, bytes]] = []

    for index, (flag, header) in enumerate(blocks):
        if flag != 0 or len(header) != 17 or header[0] != 3:
            continue
        load_address = int.from_bytes(header[13:15], "little")
        if load_address != org:
            continue
        if index + 1 >= len(blocks):
            raise TapImageError("code header has no data block")
        data_flag, payload = blocks[index + 1]
        if data_flag != 0xFF:
            raise TapImageError("code header is not followed by a data block")
        declared = int.from_bytes(header[11:13], "little")
        if declared != len(payload):
            raise TapImageError(
                f"code header length {declared} != data block length {len(payload)}"
            )
        matches.append((declared, payload))

    if len(matches) != 1:
        raise TapImageError(
            f"expected one CODE block at 0x{org:04X}, found {len(matches)}"
        )
    return matches[0]


def validate_image(
    symbols: dict[str, int], code: bytes, tap: bytes, org: int,
    load_org: int | None = None, low_code: bytes = b"",
) -> TapImage:
    data_compiler_tail = required_symbol(symbols, "__data_compiler_tail")
    data_end = required_symbol(symbols, "__DATA_END_tail")
    bss_head = required_symbol(symbols, "__BSS_head")
    ram_end = max(
        required_symbol(symbols, "__BSS_END_tail"),
        symbols.get("__bss_compiler_tail", 0),
        symbols.get("__bss_user_tail", 0),
    )

    if not org <= data_compiler_tail <= data_end <= bss_head <= ram_end:
        raise TapImageError("invalid DATA/BSS ordering in linker map")

    resident_size = ram_end - org
    if len(code) != resident_size:
        raise TapImageError(
            f"CODE.bin length {len(code)} != RAM_END - ORG ({resident_size})"
        )

    if load_org is None:
        load_org = org
    if load_org > org:
        raise TapImageError("TAP load origin is above resident ORG")
    prefix_size = org - load_org
    if len(low_code) > prefix_size:
        raise TapImageError("low CODE section reaches resident ORG")
    expected_payload = low_code + bytes(prefix_size - len(low_code)) + code

    declared, payload = find_code_block(tap, load_org)
    if declared != len(expected_payload):
        raise TapImageError(
            f"TAP CODE length {declared} != expected {len(expected_payload)}"
        )
    if payload != expected_payload:
        raise TapImageError("TAP CODE payload differs from linked sections")

    bss_offset = bss_head - load_org
    if any(payload[bss_offset:]):
        raise TapImageError("TAP BSS contains non-zero bytes")

    return TapImage(
        load_address=load_org,
        payload=payload,
        initialized_bytes=bss_offset,
        bss_bytes=ram_end - bss_head,
        data_after_compiler=data_end - data_compiler_tail,
    )


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--map", type=Path, required=True)
    parser.add_argument("--code-bin", type=Path, required=True)
    parser.add_argument("--tap", type=Path, required=True)
    parser.add_argument("--org", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--load-org", type=lambda value: int(value, 0))
    parser.add_argument("--low-code-bin", type=Path)
    args = parser.parse_args(argv)

    try:
        image = validate_image(
            parse_map(args.map.read_text(encoding="utf-8", errors="replace")),
            args.code_bin.read_bytes(),
            args.tap.read_bytes(),
            args.org,
            args.load_org,
            args.low_code_bin.read_bytes() if args.low_code_bin else b"",
        )
    except (OSError, TapImageError) as exc:
        print(f"[ERR] TAP image contract: {exc}", file=sys.stderr)
        return 1

    end = image.load_address + len(image.payload)
    print(
        f"[OK] TAP image: 0x{image.load_address:04X}..0x{end - 1:04X}, "
        f"{len(image.payload)} bytes; initialized {image.initialized_bytes}, "
        f"BSS {image.bss_bytes} zero bytes, "
        f"post-compiler DATA {image.data_after_compiler}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
