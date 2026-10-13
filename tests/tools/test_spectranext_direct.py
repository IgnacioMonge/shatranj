#!/usr/bin/env python3
"""Check the generated direct resource: matching assets and remote launch."""

import hashlib
import json
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[2]
    output = root / "build/spectranext-direct"
    package = json.loads((output / "package.json").read_text())
    entries = {item["name"]: item for item in package["files"]}
    assert set(entries) == {"boot.zx", "SHATRANJ.tap", "SHATRANJ.DAT", "SHATRANJ.OVL"}
    for name, item in entries.items():
        data = (output / name).read_bytes()
        assert len(data) == item["size"], name
        assert hashlib.sha256(data).hexdigest() == item["sha256"], name
        if name != "boot.zx":
            assert data == (root / "release/Spectranext" / name).read_bytes(), name

    image = (output / "boot.zx").read_bytes()
    blocks = []
    while image:
        size = int.from_bytes(image[:2], "little")
        block, image = image[2:2 + size], image[2 + size:]
        assert size >= 2 and len(block) == size
        checksum = 0
        for byte in block:
            checksum ^= byte
        assert checksum == 0
        blocks.append(block)
    assert len(blocks) == 2 and blocks[0][:2] == b"\0\0"
    assert int.from_bytes(blocks[0][14:16], "little") == 10  # BASIC autostart
    program = blocks[1][1:-1]
    statement = program[4:]
    assert int.from_bytes(program[:2], "big") == 10
    assert int.from_bytes(program[2:4], "little") == len(statement)
    # %tapein on the inherited mount, then tokenized LOAD ""; no install/%fs.
    assert statement == b'%tapein"SHATRANJ.tap":\xef""\r'
    assert "name=boot.zx " in (output / "index.txt").read_text()

    # The TAP shows the installer's loading screen after its CODE, then runs.
    tape = (output / "SHATRANJ.tap").read_bytes()
    tape_blocks = []
    while tape:
        size = int.from_bytes(tape[:2], "little")
        tape_blocks.append(tape[3:2 + size - 1])
        tape = tape[2 + size:]
    assert len(tape_blocks) == 6
    assert tape_blocks[2][13:15] == (28160).to_bytes(2, "little")      # CODE header
    assert tape_blocks[4][13:15] == (16384).to_bytes(2, "little")      # SCREEN$ header
    assert tape_blocks[5] == (root / "assets/spectrum/installer.scr").read_bytes()
    assert tape_blocks[1][4:] == (
        b'\xfd\xb0"28159":\xe7\xb0"0":\xef""\xaf:\xef""\xaa:\xf2\xb0"100":'
        b'\xf9\xc0\xb0"28672"\r'
    )
    print("Direct HTTP(S) package: matching assets, loading screen and inherited-mount "
          "launcher PASS")


if __name__ == "__main__":
    main()
