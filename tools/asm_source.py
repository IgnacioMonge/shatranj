"""Read maintained assembly includes for source-level contract checks.

Generated bare-name includes remain for the real assembler/build to resolve.
This does not evaluate conditional assembly.
"""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read_asm(path: Path, root: Path = ROOT, seen: tuple[Path, ...] = (),
             layout: Path | None = None) -> str:
    path = path.resolve()
    if path in seen:
        raise ValueError(f"recursive ASM include: {path}")
    text = path.read_text(encoding="utf-8")
    if layout is None:
        family = "next" if path.is_relative_to(root / "asm/next") else "zx"
        layout = root / f"src/spectrum/ui/layout_{family}.h"

    def include(match):
        if match[1] == "ui_layout.inc":
            from gen_ui_layout import layout_asm
            return layout_asm(layout)
        child = root / match[1]
        if child.is_file():
            return read_asm(child, root, (*seen, path), layout)
        if '/' in match[1]:
            raise FileNotFoundError(child)
        return match[0]

    return re.sub(r'^\s*INCLUDE "([^"]+)"\s*$', include, text, flags=re.M)
