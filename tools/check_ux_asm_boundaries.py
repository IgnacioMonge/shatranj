#!/usr/bin/env python3
"""Guard Spectrum UX assembly ownership without changing linked contracts."""

from __future__ import annotations

import re
from pathlib import Path


SCREEN = Path("asm/spectrum/screen.asm")
RULES = Path("asm/overlay/rules/rules_stub.asm")
INPUT = Path("asm/platform/input_scan.inc")
INPUT_FILES = tuple(Path(f"asm/platform/input_{part}.inc")
                    for part in ("scan", "tick", "queue", "state", "state_extern"))
BOARD_VIEW = Path("asm/spectrum/board_hints_view.inc")
BOARD_NEXT_VIEW = Path("asm/next/board_hints_next_view.inc")
RULES_VIEW = Path("asm/spectrum/rules_hints_view.inc")
SETUP_HEADER = Path("src/spectrum/ui/setup.h")
SETUP_STATE = Path("asm/spectrum/setup_state.inc")

ROOT = Path(__file__).resolve().parents[1]
SCREEN_FILES = (SCREEN, Path("asm/next/screen.asm"),
                *(p.relative_to(ROOT) for pattern in ("asm/ula/*.inc", "asm/spectrum/piece_*.inc", "asm/spectrum/board_*.inc", "asm/next/board_*.inc")
                  for p in sorted(ROOT.glob(pattern)) if "hints" not in p.name))

OWNED_FILES = (
    *SCREEN_FILES,
    Path("asm/next/board_hints_view.inc"),
    RULES,
    *INPUT_FILES,
    BOARD_VIEW,
    BOARD_NEXT_VIEW,
    RULES_VIEW,
    SETUP_HEADER,
    SETUP_STATE,
)


def _forbid(
    errors: list[str], path: Path, text: str, tokens: tuple[str, ...], owner: str
) -> None:
    for token in tokens:
        if token in text:
            errors.append(f"{path}: {owner} must not contain {token!r}")


def _setup_arena(header: str, state: str) -> list[str]:
    """Compare the ASM arena aliases with the C view layout they reinterpret.

    Both sides are derived from their own source, so neither restates the other.
    ASM aliases must follow C field order; comparison is positional even where
    the historical ASM and C names differ. The target view has byte alignment.
    """
    errors: list[str] = []
    defines = {name: value for name, value in
               re.findall(r"^#define\s+(\w+)\s+(\w+)\s*$", header, re.M)}
    body = re.search(r"typedef struct \{(.*?)\} spectrum_setup_view_t;", header, re.S)
    if body is None:
        return [f"{SETUP_HEADER}: spectrum_setup_view_t definition not found"]

    def count(expression: str) -> int:
        for name, value in defines.items():
            expression = expression.replace(name, value)
        expression = expression.replace("u", "")
        if not re.fullmatch(r"[\d+\s]+", expression):
            errors.append(f"{SETUP_HEADER}: unsupported array size {expression!r}")
            return 1
        return sum(int(part) for part in expression.split("+"))

    widths = {"uint8_t": 1, "int8_t": 1, "char": 1, "uint16_t": 2, "int16_t": 2}
    fields: list[tuple[str, int]] = []
    offset = 0
    for kind, name, extent in re.findall(
        r"^\s*(\w+)\s+(\w+)(?:\[([^\]]+)\])?;\s*$", body.group(1), re.M
    ):
        if kind not in widths:
            return errors + [f"{SETUP_HEADER}: unsupported field type {kind!r}"]
        fields.append((name, offset))
        offset += widths[kind] * (count(extent) if extent else 1)

    aliases = [(name, int(value or 0)) for name, value in re.findall(
        r"^\s*defc\s+(_setup_\w+)\s*=\s*_app_workspace(?:\s*\+\s*(\d+))?\s*$",
        state, re.M)]
    if len(aliases) != len(fields):
        errors.append(f"{SETUP_STATE}: {len(aliases)} arena aliases for "
                      f"{len(fields)} view fields")
        return errors
    for (alias, actual), (field, expected) in zip(aliases, fields):
        if actual != expected:
            errors.append(f"{SETUP_STATE}: {alias} at +{actual}; "
                          f"{field} is at +{expected}")
    size = defines.get("SPECTRUM_SETUP_WORKSPACE_SIZE", "").rstrip("u")
    if size.isdigit() and int(size) != offset:
        errors.append(f"{SETUP_HEADER}: workspace size {size} for a {offset}-byte view")
    if "defc _spectrum_setup_view = _app_workspace" not in state:
        errors.append(f"{SETUP_STATE}: the C view must alias the arena base")
    return errors


def check_root(root: Path) -> list[str]:
    errors: list[str] = []
    texts: dict[Path, str] = {}
    for rel in OWNED_FILES:
        path = root / rel
        if not path.is_file():
            errors.append(f"{rel}: missing UX assembly owner")
            continue
        texts[rel] = path.read_text(encoding="utf-8")
    if errors:
        return errors

    screen = "\n".join(texts[path] for path in SCREEN_FILES)
    rules = texts[RULES]
    input_kernel = "\n".join(texts[path] for path in INPUT_FILES)
    board_view = texts[BOARD_VIEW] + texts[Path("asm/next/board_hints_view.inc")] + texts[BOARD_NEXT_VIEW]
    rules_view = texts[RULES_VIEW]

    _forbid(
        errors,
        SCREEN,
        screen,
        (
            "EXTERN _side_to_move",
            "EXTERN _castle_rights",
            "EXTERN _ep_square",
            "SPECTRUM_OVL_HINTS EQU",
            "_spectrum_board_show_legal_hints:",
            "_spectrum_board_clear_legal_hints:",
            "next_draw_legal_hints:",
            "_spectrum_key_edit_pressed:",
            "spectrum_key_scan_raw:",
            "skp_repeatable:",
        ),
        "render source",
    )

    _forbid(
        errors,
        RULES,
        rules,
        (
            "_rules_hints_ovl:",
            "_rules_hints_clear_ovl:",
            "draw_square_hint:",
            "_spectrum_board_view_",
            "_spectrum_gui_",
            "_netchesszx_board_theme_index",
            "_netchesszx_hinted_rows",
            "compute_attr_base",
            "compute_screen_base",
            "set_square_attr_2x2",
        ),
        "rules primitive",
    )

    _forbid(
        errors,
        RULES_VIEW,
        rules_view,
        (
            "rules_current_legal",
            "rules_import_board",
            "_netchesszx_hinted_rows",
            "_spectrum_board_view_",
            "_netchesszx_board_theme_index",
        ),
        "hint view",
    )
    _forbid(
        errors,
        BOARD_VIEW,
        board_view,
        ("rules_current_legal", "rules_import_board", "SPECTRUM_OVL_HINTS"),
        "resident hint view",
    )

    _forbid(
        errors,
        INPUT,
        input_kernel,
        ("_spectrum_gui_edit_tick", "spectrum_render_", "rules_current_legal"),
        "platform input kernel",
    )
    errors += _setup_arena(texts[SETUP_HEADER], texts[SETUP_STATE])
    return errors


def main() -> int:
    errors = check_root(Path(__file__).resolve().parents[1])
    for error in errors:
        print(f"[ERR] {error}")
    if errors:
        return 1
    print("UX ASM boundaries OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
