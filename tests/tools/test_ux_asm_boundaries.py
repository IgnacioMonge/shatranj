#!/usr/bin/env python3
"""Positive and negative checks for the Spectrum UX ASM boundary guard."""

from __future__ import annotations

import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from check_ux_asm_boundaries import (INPUT, OWNED_FILES, RULES, SCREEN, SETUP_HEADER,
                                     SETUP_STATE, _setup_arena, check_root)


def copy_fixture(target: Path) -> None:
    for rel in OWNED_FILES:
        destination = target / rel
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / rel, destination)


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp_name:
        fixture = Path(tmp_name)
        copy_fixture(fixture)
        assert not check_root(fixture)

        rules = fixture / RULES
        pristine_rules = rules.read_text(encoding="utf-8")
        rules.write_text(
            pristine_rules + "\nEXTERN _spectrum_gui_redraw_square\n",
            encoding="utf-8",
        )
        assert any("rules primitive" in error for error in check_root(fixture))
        rules.write_text(pristine_rules, encoding="utf-8")

        input_kernel = fixture / INPUT
        pristine_input = input_kernel.read_text(encoding="utf-8")
        input_kernel.write_text(
            pristine_input + "\ncall _spectrum_gui_edit_tick\n", encoding="utf-8"
        )
        assert any("platform input kernel" in error for error in check_root(fixture))
        input_kernel.write_text(pristine_input, encoding="utf-8")

        screen = fixture / SCREEN
        screen.write_text(screen.read_text(encoding="utf-8") +
                          "\nEXTERN _side_to_move\n", encoding="utf-8")
        assert any("render source" in error for error in check_root(fixture))

        state = fixture / SETUP_STATE
        state.write_text(
            state.read_text(encoding="utf-8").replace(
                "_setup_time_focus = _app_workspace + 35",
                "_setup_time_focus = _app_workspace + 36",
            ),
            encoding="utf-8",
        )
        assert any("_setup_time_focus at +36" in error for error in check_root(fixture))

        header = (ROOT / SETUP_HEADER).read_text(encoding="utf-8")
        state_text = (ROOT / SETUP_STATE).read_text(encoding="utf-8")
        for declaration, expected in (
            ("uint16_t config_dirty;", "workspace size 55 for a 56-byte view"),
            ("uint8_t config_dirty;\n    uint8_t extra;", "view fields"),
            ("unknown_t config_dirty;", "unsupported field type"),
        ):
            changed = header.replace("uint8_t config_dirty;", declaration)
            assert changed != header
            assert any(expected in error for error in _setup_arena(changed, state_text))

    print("UX ASM boundary guard positive/negative tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
