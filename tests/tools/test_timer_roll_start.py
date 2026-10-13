"""Execute production timer/menu Roller code and its resident controller."""

from pathlib import Path
from itertools import product
import re
import tempfile

from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)
from asm_data import parse_defb_with_offsets


def check_phase_steps(assembler, emulator, temp):
    """Execute the native GUI scheduler, including clobbering render calls."""
    gui = (ROOT / "src/spectrum/ui/gui.c").read_text()
    bodies = []
    for name in ("timer_roll_tick", "timer_roll_finish"):
        body = re.search(rf"static void {name}\(void\) __naked\s*"
                         r"\{\s*#asm\n(.*?)#endasm", gui, re.S)
        assert body, name
        bodies.append(f"_{name}:\n" + body[1])
    harness = temp / "phase.asm"
    harness.write_text('''SECTION code_user
test_start:
    ld sp,0xff00
    ld ix,0x1234
    ld iy,0x5c3a
    ld hl,(state_seed)
    ld (0x5fe8),hl
    ld a,(finish_seed)
    or a
    jr nz,force_finish
    call _timer_roll_tick
    jr phase_done
force_finish:
    call _timer_roll_finish
phase_done:
    ld (saved_ix),ix
    ld (saved_iy),iy
    ld (saved_sp),sp
    xor a
    ld (test_result),a
test_done:
    jp test_done
test_result: defb 255
state_seed: defs 2
finish_seed: defb 0
rearm_seed: defb 0
game_calls: defb 0
clock_calls: defb 0
game_seen: defb 255
clock_seen: defb 255
saved_ix: defw 0
saved_iy: defw 0
saved_sp: defw 0
''' + "\n".join(bodies) + "\n".join(
        f'''_render_{name}_only:
    ld hl,{kind}_calls
    inc (hl)
    ld a,({address})
    ld ({kind}_seen),a
    ld hl,rearm_seed
    or (hl)
    ld ({address}),a
    ld bc,0x9876
    ld de,0x5432
    ld hl,0xdead
    xor a
    ret
''' for name, kind, address in (("game_timer", "game", "0x5fe8"),
                                ("clock", "clock", "0x5fe9"))), encoding="ascii")
    binary = temp / "phase.bin"
    symbols = assemble_vector(assembler, harness, [], binary)
    addr = lambda name: symbol_address(symbols, name)
    original = binary.read_bytes()
    assert addr("_timer_roll_finish") - addr("_timer_roll_tick") == 34
    assert addr("_render_game_timer_only") - addr("_timer_roll_finish") == 19
    total = 0
    for finish, rearm, game, clock in product((0, 1), (0, 128),
                                            (0, 128, 129, 130, 131),
                                            (0, 128, 129, 130, 131)):
        image = bytearray(original)
        for name, data in (("state_seed", bytes([game, clock])),
                           ("finish_seed", bytes([finish])),
                           ("rearm_seed", bytes([rearm]))):
            offset = addr(name) - 0x8000
            image[offset:offset + len(data)] = data
        binary.write_bytes(image)
        result, _, pc, _, _ = execute_vector(emulator, binary, symbols)
        case = (finish, rearm, game, clock)
        assert result == 0 and pc == addr("test_done"), case
        ram = binary.with_suffix(".ram").read_bytes()
        for i, (kind, phase) in enumerate((("game", game), ("clock", clock))):
            called = bool(finish or phase)
            q = 4 if finish else (phase & 7) + 1
            assert ram[addr(f"{kind}_calls")] == called, case
            assert ram[addr(f"{kind}_seen")] & 7 == (q if called else 7), case
            expected = 0 if finish or not phase or q == 4 else phase + 1
            assert ram[0x5fe8 + i] == expected, case
        for name, value in (("saved_ix", 0x1234), ("saved_iy", 0x5c3a),
                            ("saved_sp", 0xff00)):
            assert int.from_bytes(ram[addr(name):addr(name) + 2], "little") == value, case
        total += 1
    print(f"Roller native phase step: {total} independent-state, forced-finish and ABI cases passed")


def check_menu_roll(assembler, emulator, temp):
    harness = temp / "menu.asm"
    harness.write_text('''SECTION code_user
test_start:
    ld sp,0xff00
    ld ix,0x1234
    ld iy,0x5c3a
    ld hl,font_seed
    ld de,timer_roll_font
    ld bc,128
    ldir
    ld hl,labels_seed
    ld de,taboption_labels
    ld bc,55
    ldir
    ld hl,cache_seed
    ld de,taboption_cache
    ld bc,5
    ldir
    ld hl,screen_seed
    ld de,0x4000
    ld bc,6912
    ldir
    ld hl,(spec_seed)
    call _netchesszx_asm_menu_roll
    ld (saved_ix),ix
    ld (saved_iy),iy
    ld (saved_sp),sp
    xor a
    ld (test_result),a
test_done:
    jp test_done
test_result: defb 255
saved_ix: defw 0
saved_iy: defw 0
saved_sp: defw 0
spec_seed: defw 0
labels_seed: defs 55
cache_seed: defs 5
font_seed: defs 128
screen_seed: defs 6912
_spectrum_gui_about_visible_state: defb 0
_spectrum_render_menu: ret
_netchesszx_setup_nav_key_alias: ret
_spectrum_frame_wait: ret
_spectrum_uart_background_pump: ret
INCLUDE "asm/spectrum/timer_roll.asm"
''', encoding="ascii")
    transitions = [
        (0, 1, True), (1, 2, True), (2, 0, True), (0, 2, False),
        (3, 4, True), (4, 5, True), (5, 6, True), (6, 3, True),
        (7, 8, True), (8, 9, True), (9, 10, True), (10, 7, True),
    ]
    labels = [b"NTWRK", b"DISCN", b"RESET", b"OFFER",
              b"TKBCK", b"DRAW ", b"RESGN", b"THEME", b"BOARD", b"ASSET",
              b"HINTS"]
    columns = [6, 6, 6, 12, 12, 12, 12, 23, 23, 23, 23]
    total = 0
    for next_target in (False, True):
        binary = temp / ("menu_next.bin" if next_target else "menu_classic.bin")
        symbols = assemble_vector(assembler, harness, [], binary,
            defines=["NETCHESSZX_NEXT_BANKING"] if next_target else [])
        addr = lambda name: symbol_address(symbols, name)
        original = binary.read_bytes()
        assets, offsets = parse_defb_with_offsets((ROOT / "assets" /
            ("next" if next_target else "spectrum") / "ui_runtime_assets.asm").read_text())
        font = assets[offsets["timer_ikkle_packed"]:offsets["timer_ikkle_packed"] + 128]

        def glyph(ch):
            if ch < 33 or ch >= 128:
                return [0] * 4
            if ch >= 96:
                ch -= 32
            pair = font[(ch - 32) * 2:(ch - 32) * 2 + 2]
            return [pair[0] >> 4, pair[0] & 15, pair[1] >> 4, pair[1] & 15]

        def put(screen, col, rows):
            dest = 0x4240 + col // 2 - 0x4000
            shift = 0 if col & 1 else 4
            mask = 15 << shift
            for row, nibble in enumerate(rows):
                pos = dest + row * 256
                screen[pos] = (screen[pos] & ~mask) | (nibble << shift)

        for old_id, new_id, down in transitions:
            for phase in range(1, 5):
                image = bytearray(original)
                def seed(label, data):
                    offset = addr(label) - 0x8000
                    image[offset:offset + len(data)] = data
                seed("font_seed", font)
                seed("labels_seed", b"".join(labels))
                seed("cache_seed", labels[old_id])
                spec = (new_id | (old_id << 4) | (phase << 8) |
                        (int(not down) << 14))
                seed("spec_seed", spec.to_bytes(2, "little"))
                screen = bytearray((i * 37 + 91) & 255 for i in range(6912))
                for index, ch in enumerate(labels[old_id]):
                    put(screen, columns[old_id] + index,
                        [row ^ 15 for row in glyph(ch)])
                expected_screen = bytearray(screen)
                for index, (old_ch, new_ch) in enumerate(zip(labels[old_id], labels[new_id])):
                    if down:
                        strip = glyph(old_ch) + glyph(new_ch)
                        start = phase
                    else:
                        strip = glyph(new_ch) + glyph(old_ch)
                        start = 4 - phase
                    put(expected_screen, columns[new_id] + index,
                        [row ^ 15 for row in strip[start:start + 4]])
                seed("screen_seed", screen)
                binary.write_bytes(image)
                result, _, pc, _, _ = execute_vector(
                    emulator, binary, symbols, counter=1000000)
                case = (next_target, old_id, new_id, down, phase)
                assert result == 0 and pc == addr("test_done"), case
                ram = binary.with_suffix(".ram").read_bytes()
                actual_screen = ram[0x4000:0x5b00]
                differences = [(0x4000 + i, expected, actual_screen[i])
                               for i, expected in enumerate(expected_screen)
                               if actual_screen[i] != expected]
                assert not differences, (case, differences[:12])
                expected_cache = labels[new_id] if phase == 4 else labels[old_id]
                assert ram[addr("taboption_cache"):addr("taboption_cache") + 5] == expected_cache, case
                for label, value in (("saved_ix", 0x1234), ("saved_iy", 0x5c3a),
                                     ("saved_sp", 0xff00)):
                    assert int.from_bytes(
                        ram[addr(label):addr(label) + 2], "little") == value, case
                total += 1
    print(f"Roller menu: {total} Classic/Next label, selection, phase and ABI cases passed")


def check_menu_controller(assembler, emulator, temp):
    """Exercise the product ASM state machine instead of the host C model."""
    harness = temp / "menu_controller.asm"
    harness.write_text('''SECTION code_user
test_start:
    ld sp,0xff00
    ld ix,0x1234
    ld iy,0x5c3a
    ld hl,font_seed
    ld de,timer_roll_font
    ld bc,128
    ldir
    ld hl,labels_seed
    ld de,taboption_labels
    ld bc,55
    ldir
    ld hl,transitions_seed
    ld de,taboption_transitions
    ld bc,11
    ldir
    ld hl,actions_seed
    ld de,taboption_actions
    ld bc,11
    ldir
    ld a,(visible_seed)
    ld (taboption_visible_addr),a
    ld a,(focus_seed)
    ld (taboption_focus_addr),a
    ld a,(item_seed)
    ld (taboption_item_addr),a
    ld a,(about_seed)
    ld (_spectrum_gui_about_visible_state),a
    ld a,(key_seed)
    ld l,a
    call _netchesszx_asm_menu_key
    ld a,l
    ld (returned),a
menu_done:
    ld a,(taboption_visible_addr)
    ld (visible_result),a
    ld a,(taboption_focus_addr)
    ld (focus_result),a
    ld a,(taboption_item_addr)
    ld (item_result),a
    ld (saved_ix),ix
    ld (saved_iy),iy
    ld (saved_sp),sp
    xor a
    ld (test_result),a
test_done:
    jp test_done

_spectrum_render_menu:
    ld a,l
    ld (render_last),a
    ld hl,render_calls
    inc (hl)
    ret

_netchesszx_setup_nav_key_alias:
    ld a,l
    ld hl,nav_aliases
    ld b,8
nav_alias_loop:
    cp (hl)
    inc hl
    jr z,nav_alias_found
    inc hl
    djnz nav_alias_loop
    ld l,a
    ret
nav_alias_found:
    ld l,(hl)
    ret
nav_aliases:
    DEFB '5',0x83,'o',0x83,'6',0x82,'a',0x82
    DEFB '7',0x81,'q',0x81,'8',0x84,'p',0x84

test_result: defb 255
visible_seed: defb 0
focus_seed: defb 0
item_seed: defb 0
about_seed: defb 0
key_seed: defb 0
returned: defb 255
visible_result: defb 255
focus_result: defb 255
item_result: defb 255
render_calls: defb 0
render_last: defb 255
saved_ix: defw 0
saved_iy: defw 0
saved_sp: defw 0
wait_calls: defb 0
pump_calls: defb 0
roll_spec: defb 255
roll_dir: defb 255
transitions_seed: defs 11
actions_seed: defs 11
labels_seed: defs 55
font_seed: defs 128
_spectrum_gui_about_visible_state: defb 0
_spectrum_frame_wait:
    ld a,(roll_spec)
    inc a
    jr nz,wait_already
    ld a,l
    ld (roll_spec),a
    ld a,h
    ld (roll_dir),a
wait_already:
    ld hl,wait_calls
    inc (hl)
    ret
_spectrum_uart_background_pump:
    ld hl,pump_calls
    inc (hl)
    ret
INCLUDE "asm/spectrum/timer_roll.asm"
''', encoding="ascii")

    labels = [b"NTWRK", b"DISCN", b"RESET", b"OFFER",
              b"TKBCK", b"DRAW ", b"RESGN", b"THEME", b"BOARD", b"ASSET",
              b"HINTS"]
    transitions = bytes((0x21, 0x22, 0x11, 0x64, 0x65,
                         0x46, 0x54, 0xa8, 0xa9, 0x8a, 0x98))
    actions = bytes((0, 0x8d, 0x8e, 0, 0x92,
                     0x93, 0x94, 0, 0x8f, 0x95, 0x96))
    cases = [
        # name, visible, focus, item, about, key, inter-phase waits,
        # return, visible, focus, item, render calls, render spec, cache label
        ("hidden passthrough", 0, 2, 3, 0, ord('x'), 0,
         ord('x'), 0, 2, 3, 0, 255, None),
        ("about owns menu key", 0, 0, 0, 1, 0x90, 0,
         0x90, 0, 0, 0, 0, 255, None),
        ("cold garbage focus clamps before open", 0, 0xff, 0xa5, 0, 0x90, 0,
         0, 1, 0, 0x91, 1, 1, None),
        ("open on NTWRK enters DISCN", 0, 1, 9, 0, 0x90, 3,
         0, 1, 1, 1, 1, 2, 1),
        ("close and redraw roots", 1, 1, 2, 0, 0x90, 0,
         0, 0, 1, 2, 1, 0, None),
        ("left wraps", 1, 0, 0x91, 0, 0x83, 0,
         0, 1, 5, 0x8c, 1, 0x85, None),
        ("right wraps", 1, 5, 0x8c, 0, ord('p'), 0,
         0, 1, 0, 0x91, 1, 0xa8, None),
        ("enter NTWRK rolls to DISCN", 1, 0, 0x91, 0, ord('8'), 3,
         0, 1, 1, 1, 1, 0x81, 1),
        ("enter OFFER rolls to TKBCK", 1, 3, 0x8b, 0, ord('5'), 3,
         0, 1, 2, 4, 1, 0x9a, 4),
        ("enter THEME rolls to BOARD", 1, 3, 0x8b, 0, ord('8'), 3,
         0, 1, 4, 8, 1, 0x9c, 8),
        ("leave child rolls back LINKS", 1, 1, 2, 0, ord('8'), 6,
         0, 1, 2, 4, 1, 0x8a, 4),
        ("leave child rolls back MATCH", 1, 2, 4, 0, ord('8'), 3,
         0, 1, 3, 0x8b, 1, 0x93, 3),
        ("leave child rolls back THEME", 1, 4, 8, 0, ord('8'), 3,
         0, 1, 5, 0x8c, 1, 0xa5, 7),
        ("LINK down", 1, 1, 1, 0, 0x82, 3,
         0, 1, 1, 2, 0, 255, 2),
        ("LINK up wraps", 1, 1, 1, 0, ord('q'), 3,
         0, 1, 1, 2, 0, 255, 2),
        ("GAME numeric down", 1, 2, 4, 0, ord('6'), 3,
         0, 1, 2, 5, 0, 255, 5),
        ("THEME numeric up", 1, 4, 8, 0, ord('7'), 3,
         0, 1, 4, 10, 0, 255, 10),
        ("THEME down wraps children", 1, 4, 10, 0, 0x82, 3,
         0, 1, 4, 8, 0, 255, 8),
        ("leave child rolls back HINTS", 1, 4, 10, 0, ord('8'), 3,
         0, 1, 5, 0x8c, 1, 0xa5, 7),
        ("LINK down wraps children", 1, 1, 2, 0, 0x82, 3,
         0, 1, 1, 1, 0, 255, 1),
        ("MATCH up wraps children", 1, 2, 4, 0, 0x81, 3,
         0, 1, 2, 6, 0, 255, 6),
        ("FILE action", 1, 0, 0x91, 0, 32, 0,
         0x91, 0, 0, 0x91, 1, 0, None),
        ("FLIP action", 1, 3, 0x8b, 0, 13, 0,
         0x8b, 0, 3, 0x8b, 1, 0, None),
        ("ABOUT action", 1, 5, 0x8c, 0, 13, 0,
         0x8c, 0, 5, 0x8c, 1, 0, None),
    ]
    for focus, item, action in ((1, 1, 0x8d), (1, 2, 0x8e),
                                (2, 4, 0x92), (2, 5, 0x93),
                                (2, 6, 0x94), (4, 8, 0x8f),
                                (4, 9, 0x95), (4, 10, 0x96)):
        cases.append((f"child action {item}", 1, focus, item, 0, 13, 0,
                      action, 0, focus, item, 1, 0, None))

    total = 0
    for next_target in (False, True):
        binary = temp / ("menu_controller_next.bin" if next_target else
                         "menu_controller_classic.bin")
        symbols = assemble_vector(assembler, harness, [], binary,
            defines=["NETCHESSZX_NEXT_BANKING"] if next_target else [])
        addr = lambda name: symbol_address(symbols, name)
        original = binary.read_bytes()
        assets, offsets = parse_defb_with_offsets((ROOT / "assets" /
            ("next" if next_target else "spectrum") / "ui_runtime_assets.asm").read_text())
        font = assets[offsets["timer_ikkle_packed"]:offsets["timer_ikkle_packed"] + 128]

        for case in cases:
            (name, visible, focus, item, about, key, expected_waits,
             expected_return, expected_visible, expected_focus, expected_item,
             expected_calls, expected_render, cache_label) = case
            image = bytearray(original)

            def seed(label, data):
                offset = addr(label) - 0x8000
                image[offset:offset + len(data)] = data

            for label, value in (("visible_seed", visible), ("focus_seed", focus),
                                 ("item_seed", item), ("about_seed", about),
                                 ("key_seed", key)):
                seed(label, bytes((value,)))
            seed("transitions_seed", transitions)
            seed("actions_seed", actions)
            seed("labels_seed", b"".join(labels))
            seed("font_seed", font)
            binary.write_bytes(image)
            result, _, pc, _, _ = execute_vector(
                emulator, binary, symbols, counter=1000000)
            ram = binary.with_suffix(".ram").read_bytes()
            actual = (ram[addr("returned")], ram[addr("visible_result")],
                      ram[addr("focus_result")], ram[addr("item_result")],
                      ram[addr("render_calls")], ram[addr("render_last")],
                      ram[addr("wait_calls")], ram[addr("pump_calls")])
            expected = (expected_return, expected_visible, expected_focus,
                        expected_item, expected_calls, expected_render,
                        expected_waits, expected_waits)
            assert result == 0 and pc == addr("test_done"), (next_target, name)
            assert actual == expected, (next_target, name, actual, expected)
            if cache_label is not None:
                cache = 0x3f1d if next_target else 0x67d0
                assert ram[cache:cache + 5] == labels[cache_label], (next_target, name)
            leave_child_roll = {
                "leave child rolls back LINKS": 0x20,
                "leave child rolls back MATCH": 0x43,
                "leave child rolls back THEME": 0x87,
                "leave child rolls back HINTS": 0xa7,
            }
            # First roll of the key: old/new label ids, then phase 2 with
            # bit 6 set only for the reverse roll back to the group label.
            enter_roll = {
                "open on NTWRK enters DISCN": 0x01,
                "enter NTWRK rolls to DISCN": 0x01,
                "enter OFFER rolls to TKBCK": 0x34,
                "enter THEME rolls to BOARD": 0x78,
            }
            for rolls, direction in ((leave_child_roll, 0x42),
                                     (enter_roll, 0x02)):
                if name in rolls:
                    assert (ram[addr("roll_spec")], ram[addr("roll_dir")]) == (
                        rolls[name], direction), (
                        next_target, name, ram[addr("roll_spec")],
                        ram[addr("roll_dir")])
            for label, value in (("saved_ix", 0x1234), ("saved_iy", 0x5c3a),
                                 ("saved_sp", 0xff00)):
                assert int.from_bytes(
                    ram[addr(label):addr(label) + 2], "little") == value, (next_target, name)
            total += 1
    print(f"Roller menu controller: {total} Classic/Next navigation, action and ABI cases passed")


def main():
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    cases = [
        (0, b"GAME:00h00m TURN:00m00s", b"GAME:01h23m TURN:45h67m"),
        (0, b"GAME:00h00m TURN:00m00s", b" " * 23),
        (0, b" " * 23, b"GAME:00h00m TURN:00m00s"),
        (0, b"GAME:00h00m TURN:00m00s", b"GAME:00h00m TURN:00m00s"),
        (8, b"23:59", b"00:00"),
        (8, b"--:--", b"12:34"),
        (8, b"12:34", b"--:--"),
        (8, b"12:34", b"12:34"),
    ]
    total = 0
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        check_phase_steps(assembler, emulator, temp)
        check_menu_roll(assembler, emulator, temp)
        check_menu_controller(assembler, emulator, temp)
        harness = temp / "start.asm"
        harness.write_text('''SECTION code_user
test_start:
    ld sp,0xff00
    ld ix,0x1234
    ld iy,0x5c3a
    ld hl,font_seed
    ld de,timer_roll_font
    ld bc,128
    ldir
    ld hl,screen_seed
    ld de,0x4000
    ld bc,6912
    ldir
    ld hl,cache_seed
    ld de,(cache_address)
    ld bc,24
    ldir
    ld hl,(state_seed)
    ld (0x5fe8),hl
    ld hl,text_seed
    ld a,(kind_seed)
    or a
    jr nz,start_clock
    call _netchesszx_asm_timer_game
    jr start_paint
start_clock:
    call _netchesszx_asm_timer_clock
start_paint:
    ld (saved_ix),ix
    ld (saved_iy),iy
    ld (saved_sp),sp
    xor a
    ld (test_result),a
test_done:
    jp test_done
test_result: defb 255
saved_ix: defw 0
saved_iy: defw 0
saved_sp: defw 0
kind_seed: defb 0
state_seed: defs 2
cache_address: defw 0
cache_seed: defs 24
text_seed: defs 24
font_seed: defs 128
screen_seed: defs 6912
_spectrum_gui_about_visible_state: defb 0
_spectrum_render_menu: ret
_netchesszx_setup_nav_key_alias: ret
_spectrum_frame_wait: ret
_spectrum_uart_background_pump: ret
INCLUDE "asm/spectrum/timer_roll.asm"
''', encoding="ascii")
        for next_target in (False, True):
            binary = temp / ("next.bin" if next_target else "classic.bin")
            symbols = assemble_vector(assembler, harness, [], binary,
                defines=["NETCHESSZX_NEXT_BANKING"] if next_target else [])
            addr = lambda name: symbol_address(symbols, name)
            original = binary.read_bytes()
            assets, offsets = parse_defb_with_offsets((ROOT / "assets" /
                ("next" if next_target else "spectrum") / "ui_runtime_assets.asm").read_text())
            font = assets[offsets["timer_ikkle_packed"]:offsets["timer_ikkle_packed"] + 128]

            def glyph(ch):
                if ch < 33 or ch >= 128:
                    return [0] * 4
                if ch >= 96:
                    ch -= 32
                pair = font[(ch - 32) * 2:(ch - 32) * 2 + 2]
                return [pair[0] >> 4, pair[0] & 15, pair[1] >> 4, pair[1] & 15]

            for kind, old, new in cases:
                for phase in range(5):
                    image = bytearray(original)
                    def seed(label, data):
                        offset = addr(label) - 0x8000
                        image[offset:offset + len(data)] = data
                    cache = 0x5e92 if kind else 0x5e9a
                    seed("font_seed", font)
                    seed("cache_seed", old.ljust(24, b"\0"))
                    seed("text_seed", new.ljust(24, b"\0"))
                    seed("cache_address", cache.to_bytes(2, "little"))
                    seed("kind_seed", bytes([kind]))
                    state = bytearray([0x82, phase] if kind else [phase, 0x83])
                    seed("state_seed", state)
                    screen = bytearray((i * 37 + 91) & 255 for i in range(6912))
                    def put(index, rows):
                        col = (56 if kind else 41) + index
                        dest = (0x52c0 if kind else 0x4240) + col // 2 - 0x4000
                        shift = 0 if col & 1 else 4
                        mask = 15 << shift
                        for row, nibble in enumerate(rows):
                            pos = dest + row * 256
                            screen[pos] = (screen[pos] & ~mask) | (nibble << shift)
                    for index, ch in enumerate(old):
                        put(index, glyph(ch))
                    seed("screen_seed", screen)
                    jobs = [i for i, (a, b) in enumerate(zip(old, new)) if a != b]
                    expected_cache = bytearray(old.ljust(24, b"\0"))
                    for index in jobs:
                        strip = glyph(old[index]) + glyph(new[index])
                        put(index, strip[phase:phase + 4])
                        if phase == 4:
                            expected_cache[index] = new[index]
                    binary.write_bytes(image)
                    result, _, pc, _, _ = execute_vector(emulator, binary, symbols, counter=1000000)
                    case = (next_target, kind, old, new, phase)
                    assert result == 0 and pc == addr("test_done"), case
                    ram = binary.with_suffix(".ram").read_bytes()
                    assert ram[0x4000:0x5b00] == screen, case
                    assert ram[cache:cache + 24] == expected_cache, case
                    state[bool(kind)] = phase | (128 if jobs else 0)
                    assert ram[0x5fe8:0x5fea] == state, case
                    for label, value in (("saved_ix", 0x1234), ("saved_iy", 0x5c3a), ("saved_sp", 0xff00)):
                        assert int.from_bytes(ram[addr(label):addr(label) + 2], "little") == value, case
                    total += 1
    print(f"Roller UI boundary: {total} Classic/Next VRAM, attributes, delayed-cache and ABI cases passed")


if __name__ == "__main__":
    main()
