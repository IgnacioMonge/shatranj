"""Emit assembly EQU values from the selected C presentation layout."""
import re
import sys
from pathlib import Path


def layout_values(header: Path) -> dict[str, str]:
    values = {}
    for path in (header.with_name("log_contract.h"), header):
        text = re.sub(r'/\*.*?\*/', '', path.read_text(encoding="ascii"), flags=re.S)
        text = text.replace('\\\n', ' ')
        for name, expression in re.findall(r'^#define[ \t]+(\w+)[ \t]+([^\n]+)', text, re.M):
            if '"' in expression:
                continue
            expression = re.sub(r'\b(0x[0-9a-fA-F]+|\d+)[uU]\b', r'\1', expression.strip())
            if not re.fullmatch(r'[\w\s()+*/|&<>~^-]+', expression):
                raise ValueError(f"{path}: unsupported layout expression {name}={expression}")
            if name in values and values[name] != expression:
                raise ValueError(f"{path}: duplicate layout constant {name}")
            values[name] = expression
    return values


def layout_asm(header: Path) -> str:
    values = layout_values(header)
    text = "; Generated from log_contract.h and " + header.name + "; do not edit.\n"
    text += ''.join(f"{name} EQU {value}\n" for name, value in values.items())
    text += ''.join(f"{name.removeprefix('NETCHESSZX_')} EQU {name}\n"
                    for name in values if name.startswith('NETCHESSZX_ATTR_'))
    return text


if __name__ == '__main__':
    header, output = map(Path, sys.argv[1:])
    text = layout_asm(header)
    if not output.exists() or output.read_text() != text:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="ascii")
