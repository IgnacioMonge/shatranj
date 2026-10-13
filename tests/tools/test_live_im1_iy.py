#!/usr/bin/env python3
"""Guard Classic/Next overlay code that runs with interrupts enabled."""

import argparse
import re
import subprocess
import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import ROOT, required_tool


OBJECTS = (
    Path("ovl/DIRECT/direct_ovl_0.o"),
    Path("ovl/FILEUI/fileui_ovl_0.o"),
    Path("ovl/GUI_LOG/gui_log_ovl_0.o"),
)
ENTRY_SOURCES = (
    Path("asm/overlay/direct/entry_direct.asm"),
    Path("asm/overlay/gui_log/entry_gui_log.asm"),
)
PRESERVE_ONLY_SOURCES = (
    Path("asm/esxdos/esx_fileio_spectalk.asm"),
    Path("asm/overlay/about/entry_about.asm"),
)


def run(command: list[str]) -> str:
    result = subprocess.run(
        command,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout


def object_code(path: Path) -> bytes:
    output = run([required_tool("z88dk-z80nm"), "-c", str(path)])
    match = re.search(
        r'^  Section code_compiler: (\d+) bytes\n(.*?)(?=^  Section |^  Symbols:)',
        output,
        re.M | re.S,
    )
    assert match, f"{path}: missing code_compiler section"
    data = bytes(
        int(value, 16)
        for line in match.group(2).splitlines()
        for value in re.findall(r"\b[0-9A-F]{2}\b", line.partition(":")[2])
    )
    assert len(data) == int(match.group(1)), (path, len(data), match.group(1))
    return data


def check_object(path: Path) -> None:
    data = object_code(path)
    with tempfile.TemporaryDirectory() as directory:
        binary = Path(directory) / "code.bin"
        binary.write_bytes(data)
        output = run([
            required_tool("z88dk-dis"),
            "-mz80",
            "-o", "0",
            "-s", "0",
            "-e", str(len(data)),
            str(binary),
        ])
    assert re.search(r"\biy\b", output, re.I) is None, (
        f"{path}: SDCC emitted IY use into a live-interrupt overlay\n{output}"
    )


def check_entry_sources() -> None:
    for relative in ENTRY_SOURCES:
        source = (ROOT / relative).read_text(encoding="utf-8")
        code = "\n".join(line.split(";", 1)[0] for line in source.splitlines())
        assert re.search(r"\biy\b", code, re.I) is None, (
            f"{relative}: handwritten live-interrupt entry code uses IY"
        )

    for relative in PRESERVE_ONLY_SOURCES:
        source = (ROOT / relative).read_text(encoding="utf-8")
        for number, source_line in enumerate(source.splitlines(), 1):
            line = source_line.split(";", 1)[0].strip().lower()
            if re.search(r"\biy\b", line) and line not in ("push iy", "pop iy"):
                raise AssertionError(
                    f"{relative}:{number}: live-interrupt closure modifies IY: {line}"
                )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir", nargs="+", type=Path)
    args = parser.parse_args()

    check_entry_sources()
    for build_dir in args.build_dir:
        for relative in OBJECTS:
            check_object(build_dir / relative)
        print(f"{build_dir}: live-interrupt overlay objects preserve IY")


if __name__ == "__main__":
    main()
