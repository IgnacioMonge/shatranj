"""Execute real host glyphs, half-cell bitmap shift, cursor and clear on Z80."""
from pathlib import Path
import tempfile


from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)
from asm_data import parse_defb_with_offsets


def span(path, start, end):
    source = (ROOT / path).read_text()
    return start + source.split(start, 1)[1].split(end, 1)[0]


def pixel(y, x):
    return 0x4000 + ((y & 192) << 5) + ((y & 7) << 8) + ((y & 56) << 2) + x


def check_labels(assembler, emulator):
    cases = [(b"H" * size + b" (" + colour + b")", b"DEVICE", 0)
             for size in range(1, 20) for colour in (b"W", b"B")]
    cases += [(name, b"DEVICE", 0) for name in (
        b"", b"W", b" (W)", b"HOST", b"HOST (X)", b"HOST [W)",
        b"HOST (W]", b"HOSTX(W)", b"HOST (W) tail", b"A\x01\x1f\x7f\xffZ (B)")]
    cases += [(b"HOST (W)", b"", 0), (b"HOST (W)", b"DEVICE", 1)]
    calls, data, expected = [], [], []
    for i, (name, device, protected) in enumerate(cases):
        snapshot = bytes(4) + device.ljust(65, b"\0") + name.ljust(24, b"\0") + bytes((protected,))
        accepted = bool(device and not protected and len(name) >= 5 and
                        name[-4:] in (b" (W)", b" (B)"))
        label = bytes((0xa5,)) * 29
        if accepted:
            clean = bytes(c if 32 <= c <= 126 else ord("?") for c in name[:-4])
            label = b"\xa5" + clean.ljust(22, b" ") + (b"WHITE" if name[-2] == 87 else b"BLACK") + b"\0"
        expected.append((snapshot, label, accepted))
        data += [f"snapshot_{i}: defb " + ",".join(map(str, snapshot)),
                 f"label_{i}: defs 29, 165", f"guard_{i}: defb 90",
                 f"accepted_{i}: defb 255"]
        calls += [f"    ld hl, snapshot_{i}", "    push hl", f"    ld hl, label_{i}",
                  "    push hl", "    call _match_picker_label", "    ld a, l",
                  f"    ld (accepted_{i}), a"]
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "labels.asm"
        harness.write_text("""SECTION code_user
PUBLIC test_start, test_done, test_result
PUBLIC clear_right_panel_rows, clear_right_pixel_band_abs, draw_text64_line_attr_fast
PUBLIC draw_right_hline, compute_pixel_base, pixel_down_hl, draw_ikkle_inline
PUBLIC _spectrum_render_ikkle_abs_at, current_attr, _spectrum_gui_set_status
PUBLIC _spectrum_gui_notify_persistent
EXTERN _match_picker_label
test_start:
    ld sp, 0xff00
    ld ix, 0x1234
""" + "\n".join(calls) + """
    ld (saved_sp), sp
    ld (saved_ix), ix
    xor a
    ld (test_result), a
test_done: halt
test_result: defb 255
saved_sp: defw 0
saved_ix: defw 0
current_attr: defb 0
clear_right_panel_rows:
clear_right_pixel_band_abs:
draw_text64_line_attr_fast:
draw_right_hline:
compute_pixel_base:
pixel_down_hl:
draw_ikkle_inline:
_spectrum_render_ikkle_abs_at:
_spectrum_gui_set_status:
_spectrum_gui_notify_persistent: ret
""" + "\n".join(data))
        binary = temp / "labels.bin"
        mapping = assemble_vector(assembler, harness,
            [ROOT / "asm/overlay/direct/match_picker.asm"], binary)
        result, output, *_ = execute_vector(emulator, binary, mapping, counter=2000000)
        assert result == 0, output
        ram = binary.with_suffix(".ram").read_bytes()
        for i, (snapshot, label, accepted) in enumerate(expected):
            at = symbol_address(mapping, f"snapshot_{i}")
            assert ram[at:at + 94] == snapshot, f"snapshot unchanged {i}"
            at = symbol_address(mapping, f"label_{i}")
            assert ram[at:at + 29] == label, f"label bytes {i}"
            assert ram[symbol_address(mapping, f"guard_{i}")] == 90, f"label bound {i}"
            assert bool(ram[symbol_address(mapping, f"accepted_{i}")]) == accepted, f"host filter {i}"
        for label, value in (("saved_sp", 0xff00), ("saved_ix", 0x1234)):
            at = symbol_address(mapping, label)
            assert int.from_bytes(ram[at:at + 2], "little") == value, f"callee preserves {label}"
    print(f"MATCH: real ASM formatter passed {len(cases)} labels, filtering, bounds and callee ABI")


def main():
    assembler = required_tool("z88dk-z80asm")
    emulator = required_tool("z88dk-ticks")
    check_labels(assembler, emulator)
    helpers = span("asm/ula/address.inc", "compute_screen_base:", "ula_screen_board_top_m1 EQU")
    helpers += span("asm/ula/panel_primitives.inc", "draw_right_hline:", "draw_right_hline_full_left:")
    helpers += span("asm/ula/panel_primitives.inc", "clear_right_text_pixels:", "align_notice_text:")
    helpers += span("asm/ula/panel_primitives.inc", "align_notice_text:", "font_scanline:")
    helpers += span("asm/ula/panel_entries.inc", "draw_ikkle_inline:", "PUBLIC _spectrum_render_ikkle_at")
    helpers += span("asm/ula/panel_entries.inc", "render_ikkle_text_at:", "PUBLIC _spectrum_render_fileui_select")
    helpers += span("asm/ula/panel_entries.inc", "clear_right_panel_rows:", "_spectrum_info_clear_tail:")
    helpers += span("asm/ula/panel_primitives.inc", "font_scanline:", "clear_text_row:")
    helpers += span("asm/ula/text.inc", "draw_text64_line_attr_fast:", "draw_char64_pixels_at_tmp:")
    helpers += "screen_at_tmp:" + (ROOT / "asm/ula/text.inc").read_text().split("screen_at_tmp:", 1)[1]
    helpers += span("asm/ula/panel_entries.inc", "attr_cell_for_tmpcol:", "_spectrum_info_show_preflight:")
    assets = span("assets/spectrum/ui_runtime_assets.asm", "font_lut:", "badge_pattern:")
    data, offsets = parse_defb_with_offsets(assets)
    for row in range(10):
        hosts = ([f"HOST{index}".ljust(22) + ("WHITE" if index % 2 == 0 else "BLACK")
                  for index in range(9)] if row < 9 else ["NO HOSTS"])
        host_data = "\n".join(f'host_row_{index}: defb {index}, "{text}", 0'
                              for index, text in enumerate(hosts))
        draw_rows = "\n".join(f"    ld hl, host_row_{index}\n    call _match_picker_row"
                              for index in range(len(hosts)))
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            harness = temp / "picker.asm"
            harness.write_text(f"""SECTION code_user
PUBLIC test_start, test_done, test_result
PUBLIC compute_screen_base, compute_pixel_base, clear_right_pixel_band_abs, current_attr, pixel_down_hl
PUBLIC clear_right_panel_rows, draw_text64_line_attr_fast, draw_right_hline
PUBLIC draw_ikkle_inline, _spectrum_render_ikkle_abs_at
PUBLIC _spectrum_gui_set_status, _spectrum_render_notice, _spectrum_gui_notify_persistent
EXTERN _match_picker_frame, _match_picker_clear, _match_picker_cursor, _match_picker_row
EXTERN _match_picker_finish
INCLUDE "ui_layout.inc"
test_start:
    ld sp, 0xff00
    ld hl, 0x4000
    ld de, 0x4001
    ld bc, 6911
    ld (hl), 0xa5
    ldir
    call _match_picker_frame
    call _match_picker_clear
{draw_rows}
    call _match_picker_finish
    ld hl, 0x4000
    ld de, 0x6000
    ld bc, 6912
    ldir
    call clear_right_panel_rows
    ld hl, 0x4000
    ld de, 0xe000
    ld bc, 6912
    ldir
    ld hl, 0x4000
    ld de, 0x4001
    ld bc, 6911
    ld (hl), 0xa5
    ldir
    ld l, {row}
    call _match_picker_cursor
    ld hl, 0x4000
    ld de, 0xa000
    ld bc, 6912
    ldir
    ld l, {row}
    call _match_picker_cursor
    ld hl, 0x4000
    ld de, 0xc000
    ld bc, 6912
    ldir
    ld hl, connect_notice
    call _spectrum_gui_notify_persistent
    call _match_picker_clear
    xor a
    ld (test_result), a
test_done: halt
test_result: defb 255
current_attr: defb 0
tmp_scan: defb 0
tmp_row: defb 0
tmp_col: defb 0
piece_scan: defb 0
piece_row: defb 0
piece_col: defb 0
render_skip_clear: defb 0
notice_clears: defb 0
notice_shown: defb 0
notice_active: defb 255
connect_notice: defm "CONNECTING", 0
tmp_char: defb 0
tmp_attr: defb 0
text_ptr: defw 0
sprite_ptr: defw 0
{host_data}
_spectrum_gui_set_status: ret
; The GUI API copies to writable notice storage; the raw renderer edits HL.
_spectrum_gui_notify_persistent:
    ld a, (hl)
    ld (notice_active), a
    push hl
    ld hl, notice_clears
    or a
    jr z, notice_track
    inc hl
notice_track:
    inc (hl)
    pop hl
    ld de, 0x9000
notice_copy:
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    or a
    jr nz, notice_copy
    ld hl, 0x9000
_spectrum_render_notice:
    jp align_notice_text
{helpers}
{assets}
""")
            binary = temp / "picker.bin"
            mapping = assemble_vector(assembler, harness,
                [ROOT / "asm/overlay/direct/match_picker.asm"], binary)
            result, output, *_ = execute_vector(emulator, binary, mapping, counter=2000000)
            assert result == 0, output
            ram = binary.with_suffix(".ram").read_bytes()
            assert 37 + len(b"ENTER/SPACE:JOIN BREAK:BACK") <= 64, "footer fits panel"
            drawn = bytearray([0xA5]) * 6912
            # Execute the real frame and font writers, including footer attributes.
            for y in range(32, 168):
                for x in range(18, 32):
                    drawn[pixel(y, x) - 0x4000] = 0
            for attr_row in range(4, 21):
                attr = 0x1800 + attr_row * 32 + 18
                drawn[attr:attr + 14] = bytes([0x07]) * 14
            assert ram[0xE000:0xFB00] == drawn, "Setup/game panel clear removes every picker pixel and attribute"
            for y in (40, 148):
                drawn[pixel(y, 18) - 0x4000] = 0x0F
                for x in range(19, 32):
                    drawn[pixel(y, x) - 0x4000] = 0xFF
            for y, col, text in (
                (34, 37, "HOST"), (34, 59, "PLAYS"),
                (152, 37, "SELECT HOST: UP/DOWN"),
                (160, 37, "ENTER/SPACE:JOIN BREAK:BACK"),
            ):
                for char_index, char in enumerate(text):
                    half_col = col + char_index
                    x = half_col // 2
                    for attr_row in {y // 8, (y + 3) // 8}:
                        drawn[0x1800 + attr_row * 32 + x] = 0x46
                    for scan in range(4):
                        packed = data[offsets["timer_ikkle_packed"] + (ord(char) - 32) * 2 + scan // 2]
                        nibble = packed & 15 if scan & 1 else packed >> 4
                        mask = 0x0F if half_col % 2 else 0xF0
                        bits = nibble if half_col % 2 else nibble << 4
                        address = pixel(y + scan, x) - 0x4000
                        drawn[address] = (drawn[address] & ~mask) | bits
            for index, text in enumerate(hosts):
                for char_index, char in enumerate(text):
                    for scan in range(6):
                        packed = data[offsets["font_packed"] + (ord(char) - 32) * 3 + scan // 2]
                        nibble = packed & 15 if scan & 1 else packed >> 4
                        glyph = data[offsets["font_lut"] + nibble]
                        x = (37 + char_index) // 2
                        bits = glyph & (15 if (37 + char_index) % 2 else 240)
                        drawn[pixel(53 + index * 8 + scan, x) - 0x4000] |= bits
            for index in range(len(hosts)):
                attr = 0x1800 + (7 + index) * 32 + 18
                drawn[attr:attr + 14] = bytes([0x47]) * 14
            drawn[0x1800 + 6 * 32 + 18:0x1800 + 6 * 32 + 32] = bytes([0x47]) * 14
            assert ram[0x6000:0x7B00] == drawn, f"real frame/list/footer bitmap and attributes row {row}"
            assert 160 + 3 < 21 * 8, "entire footer stays within the Setup/game clear area"
            assert 152 == (148 // 8 + 1) * 8, "footer starts at the first attribute row below separator"
            assert 123 < 148 < 152, "ninth cursor row, separator and footer pixels are disjoint"
            assert 123 // 8 < 148 // 8 < 152 // 8, "list, separator and footer attribute rows are disjoint"
            assert ram[0x6000 + 0x1800 + 18 * 32 + 18:0x6000 + 0x1800 + 19 * 32] == bytes([0x07]) * 14, "separator keeps frame attribute"
            notice = symbol_address(mapping, "notice_clears")
            assert ram[notice:notice + 3] == bytes((1, 1, ord("C"))), "only frame entry clears notices; list refresh preserves notices"
            notice_text = symbol_address(mapping, "connect_notice")
            assert ram[notice_text:notice_text + 11] == b"CONNECTING\0", "notice API preserves constant text"
            empty = symbol_address(mapping, "match_empty")
            assert ram[empty:empty + 28] == binary.read_bytes()[empty - 0x8000:empty - 0x8000 + 28].ljust(28, b"\0"), "frame must not write into its constant text/code"
            original = bytes([0xA5]) * 6912
            selected = bytearray(original)
            if row < 9:
                for y in range(52 + row * 8, 60 + row * 8):
                    for x in range(18, 32):
                        selected[pixel(y, x) - 0x4000] ^= 15 if x == 18 else 255
            assert ram[0xA000:0xBB00] == selected, f"cursor bounds row {row}"
            assert ram[0xC000:0xDB00] == original, f"XOR restores row {row}"
            cleared = bytearray(original)
            for y in range(52, 128):
                for x in range(18, 32):
                    cleared[pixel(y, x) - 0x4000] = 0
            assert ram[0x4000:0x5B00] == cleared, "clear touches only list and four source-padding scans"
    print("MATCH: real ikkle footer Y152/160 clears fully on exit, separator Y148 and distinct attributes; real 4x6 host glyphs shifted up4 across cell/third boundary; nine rows and NO HOSTS; 10 cursor cases, XOR restore, attributes and list-only clear passed")


if __name__ == "__main__":
    main()
