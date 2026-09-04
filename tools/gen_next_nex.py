#!/usr/bin/env python3
"""Build a ZX Spectrum Next .nex from Shatranj resident code plus OVL/DAT bundle."""

from __future__ import annotations

import argparse
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

from gen_overlay_defs import parse_map
from next_bundle_codec import BANK_SIZE, PAGE_SIZE, pack_pages, unpack_pages

NEX_HEADER_SIZE = 512
MAIN_BASE = 0x4000
MAIN_SIZE = 0xC000
MIN_BUNDLE_SIZE = BANK_SIZE * 2
MAIN_BANKS = ((5, 0x4000), (2, 0x8000), (0, 0xC000))
LOADER_COMPRESSED_BANK_BASE = 8
LOADER_RAW_BANK_BASE = 16
LOADER_RAW_BANK_COUNT = 15
OVERLAY_PAGE_COUNT = 17
OVERLAY_EXEC_SIZE = 0x1000
RESIDENT_MIRROR_BASE = 0x7000
RESIDENT_MIRROR_END = RESIDENT_MIRROR_BASE + OVERLAY_EXEC_SIZE
EXTENSION_TABLE_SIZE = 147
MIRROR_INSTALL_SYMBOLS = (
    "next_install_overlay_mirrors",
    "next_map_slot2",
    "next_map_slot3",
    "nextreg_write",
)

def symbol(symbols: dict[str, int], *names: str) -> int:
    for name in names:
        if name in symbols:
            return symbols[name]
    raise SystemExit("missing map symbol: " + " or ".join(names))


def resident_length(symbols: dict[str, int], origin: int) -> int:
    return symbol(symbols, "__DATA_END_tail") - origin


def ranges_overlap(a0: int, a1: int, b0: int, b1: int) -> bool:
    return a0 < b1 and b0 < a1


@dataclass(frozen=True)
class BundleRegion:
    label: str
    offset: int
    data: bytes

    @property
    def end(self) -> int:
        return self.offset + len(self.data)

    def overlaps(self, other: "BundleRegion") -> bool:
        return ranges_overlap(self.offset, self.end, other.offset, other.end)


def append_bundle_region(
    regions: list[BundleRegion], region: BundleRegion, overlap_message: str
) -> None:
    if any(region.overlaps(used) for used in regions):
        raise SystemExit(overlap_message)
    regions.append(region)


def render_bundle(regions: list[BundleRegion], size: int) -> bytes:
    bundle = bytearray(size)
    for region in regions:
        bundle[region.offset : region.end] = region.data
    return bytes(bundle)


def render_overlay_pages(atlas: bytes, resident_mirror: bytes) -> bytes:
    expected = OVERLAY_PAGE_COUNT * PAGE_SIZE
    if len(atlas) != expected:
        raise SystemExit(
            f"Next requires {OVERLAY_PAGE_COUNT} 8K overlay pages, got {len(atlas)} bytes"
        )
    if len(resident_mirror) != OVERLAY_EXEC_SIZE:
        raise SystemExit(
            f"Next resident mirror must be {OVERLAY_EXEC_SIZE} bytes"
        )

    pages = bytearray(expected)
    for index in range(OVERLAY_PAGE_COUNT):
        start = index * PAGE_SIZE
        split = start + OVERLAY_EXEC_SIZE
        end = start + PAGE_SIZE
        if any(atlas[split:end]):
            raise SystemExit(f"Next overlay page {index} uses its resident mirror half")
        pages[start:split] = atlas[start:split]
        pages[split:end] = resident_mirror
    return bytes(pages)


def validate_resident_mirror(symbols: dict[str, int]) -> None:
    for section in ("data_compiler", "data_user", "bss_compiler", "bss_user"):
        start = symbol(symbols, f"__{section}_head")
        end = symbol(symbols, f"__{section}_tail")
        if start != end and ranges_overlap(
            start, end, RESIDENT_MIRROR_BASE, RESIDENT_MIRROR_END
        ):
            raise SystemExit(f"writable section {section} overlaps resident mirror")


def validate_mirror_installer(symbols: dict[str, int]) -> None:
    for name in MIRROR_INSTALL_SYMBOLS:
        address = symbol(symbols, name)
        if address < RESIDENT_MIRROR_END:
            raise SystemExit(
                f"Next mirror installer symbol {name} is in a remapped MMU window"
            )


def validate_graphics_bank(
    blob: bytes, offset: int, origin: int, used: list[tuple[int, int]]
) -> int:
    if len(blob) < EXTENSION_TABLE_SIZE:
        raise SystemExit("extension bank is missing its jump table")
    end = offset + len(blob)
    if offset % PAGE_SIZE != origin % PAGE_SIZE:
        raise SystemExit("graphics bank origin does not match its bundle page offset")
    if offset // PAGE_SIZE != (end - 1) // PAGE_SIZE:
        raise SystemExit("graphics bank crosses its 8K MMU page")
    for start, used_end in used:
        if ranges_overlap(offset, end, start, used_end):
            raise SystemExit("graphics bank overlaps another bundle area")
    for entry in range(EXTENSION_TABLE_SIZE // 3):
        table = entry * 3
        if blob[table] != 0xC3:
            raise SystemExit("extension bank entry table contains a non-jump")
        target = int.from_bytes(blob[table + 1 : table + 3], "little")
        if not origin + EXTENSION_TABLE_SIZE <= target < origin + len(blob):
            raise SystemExit("graphics bank jump target is outside the linked blob")
    return end


def build_header(load_banks: list[int], pc: int, sp: int) -> bytearray:
    header = bytearray(NEX_HEADER_SIZE)
    header[0:4] = b"Next"
    header[4:8] = b"V1.1"
    header[8] = 1 if any(bank >= 48 for bank in load_banks) else 0
    header[9] = len(load_banks)
    header[11] = 0
    header[12:14] = sp.to_bytes(2, "little")
    header[14:16] = pc.to_bytes(2, "little")
    for bank in load_banks:
        if not 0 <= bank < 112:
            raise SystemExit(f"bank out of NEX range: {bank}")
        header[18 + bank] = 1
    return header


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--map", type=Path, required=True)
    parser.add_argument("--code-bin", type=Path, required=True)
    parser.add_argument("--ovl", type=Path, required=True)
    parser.add_argument(
        "--overlay-offset", type=lambda value: int(value, 0), default=98304
    )
    parser.add_argument("--dat", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--org", type=lambda value: int(value, 0), default=RESIDENT_MIRROR_BASE
    )
    parser.add_argument("--bundle-bank-base", type=int, default=8)
    parser.add_argument("--raw-bank-base", type=int, default=16)
    parser.add_argument("--max-bundle-banks", type=int, default=4)
    parser.add_argument("--zx0", default="z88dk-zx0")
    parser.add_argument("--dat-offset", type=lambda value: int(value, 0), default=8192)
    parser.add_argument("--sprite-patterns", type=Path)
    parser.add_argument(
        "--sprite-offset", type=lambda value: int(value, 0), default=16384
    )
    parser.add_argument("--sprite-pal", type=Path)
    parser.add_argument(
        "--sprite-pal-offset", type=lambda value: int(value, 0), default=32768
    )
    parser.add_argument("--about-nxi", type=Path)
    parser.add_argument(
        "--about-pal-offset", type=lambda value: int(value, 0), default=33792
    )
    parser.add_argument(
        "--about-pixels-offset", type=lambda value: int(value, 0), default=49152
    )
    parser.add_argument("--graphics-bank", type=Path)
    parser.add_argument(
        "--graphics-bank-offset", type=lambda value: int(value, 0), default=0
    )
    parser.add_argument(
        "--graphics-bank-org", type=lambda value: int(value, 0), default=0x2000
    )
    args = parser.parse_args(argv)

    if args.bundle_bank_base != LOADER_COMPRESSED_BANK_BASE:
        raise SystemExit(
            f"loader requires compressed bundle bank {LOADER_COMPRESSED_BANK_BASE}"
        )
    if args.raw_bank_base != LOADER_RAW_BANK_BASE:
        raise SystemExit(f"loader requires raw bundle bank {LOADER_RAW_BANK_BASE}")
    if args.max_bundle_banks <= 0:
        raise SystemExit("max bundle banks must be positive")

    if args.about_nxi and args.about_pixels_offset % BANK_SIZE != 0:
        raise SystemExit(
            "about pixels offset must be 16K-bank aligned (Layer 2 shows them in place)"
        )

    if args.dat_offset % 8192 != 0:
        raise SystemExit("DAT offset must be 8K-page aligned")
    if args.overlay_offset % PAGE_SIZE != 0:
        raise SystemExit("overlay offset must be 8K-page aligned")

    symbols = parse_map(args.map)
    pc = symbol(symbols, "__crt_org_code")
    sp = symbol(symbols, "__register_sp", "TAR__register_sp")
    if args.org != RESIDENT_MIRROR_BASE or pc != args.org:
        raise SystemExit("Next resident image must start at 0x7000")
    validate_resident_mirror(symbols)
    validate_mirror_installer(symbols)
    resident_len = resident_length(symbols, args.org)
    if resident_len <= 0:
        raise SystemExit(f"invalid resident length: {resident_len}")

    code = args.code_bin.read_bytes()
    if resident_len > len(code):
        raise SystemExit(f"resident length {resident_len} exceeds code bin {len(code)}")
    if args.org + resident_len > 0x10000:
        raise SystemExit("resident image must fit in 0x7000..0xffff")

    main_mem = bytearray(MAIN_SIZE)
    main_offset = args.org - MAIN_BASE
    main_mem[main_offset : main_offset + resident_len] = code[:resident_len]
    mirror_offset = RESIDENT_MIRROR_BASE - MAIN_BASE
    resident_mirror = bytes(
        main_mem[mirror_offset : mirror_offset + OVERLAY_EXEC_SIZE]
    )

    main_present: list[int] = []
    resident_start = args.org
    resident_end = args.org + resident_len
    for bank, addr in MAIN_BANKS:
        if ranges_overlap(resident_start, resident_end, addr, addr + BANK_SIZE):
            main_present.append(bank)

    ovl_atlas = args.ovl.read_bytes()
    # Validate the complete runtime shape. The NEX stores the mirror once in
    # bank 5; boot copies it into each expanded overlay page.
    render_overlay_pages(ovl_atlas, resident_mirror)
    ovl = ovl_atlas
    dat = args.dat.read_bytes()
    sprite_patterns = args.sprite_patterns.read_bytes() if args.sprite_patterns else b""
    graphics_bank = args.graphics_bank.read_bytes() if args.graphics_bank else b""
    regions = [
        BundleRegion("OVL", args.overlay_offset, ovl),
        BundleRegion("DAT", args.dat_offset, dat),
    ]
    if sprite_patterns:
        sprite_region = BundleRegion(
            "sprite patterns", args.sprite_offset, sprite_patterns
        )
        if sprite_region.overlaps(regions[0]):
            raise SystemExit("sprite patterns overlap OVL bundle area")
        if sprite_region.overlaps(regions[1]):
            raise SystemExit("sprite patterns overlap DAT bundle area")
        regions.append(sprite_region)
    sprite_pal = args.sprite_pal.read_bytes() if args.sprite_pal else b""
    if sprite_pal:
        append_bundle_region(
            regions,
            BundleRegion("sprite palette", args.sprite_pal_offset, sprite_pal),
            "sprite palette overlaps another bundle area",
        )
    if args.about_nxi:
        about = args.about_nxi.read_bytes()
        if len(about) != 512 + 49152:
            raise SystemExit(
                f"{args.about_nxi}: expected 49664-byte .nxi (512 palette + 48K pixels)"
            )
        about_pal, about_pixels = about[:512], about[512:]
        about_pal_region = BundleRegion(
            "about palette", args.about_pal_offset, about_pal
        )
        about_pixels_region = BundleRegion(
            "about pixels", args.about_pixels_offset, about_pixels
        )
        if any(
            about_pixels_region.overlaps(region)
            for region in regions + [about_pal_region]
        ):
            raise SystemExit("about pixels overlap another bundle area")
        append_bundle_region(
            regions,
            about_pal_region,
            "about palette overlaps another bundle area",
        )
        regions.append(about_pixels_region)
    if graphics_bank:
        validate_graphics_bank(
            graphics_bank,
            args.graphics_bank_offset,
            args.graphics_bank_org,
            [(region.offset, region.end) for region in regions],
        )
        regions.append(
            BundleRegion("graphics bank", args.graphics_bank_offset, graphics_bank)
        )
    bundle_end = max(MIN_BUNDLE_SIZE, max(region.end for region in regions))
    bundle_size = ((bundle_end + BANK_SIZE - 1) // BANK_SIZE) * BANK_SIZE
    if bundle_size != LOADER_RAW_BANK_COUNT * BANK_SIZE:
        raise SystemExit(
            f"loader requires a {LOADER_RAW_BANK_COUNT}-bank raw bundle, "
            f"got {bundle_size // BANK_SIZE} banks"
        )

    bundle = render_bundle(regions, bundle_size)

    if bundle_size % PAGE_SIZE != 0:
        raise SystemExit("raw bundle is not an exact number of 8K pages")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="next_bundle_zx0_", dir=args.out.parent
    ) as temp_name:
        packed = pack_pages(
            bundle, args.zx0, Path(temp_name), args.raw_bank_base
        )
    restored, raw_bank_base = unpack_pages(packed)
    if restored != bundle or raw_bank_base != args.raw_bank_base:
        raise SystemExit("compressed bundle failed byte-for-byte host reconstruction")

    packed_size = ((len(packed) + BANK_SIZE - 1) // BANK_SIZE) * BANK_SIZE
    packed_banks = packed_size // BANK_SIZE
    if packed_banks > args.max_bundle_banks:
        raise SystemExit(
            f"compressed bundle needs {packed_banks} banks, "
            f"limit is {args.max_bundle_banks}"
        )
    raw_banks = bundle_size // BANK_SIZE
    compressed_end = args.bundle_bank_base + packed_banks
    raw_end = args.raw_bank_base + raw_banks
    if ranges_overlap(
        args.bundle_bank_base, compressed_end, args.raw_bank_base, raw_end
    ):
        raise SystemExit("compressed and expanded bundle banks overlap")
    for bank in main_present:
        if args.bundle_bank_base <= bank < compressed_end:
            raise SystemExit("compressed bundle overlaps main RAM bank")
        if args.raw_bank_base <= bank < raw_end:
            raise SystemExit("expanded bundle overlaps main RAM bank")

    packed_bundle = packed + bytes(packed_size - len(packed))
    bundle_banks = list(range(args.bundle_bank_base, compressed_end))
    load_banks = main_present + bundle_banks
    header = build_header(load_banks, pc, sp)

    with args.out.open("wb") as handle:
        handle.write(header)
        for bank, addr in MAIN_BANKS:
            if bank in main_present:
                offset = addr - MAIN_BASE
                handle.write(main_mem[offset : offset + BANK_SIZE])
        for index, _bank in enumerate(bundle_banks):
            offset = index * BANK_SIZE
            handle.write(packed_bundle[offset : offset + BANK_SIZE])

    banks = ",".join(str(bank) for bank in load_banks)
    sprite_msg = f"; sprites {len(sprite_patterns)}" if sprite_patterns else ""
    graphics_msg = f"; graphics {len(graphics_bank)}" if graphics_bank else ""
    print(
        f"[OK] {args.out}: NEX banks {banks}; resident {resident_len}; "
        f"OVL {len(ovl_atlas)} ({OVERLAY_PAGE_COUNT} boot-mirrored pages); "
        f"DAT {len(dat)}{sprite_msg}{graphics_msg}; "
        f"bundle {bundle_size}->{len(packed)} bytes ({packed_banks} banks)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
