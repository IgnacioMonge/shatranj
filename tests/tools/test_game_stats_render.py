#!/usr/bin/env python3
"""Focused ABOUT statistics framebuffer, asset fidelity and capacity vector."""
from pathlib import Path
import sys
import tempfile

from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)

sys.path.insert(0, str(ROOT / "tools"))
from make_game_stats_mockup import Screen, draw_context, build_mockup
from check_lowmem_layout import parse_const


def ula_offset(y):
    return ((y & 0xc0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)


HARNESS = r'''
SECTION code_user
PUBLIC test_start, test_done, test_result
PUBLIC _spectrum_game_stats, _netchesszx_session_state
PUBLIC _spectrum_journal_pend, _spectrum_journal_warning
PUBLIC _netchesszx_local_color, _netchesszx_board_light_attr
PUBLIC _spectrum_render_fileui_frame, _spectrum_render_ikkle_at
PUBLIC _spectrum_render_ikkle_abs_at
PUBLIC _spectrum_frame_wait, compute_screen_base, piece_sprites_16x16, pixel_down_hl
PUBLIC ovl_close_overlay_file, ovl_read_chunked
PUBLIC _esx_fopen, _esx_fclose, _esx_handle, _spxn_xfs_fseek, _spxn_dat_bind
EXTERN _game_stats_render_ovl_entry
INCLUDE "ui_layout.inc"
INCLUDE "asm/app/player_name_layout.inc"
_spectrum_game_stats EQU 0x5fe0
IFDEF NETCHESSZX_NEXT_BANKING
asset_base EQU 0x3500
ELSE
asset_base EQU 0x6001
ENDIF
piece_sprites_16x16 EQU asset_base+889
timer_ikkle_packed EQU asset_base+38
test_start:
 ld sp,0xff00
 ld hl,screen_seed
 ld de,0x4000
 ld bc,6912
 ldir
 ld hl,assets_seed
 ld de,asset_base
 ld bc,1273
 ldir
 ld hl,stats_seed
 ld de,_spectrum_game_stats
 ld bc,33
 ldir
 ld hl,result_text
 ld (_spectrum_game_stats+19),hl
 ld hl,names_seed
 ld de,PLAYER_NAME
 ld bc,22
 ldir
 ; Poison the overlay scratch: the page must zero or write all it reads.
 ld hl,OVERLAY_SCRATCH
 ld de,OVERLAY_SCRATCH+1
 ld bc,159
 ld (hl),0xa5
 ldir
 ld ix,0x1234
 ld iy,0x5c3a
 call _game_stats_render_ovl_entry
 ld (returned),hl
 ld (saved_ix),ix
 ld (saved_iy),iy
 xor a
 ld (test_result),a
test_done: jp test_done
_spectrum_frame_wait:
 ld a,(waits)
 inc a
 ld (waits),a
 ret
_spectrum_render_fileui_frame:
 ld a,5
frame_row:
 push af
 call compute_screen_base
 inc l
 ld c,8
frame_scan:
 push hl
 ld b,16
frame_col:
 ld (hl),0
 inc hl
 djnz frame_col
 pop hl
 inc h
 dec c
 jr nz,frame_scan
 pop af
 inc a
 cp 21
 jr c,frame_row
 ld a,8
 call compute_screen_base
 inc l
 ld b,16
frame_separator:
 ld (hl),255
 inc hl
 djnz frame_separator
 ret
_spectrum_render_ikkle_at:
 ld a,(hl)
 ld (tmp_row),a
 inc hl
 ld a,(hl)
 ld (tmp_col),a
 inc hl
 ld a,(hl)
 ld (current_attr),a
 inc hl
 ld a,2
 ld (tmp_scan),a
 jp draw_ikkle_text_at
_spectrum_render_ikkle_abs_at:
 ld a,(hl)
 ld b,a
 inc hl
 ld a,(hl)
 ld c,a
 inc hl
 ld a,(hl)
 ld (current_attr),a
 inc hl
 jp draw_ikkle_text_abs_y
attr_cell_for_tmpcol:
 call compute_attr_base
 ld a,(tmp_col)
 srl a
 add a,l
 ld l,a
 ret
font_scanline:
 xor a
 ret
; Same as asm/ula/panel_primitives.inc: 6-pixel rows cross char rows.
pixel_down_hl:
 inc h
 ld a,h
 and 7
 ret nz
 ld a,l
 add a,32
 ld l,a
 ret c
 ld a,h
 sub 8
 ld h,a
 ret
ovl_close_overlay_file:
ovl_read_chunked:
_esx_fopen:
_esx_fclose:
_spxn_xfs_fseek:
_spxn_dat_bind:
 ret
INCLUDE "asm/ula/address.inc"
INCLUDE "asm/ula/text.inc"
tmp_row: defb 0
tmp_col: defb 0
tmp_scan: defb 0
tmp_char: defb 0
current_attr: defb 0
piece_scan: defb 0
piece_row: defb 0
piece_col: defb 0
tmp_attr: defb 0
render_skip_clear: defb 0
sprite_ptr: defw 0
text_ptr: defw 0
_netchesszx_session_state: defw 61
_spectrum_journal_pend: defb PENDING
_spectrum_journal_warning: defb WARNING
_netchesszx_local_color: defb 0
_netchesszx_board_light_attr: defb 0x38
_esx_handle: defb 0
returned: defw 0
saved_ix: defw 0
saved_iy: defw 0
waits: defb 0
test_result: defb 255
names_seed: NAMES_SEED
result_text: defb RESULT_TEXT,0
stats_seed: BINARY "stats.bin"
screen_seed: BINARY "screen.bin"
assets_seed: BINARY "assets.bin"
'''


def name_field(name):
    return '"' + name + '"' + ",0" * (11 - len(name))


def run_case(tmp, defines, kings=False, pending=0, warning=0, missing=False, saturated=False,
             names=("NACHO", "PEPE")):
    screen = Screen(0, 0)
    draw_context(screen)
    seed = bytearray(6912)
    for y in range(192):
        seed[ula_offset(y):ula_offset(y)+32] = screen.pixels[y*32:(y+1)*32]
    seed[6144:] = screen.attrs
    (tmp / "screen.bin").write_bytes(seed)
    (tmp / "assets.bin").write_bytes(screen.ui[:889] + screen.sprites)
    captures = bytes([0x88, 0x22, 0x22, 0x22, 0x11] if kings else
                     [0x22, 0x10, 0x10, 0x11, 0x01])
    # common/chess/game_stats.h: total, longest+AT, fastest+AT per side;
    # base 16, flags 18, undo 19..27 (result text after OVER), captures 28.
    def side(total, longest, at, fastest, fast_at):
        return (total.to_bytes(2, "little") + longest.to_bytes(2, "little") +
                bytes([at]) + fastest.to_bytes(2, "little") + bytes([fast_at]))
    stats = side(751, 190, 23, 2, 5) + side(545, 108, 17, 1, 9)
    stats += bytes(12) + captures
    if missing:
        stats = bytes([0xa5] * 18 + [0x41] + [0xa5] * 14)
    if saturated:
        stats = side(65535, 4000, 255, 3725, 255) * 2 + bytes([0, 0, 6]) + bytes(9) + captures
    assert len(stats) == 33
    (tmp / "stats.bin").write_bytes(stats)
    harness = tmp / "vector.asm"
    scratch = parse_const(ROOT / "src/spectrum/lowram_map.h",
                          "NETCHESSZX_NEXT_OVERLAY_SCRATCH_ADDR"
                          if "NETCHESSZX_NEXT_BANKING" in defines
                          else "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR",
                          frozenset(defines))
    harness.write_text(HARNESS.replace("OVERLAY_SCRATCH", str(scratch))
                       .replace("NAMES_SEED", "defb " + name_field(names[0]) + "," + name_field(names[1]))
                       .replace("PENDING", str(pending)).replace("WARNING", str(warning))
                       .replace("RESULT_TEXT", '"DRAW AGREED"' if kings else '"CHECKMATE:YOU WON"'),
                       encoding="ascii")
    binary = tmp / "vector.bin"
    symbols = assemble_vector(required_tool("z88dk-z80asm"), harness,
                              [ROOT / "asm/overlay/about/entry_about.asm",
                               ROOT / "asm/spectrum/text.asm"], binary, defines)
    size = symbol_address(symbols, "gs_render_end") - symbol_address(symbols, "_about_render_ovl_entry") + 5
    assert size <= (8192 if "NETCHESSZX_NEXT_BANKING" in defines else 2048), size
    result, output, pc, _, _ = execute_vector(required_tool("z88dk-ticks"), binary, symbols,
                                             counter=5_000_000)
    assert result == 0 and pc == symbol_address(symbols, "test_done"), output
    ram = binary.with_suffix(".ram").read_bytes()
    get = lambda name, width=2: int.from_bytes(ram[symbol_address(symbols, name):
                                                symbol_address(symbols, name)+width], "little")
    assert (get("returned"), get("saved_ix"), get("saved_iy"), get("waits", 1)) == (1, 0x1234, 0x5c3a, 16)
    # The poison took, and the page's 97 working bytes stay inside scratch.
    assert ram[scratch + 97:scratch + 160] == b"\xa5" * 63
    assert ram[0x5fe0:0x5ff3] == stats[:19]
    assert int.from_bytes(ram[0x5ff3:0x5ff5], "little") == symbol_address(symbols, "result_text")
    assert ram[0x5ff5:0x6001] == stats[21:]
    for y in range(192):
        for x in range(32):
            if not (40 <= y < 168 and 1 <= x <= 16):
                assert ram[0x4000+ula_offset(y)+x] == seed[ula_offset(y)+x], (y, x)
    for row in range(24):
        for col in range(32):
            if not (5 <= row <= 20 and 1 <= col <= 16):
                assert ram[0x5800+row*32+col] == seed[6144+row*32+col]
    screen.pixels = bytearray(b"".join(ram[0x4000+ula_offset(y):0x4000+ula_offset(y)+32] for y in range(192)))
    screen.attrs = bytearray(ram[0x5800:0x5b00])
    # Journal I/O can fail after capture; that does not lose a timed move.
    if not pending and not missing and not saturated:
        actual, expected = screen.image(), build_mockup(0, 0, kings, names)
        if not defines:
            actual.save(ROOT / "build" / ("game-stats-render-kings.png" if kings else "game-stats-render.png"))
        differences = [(x, y) for y in range(40,168) for x in range(8,136)
                       if actual.getpixel((x,y)) != expected.getpixel((x,y))]
        assert not differences, f"approved page mismatch: {differences[:20]} ({len(differences)})"
    def same_text(cases):
        expected = Screen(0, 0)
        for x, y, text in cases:
            expected.small(text, x, y)
            start, end = x // 8, (x + len(text) * 4 + 7) // 8
            for scan in range(y, y + 4):
                assert (screen.pixels[scan*32+start:scan*32+end] ==
                        expected.pixels[scan*32+start:scan*32+end]), (text, x, y)
    if pending or missing:
        for y in range(136, 160):
            assert not any(screen.pixels[y*32+3:y*32+16]), "stale captures on empty START"
        # No captures: each side's material reads 0; no time, no share.
        same_text([(76, 130, "0"), (128, 130, "0")])
        assert not any(any(screen.pixels[y*32+8:y*32+10]) or any(screen.pixels[y*32+15:y*32+17])
                       for y in range(91, 95)), "share"
    if missing or pending & 0x40:
        same_text([(12, 162, "PARTIAL"), (32, 97, "--"), (84, 97, "--"),
                   (32, 115, "--"), (84, 121, "--")])
    if saturated:
        same_text([(x, y, text) for y, text in ((91, "18:12:15+"), (97, "--"),
                                                (103, "1:06:40"), (109, "255+"),
                                                (115, "1:02:05"), (121, "255+"))
                   for x in (32, 84)])
    for y in range(80, 88):
        assert screen.pixels[y*32+16] & 0x0f == 0, "names touch the FILE frame"
    print(f"[OK] ABOUT {','.join(defines) or 'Classic'}: {size} bytes; "
          f"{'30 captures' if kings else 'sample'}; pending={pending}, warning={warning}, "
          f"missing={missing}, saturated={saturated}")
    return size


def main():
    with tempfile.TemporaryDirectory(prefix="game-stats-render-") as directory:
        tmp = Path(directory)
        run_case(tmp, [], True)
        run_case(tmp, [])
        # Ten-character names on both sides: complete, and clear of the frame.
        run_case(tmp, [], names=("ABCDEFGHIJ", "KLMNOPQRST"))
        run_case(tmp, [], pending=128)
        run_case(tmp, [], warning=1)
        run_case(tmp, [], missing=True)
        run_case(tmp, [], pending=0xc0)
        run_case(tmp, [], saturated=True)
        run_case(tmp, ["NETCHESSZX_NEXT_BANKING", "NETCHESSZX_SDCC_IY"])
        run_case(tmp, ["NETCHESSZX_SPECTRANEXT"])


if __name__ == "__main__":
    main()
