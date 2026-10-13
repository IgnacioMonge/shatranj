#!/usr/bin/env python3
"""Write the Spectranext TAP: BASIC loader, resident CODE, then the loading screen.

As SpecTalkZX does, the loader loads CODE first and the SCREEN$ last (so the
ROM's "Bytes:" line never lands on the picture), shows it for two seconds
(any key skips) and starts the program:

  10 CLEAR VAL "<clear>": BORDER VAL "0": LOAD "" CODE : LOAD "" SCREEN$ :
     PAUSE VAL "100": RANDOMIZE USR VAL "<usr>"
"""

from __future__ import annotations

import argparse
from pathlib import Path

from check_tap_image import tap_block, tape_header

SCREEN_ADDRESS = 16384
SCREEN_SIZE = 6912


def number(value: int) -> bytes:
    return b'\xb0"' + str(value).encode("ascii") + b'"'  # VAL "n"


def loader(clear_address: int, entry_address: int) -> bytes:
    body = (
        b"\xfd" + number(clear_address) + b":"           # CLEAR
        + b"\xe7" + number(0) + b":"                     # BORDER
        + b'\xef""\xaf:'                                 # LOAD "" CODE
        + b'\xef""\xaa:'                                 # LOAD "" SCREEN$
        + b"\xf2" + number(100) + b":"                   # PAUSE
        + b"\xf9\xc0" + number(entry_address) + b"\r"    # RANDOMIZE USR
    )
    return bytes((0, 10)) + len(body).to_bytes(2, "little") + body


def build(code: bytes, org: int, clear_address: int, entry_address: int,
          screen: bytes) -> bytes:
    if len(screen) != SCREEN_SIZE:
        raise SystemExit(f"loading screen must be {SCREEN_SIZE} bytes, not {len(screen)}")
    basic = loader(clear_address, entry_address)
    return (
        tap_block(0, tape_header(0, "Loader", len(basic), 10, len(basic)))
        + tap_block(0xFF, basic)
        + tap_block(0, tape_header(3, "SHATRANJ", len(code), org, 32768))
        + tap_block(0xFF, code)
        + tap_block(0, tape_header(3, "Shatranj", SCREEN_SIZE, SCREEN_ADDRESS, 32768))
        + tap_block(0xFF, screen)
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--code", type=Path, required=True)
    parser.add_argument("--org", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--clearaddr", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--usraddr", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--screen", type=Path, required=True)
    parser.add_argument("-o", "--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_bytes(build(args.code.read_bytes(), args.org, args.clearaddr,
                                  args.usraddr, args.screen.read_bytes()))


if __name__ == "__main__":
    main()
