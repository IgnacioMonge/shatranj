#!/usr/bin/env python3
"""Guard dependency direction; the compiler enforces interaction state privacy."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
DENIED = re.compile(
    r"\b(?:netchesszx_session_\w*|spectrum_board_\w*|"
    r"spectrum_net_\w*|netchess\w*_(?:legal|apply|session)\w*|"
    r"send_local_move|local_turn|rules_current_legal)\b"
)
VISIBILITY_STORAGE = "spectrum_gui_board_pieces_visible"
VISIBILITY_OWNER = "src/spectrum/ui/gui.c"
VISIBILITY_READERS = {("src/spectrum/overlay/gui_log_ovl.c", VISIBILITY_STORAGE)}


def visibility_accesses(rel: str, text: str) -> list[str]:
    """Guard the byte ABI and scalar reads without depending on line layout."""
    storage = VISIBILITY_STORAGE
    code = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  " ", text, flags=re.S)
    if rel == VISIBILITY_OWNER:
        return ([] if re.search(rf"(?m)^\s*uint8_t\s+{storage}\s*(?:=[^;]+)?;", code)
                else [f"{rel}: GUI visibility definition must remain uint8_t"])
    if rel.startswith("asm/"):
        errors = []
        for line in text.splitlines():
            line = line.split(";", 1)[0]
            if re.search(rf"\b_?{storage}\b", line) and not re.fullmatch(
                    rf"\s*(?:EXTERN\s+_{storage}|ld\s+a,\s*\(_{storage}\))\s*", line, re.I):
                errors.append(f"{rel}: private GUI visibility; use getter/setter")
        return errors
    if not re.search(rf"\b{storage}\b", code):
        return []
    if (rel, storage) not in VISIBILITY_READERS:
        return [f"{rel}: private GUI visibility; use getter/setter"]
    # Both ends retain the uint8_t ABI used by the ASM readers. A mismatched
    # hand-written declaration fails even though it is in a different C unit.
    code, declarations = re.subn(rf"\bextern\s+uint8_t\s+{storage}\s*;", " ", code)
    if declarations != 1:
        return [f"{rel}: GUI visibility reader must declare extern uint8_t"]
    for match in re.finditer(rf"\b{storage}\b", code):
        prefix, tail = code[:match.start()], code[match.end():]
        write = re.match(r"\s*\)*\s*(?:=(?!=)|(?:<<|>>|[+*/%&|^\-])=|\+\+|--)", tail)
        increment = re.search(r"(?:\+\+|--)\s*\(*\s*$", prefix)
        address = re.search(r"(?:^|[=,(?:!~+*/%|^{};]|\breturn|\([^()]*\*\s*\))\s*&\s*\(*\s*$", prefix)
        alias = (re.search(r"#\s*define\s+\w+(?:\([^)]*\))?\s+\(*\s*$", prefix) and
                 re.match(r"\s*\)*\s*(?:\n|$)", tail))
        if write or increment or address or alias:
            return [f"{rel}: GUI visibility reader cannot write or borrow its address"]
    return []


def violations(root: Path) -> list[str]:
    errors = []
    for base in (root / "src", root / "asm"):
        for path in base.rglob("*"):
            if path.suffix not in {".c", ".h", ".asm", ".inc"}:
                continue
            rel = path.relative_to(root).as_posix()
            errors.extend(visibility_accesses(rel, path.read_text(encoding="utf-8")))
    owners = [root / "src/spectrum/ui/gui.c",
              root / "src/spectrum/ui/chess_move_view.inc",
              root / "src/spectrum/ui/board_interaction.c",
              root / "src/spectrum/ui/chess_navigation.c",
              root / "src/spectrum/ui/cell_cursor.c",
              root / "src/spectrum/ui/setup.c",
              root / "src/spectrum/overlay/input_edit_ovl.c"]
    owners += list((root / "src/pc/client").glob("*widget.*"))
    for path in owners:
        if not path.exists():
            continue
        text = re.sub(r"/\*.*?\*/|//[^\n]*", "", path.read_text(), flags=re.S)
        for match in DENIED.finditer(text):
            # Palette application is a render operation, not board/domain mutation.
            if match[0] == "netchesszx_board_theme_apply":
                continue
            errors.append(f"{path.relative_to(root)}: domain access {match[0]}")
        if re.search(r"\bspectrum_(?:gui_poll_key|key_poll|input_poll_event)\b", text):
            errors.append(f"{path.relative_to(root)}: physical polling belongs to platform/application")
        # Coordinate formatting is a value helper, not rules or board access.
        if re.search(r'#\s*include\s*"(?:common/(?:chess/(?!move_coords\.h)|(?:session|protocol)/)|spectrum/(?:board|session|transport)/)', text):
            errors.append(f"{path.relative_to(root)}: domain include")
    app = root / "src/spectrum/app/app.c"
    if app.exists() and re.search(r'\b(?:cursor_row|cursor_col|selected_row|selected_col|cell_cursor|selected_position)\b|#\s*include[^\n]*(?:board_interaction|cell_cursor)\.(?:c|inc)', app.read_text()):
        errors.append("app/app.c: private interaction state or implementation")
    return errors


if __name__ == "__main__":
    errors = violations(ROOT)
    for error in errors:
        print(error, file=sys.stderr)
    print("[FAIL] UX boundaries" if errors else "[OK] UX facts/intents and visual-state owners")
    sys.exit(bool(errors))
