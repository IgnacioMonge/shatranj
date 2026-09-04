#!/usr/bin/env python3
"""Check the SDCC/IY ABI assumptions used by handwritten Z80 stubs."""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


PROBE_SOURCE = r"""
#include <stdint.h>

extern uint8_t abi_pair(uint8_t first, uint8_t second);

uint8_t abi_probe(void)
{
    return abi_pair(0x12u, 0x34u);
}
"""


def run(argv, cwd):
    return subprocess.run(
        argv,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def normalize(text):
    return "\n".join(line.strip().lower() for line in text.splitlines())


def check_rst8_iy_file(path, label):
    lines = []
    for source_line in path.read_text(encoding="utf-8").splitlines():
        line = source_line.split(";", 1)[0].strip().lower()
        if line:
            lines.append(line)

    rst_indices = [index for index, line in enumerate(lines) if line == "rst 8"]
    if not rst_indices:
        print(f"[ERR] {label} has no esxDOS RST 8 sites")
        return False
    for index in rst_indices:
        if (index == 0 or index + 2 >= len(lines) or
                lines[index - 1] != "push iy" or
                not lines[index + 1].startswith("defb 0x") or
                lines[index + 2] != "pop iy"):
            print(f"[ERR] {label} RST 8 at site {index} does not preserve IY")
            return False
    print(f"[OK] {label} preserves IY at {len(rst_indices)} RST 8 sites")
    return True


def conditional_lines(path):
    lines = []
    guards = []
    for source_line in path.read_text(encoding="utf-8").splitlines():
        line = source_line.split(";", 1)[0].strip().lower()
        directive = line.lstrip("#").strip().split()
        if directive and directive[0] in ("ifdef", "ifndef"):
            guards.append(
                directive[0] == "ifdef"
                if directive[1] == "netchesszx_next_banking" else None
            )
        elif directive and directive[0] == "else" and guards:
            if guards[-1] is not None:
                guards[-1] = not guards[-1]
        elif directive and directive[0] == "endif" and guards:
            guards.pop()
        elif line:
            lines.append((line, True in guards))
    return lines


def asm_equ(path, name):
    match = re.search(
        rf"(?mi)^{re.escape(name)}\s+equ\s+(0x[0-9a-f]+|[0-9]+)\s*$",
        path.read_text(encoding="utf-8"),
    )
    if not match:
        raise ValueError(f"{name} not found in {path}")
    return int(match.group(1), 0)


def check_next_rst8_mmu1(path, label, extension_page):
    lines = conditional_lines(path)

    rst_indices = [index for index, (line, _) in enumerate(lines) if line == "rst 8"]
    enter = "defb 0xed, 0x91, 0x51, 0xff"
    leaves = {
        "defb 0xed, 0x91, 0x51, next_extension_page",
        f"defb 0xed, 0x91, 0x51, 0x{extension_page:02x}",
    }
    if not rst_indices:
        print(f"[ERR] {label} has no Next RST 8 sites")
        return False
    if any(not guarded for line, guarded in lines if line == enter or line in leaves):
        print(f"[ERR] {label} has an MMU1 write outside NETCHESSZX_NEXT_BANKING")
        return False
    for index in rst_indices:
        before = lines[max(0, index - 3):index]
        after = lines[index + 1:index + 4]
        if (not any(line == enter and guarded for line, guarded in before) or
                not any(line in leaves and guarded for line, guarded in after)):
            print(f"[ERR] {label} RST 8 at site {index} does not expose and restore MMU1 ROM")
            return False
    print(f"[OK] {label} protects MMU1 at {len(rst_indices)} RST 8 sites")
    return True


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--zcc", default="zcc")
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--root", default=".")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    iy_contracts = (
        ("asm/esxdos/overlay_loader.asm", "Classic overlay loader"),
        ("asm/overlay/about/entry_about.asm", "Classic About overlay"),
    )
    for path, label in iy_contracts:
        if not check_rst8_iy_file(root / path, label):
            return 1

    next_mmu1_contracts = (
        ("asm/esxdos/esx_fileio_spectalk.asm", "Next file I/O"),
        ("asm/overlay/time_config/entry_time_config.asm", "Next TIME_CONFIG RTC"),
        ("src/spectrum/overlay/mqtt_tx_ovl.c", "Next MQTT_TX RTC"),
    )
    extension_page = asm_equ(
        root / "asm/next/extension_bank_layout.asm", "next_extension_page"
    )
    for path, label in next_mmu1_contracts:
        if not check_next_rst8_mmu1(root / path, label, extension_page):
            return 1

    zcc = shutil.which(args.zcc) or args.zcc
    build_dir = Path(args.build_dir)
    if not build_dir.is_absolute():
        build_dir = root / build_dir
    probe_dir = build_dir / "abi_probe_sdcc_iy"
    probe_dir.mkdir(parents=True, exist_ok=True)

    source_path = probe_dir / "abi_probe.c"
    source_path.write_text(PROBE_SOURCE, encoding="ascii")

    for old in probe_dir.glob("abi_probe*"):
        if old != source_path:
            old.unlink()

    cmd = [
        zcc,
        "+z80",
        "-vn",
        "-clib=sdcc_iy",
        "-SO3",
        "-compiler=sdcc",
        "-Cs--no-reg-params",
        "--opt-code-size",
        "--fomit-frame-pointer",
        "-S",
        str(source_path.name),
        "-o",
        "abi_probe",
    ]
    result = run(cmd, probe_dir)
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        print("[ERR] SDCC/IY ABI probe compile failed")
        return result.returncode

    asm_files = sorted(probe_dir.glob("abi_probe*.asm"))
    if not asm_files and (probe_dir / "abi_probe").exists():
        asm_files = [probe_dir / "abi_probe"]
    if not asm_files:
        print("[ERR] SDCC/IY ABI probe did not emit asm")
        return 1

    asm_text = normalize(asm_files[0].read_text(encoding="utf-8", errors="replace"))
    packed_word_patterns = [
        ("ld\thl,0x3412", "push\thl"),
        ("ld\tde,0x3412", "push\tde"),
        ("ld\tbc,0x3412", "push\tbc"),
    ]
    call_index = asm_text.find("call\t_abi_pair")
    if call_index < 0:
        print("[ERR] SDCC/IY ABI changed: abi_pair call not found")
        return 1
    before_call = asm_text[:call_index]
    if not any(load in before_call and push in before_call for load, push in packed_word_patterns):
        print("[ERR] SDCC/IY ABI changed: expected uint8,uint8 packed word 0x3412 before abi_pair")
        return 1

    print("[OK] SDCC/IY ABI probe: uint8,uint8 packed stack call")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
