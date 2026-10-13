"""Negative tests for UX dependency direction."""
import importlib.util
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("ux", ROOT / "tools/check_ux_boundaries.py")
ux = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ux)

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    view = root / "src/spectrum/ui/board_interaction.c"
    view.parent.mkdir(parents=True)
    view.write_text("spectrum_gui_redraw_square(0, 0);")
    assert not ux.violations(root)
    view.write_text("spectrum_board_apply_trusted_move(move);")
    assert ux.violations(root)
    view.write_text("spectrum_key_poll();")
    assert ux.violations(root)
    view.write_text("")
    app = root / "src/spectrum/app/app.c"
    app.parent.mkdir(parents=True)
    app.write_text("selected_row = 6u;")
    assert ux.violations(root)
    app.write_text("")
    for private_name in ("cell_cursor", "selected_position"):
        app.write_text(f"extern unsigned short {private_name};")
        assert ux.violations(root)
    app.write_text("")
    projection = root / "src/spectrum/ui/chess_move_view.inc"
    projection.write_text("spectrum_board_apply_trusted_move(move);")
    assert ux.violations(root)
    projection.write_text("")
    setup = root / "src/spectrum/ui/setup.c"
    setup.write_text("netchesszx_board_theme_apply(1);")
    assert not ux.violations(root)
    setup.write_text("netchesszx_session_set_id(1);")
    assert ux.violations(root)
    setup.write_text("")
    app.write_text("if (spectrum_gui_are_board_pieces_visible()) {}")
    assert not ux.violations(root)
    for statement in ("spectrum_gui_board_pieces_visible = 0;",
                      "++spectrum_gui_board_pieces_visible;",
                      "uint8_t *p = &spectrum_gui_board_pieces_visible;"):
        app.write_text(statement)
        assert ux.violations(root)
    app.write_text("")
    asm = root / "asm/ula/board_coords.inc"
    asm.parent.mkdir(parents=True)
    asm.write_text("EXTERN _spectrum_gui_board_pieces_visible\nld a, (_spectrum_gui_board_pieces_visible)")
    assert not ux.violations(root)
    for statement in ("ld (_spectrum_gui_board_pieces_visible), a",
                      "ld hl, _spectrum_gui_board_pieces_visible"):
        asm.write_text(statement)
        assert ux.violations(root)
storage = ux.VISIBILITY_STORAGE
reader = "src/spectrum/overlay/gui_log_ovl.c"
declaration = f"extern\n uint8_t  {storage} ;\n"
for source in (f"if ({storage})\n{{}}", f"else if ( ! {storage} ) {{}}",
               f"return {storage};", f"return mask & {storage};",
               f"#define VISIBLE ({storage} != 0)\n"):
    assert not ux.visibility_accesses(reader, declaration + source), source
for source in (f"{storage} = 0;", f"({storage})++;", f"-- ( {storage} );",
               f"{storage} |= 1;", f"memset( &({storage}), 0, 1);",
               f"p = (uint8_t *)&{storage};", f"#define ALIAS ({storage})\n"):
    assert ux.visibility_accesses(reader, declaration + source), source
assert not ux.visibility_accesses(ux.VISIBILITY_OWNER, f"uint8_t {storage};")
assert not ux.visibility_accesses(ux.VISIBILITY_OWNER, f"uint8_t\n {storage} = 0u;")
for type_ in ("uint16_t", "const uint8_t", "int"):
    assert ux.visibility_accesses(ux.VISIBILITY_OWNER, f"{type_} {storage};")
    assert ux.visibility_accesses(reader, f"extern {type_} {storage};\nif ({storage}) {{}}")
assert ux.visibility_accesses("asm/ula/board_coords.inc",
                              f"ex af, af'\nld (_{storage}), a\nex af, af'")
print("UX negative boundary tests passed")
