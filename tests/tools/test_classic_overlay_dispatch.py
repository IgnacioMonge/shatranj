#!/usr/bin/env python3
"""Execute the Classic dispatcher and guard live-interrupt overlay paths."""

import re
import subprocess
import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import required_tool

ROOT = Path(__file__).resolve().parents[2]


def block(source, start, end):
    return source[source.index(start):source.index(end, source.index(start))]


def check_dispatch(work, source):
    production = block(
        source,
        "SECTION bss_user",
        "IFNDEF NETCHESSZX_CLASSIC_BOOT\n_spectrum_assets_load:",
    )
    production += block(source, "assets_fail:", "_netchesszx_piece_set_load:")
    production += block(source, "ovl_call_loaded:", "ovl_select_atlas_entry:")
    prefix = source[:source.index("SECTION bss_user")]
    prefix = re.sub(r"(?m)^(?:SECTION|PUBLIC|EXTERN)\b[^\n]*", "", prefix)
    (work / "overlay_atlas_table.asm").write_text("ovl_atlas_count EQU 2\n")
    harness = (ROOT / "tests/spectrum/classic_overlay_dispatch_vector.asm").read_text()
    (work / "vector.asm").write_text(harness + prefix + production)
    for command in (
        [required_tool("z88dk-z80asm"), "-b", "-m", "-r0x8000", "-I.",
         "-O=.", "-o=vector.bin", "vector.asm"],
        [required_tool("z88dk-ticks"), "-mz80", "-l", "0x8000", "-pc", "8000",
         "-end", "0", "-counter", "1000000", "-output", "vector.ram", "vector.bin"],
    ):
        result = subprocess.run(command, cwd=work, text=True, capture_output=True)
        assert result.returncode == 0, result.stdout + result.stderr
    ram = (work / "vector.ram").read_bytes()
    assert ram[0xF800] == 0 and int.from_bytes(ram[0x10006:0x10008], "little") == 0, (
        f"Classic dispatcher vector failed: {ram[0xF800]}"
    )


READ_CASES = [(n, 0) for n in (0, 1, 255, 256, 511, 512, 513, 1800, 2048, 13824)]
READ_CASES += [(2048, 1), (2048, 2)]  # F_READ carry, short read

READ_HARNESS = """
    xor a
    ld (0xF800), a
    jp 0
check_frame:
    push iy
    pop hl
    ld de, 0x5C3A
    or a
    sbc hl, de
    jp nz, fail
    ld hl, 2
    add hl, sp
    ld de, 0xFF00
    or a
    sbc hl, de
    jp nz, fail
    ld a, (ovl_handle)
    cp 0x42
    jp nz, fail
    ret
check_request:
    ld a, (stub_fn)
    cp 0x9d
    jp nz, fail
    ld a, (stub_a)
    cp 0x42
    jp nz, fail
    ld hl, (stub_ix)
    ld de, 0x9000
    or a
    sbc hl, de
    jp nz, fail
    ret
rst8_stub:
    ld (stub_a), a
    ex (sp), hl
    ld a, (hl)
    inc hl
    ex (sp), hl
    ld (stub_fn), a
    ld (stub_bc), bc
    ld (stub_ix), ix
    ld hl, stub_calls
    inc (hl)
    ld iy, 0
    ld ix, 0x5555
    ld de, 0xAAAA
    ld hl, 0x3333
    ld a, (mode)
    cp 1
    jr z, stub_carry
    cp 2
    jr nz, stub_ok
    dec bc
stub_ok:
    or a
    ret
stub_carry:
    ld bc, 0
    ld a, 5
    scf
    ret
_spectrum_uart_background_pump:
    ld hl, pump_calls
    inc (hl)
    ld bc, 0x7777
    ld de, 0x7777
    ret
fail:
    ld a, 255
    ld (0xF800), a
    jp 0
mode: defb 0
stub_calls: defb 0
pump_calls: defb 0
stub_a: defb 0
stub_fn: defb 0
stub_bc: defw 0
stub_ix: defw 0
ovl_handle: defb 0
"""


def check_read(work, source):
    """Run the production Classic reader against a hostile F_READ stub: one
    RST 8 per request, one UART pump after success, none after failure."""
    reader = block(source, "ovl_read_chunked:",
                   "IFNDEF NETCHESSZX_SPECTRANEXT\novl_load_fail_close:")
    cases = []
    for length, mode in READ_CASES:
        cases.append(f"""
    ld a, {mode}
    ld (mode), a
    xor a
    ld (stub_calls), a
    ld (pump_calls), a
    ld bc, {length}
    ld ix, 0x9000
    ld a, 0x42
    call ovl_read_chunked
    jp {'nc' if mode else 'c'}, fail
    ld hl, {length}
    or a
    sbc hl, bc
    jp nz, fail
    ld a, (stub_calls)
    cp {1 if length else 0}
    jp nz, fail
    ld a, (pump_calls)
    cp {1 if length and not mode else 0}
    jp nz, fail
    call check_frame""")
        if length:
            cases.append(f"""
    ld hl, (stub_bc)
    ld de, {length}
    or a
    sbc hl, de
    jp nz, fail
    call check_request""")
    (work / "reader.asm").write_text(
        "SECTION code_user\n"
        "    ld sp, 0xFF00\n    ld a, 254\n    ld (0xF800), a\n"
        "    ld a, 0xC3\n    ld (0x0008), a\n    ld hl, rst8_stub\n"
        "    ld (0x0009), hl\n    ld iy, 0x5C3A\n"
        + "".join(cases) + READ_HARNESS + reader)
    for command in (
        [required_tool("z88dk-z80asm"), "-b", "-m", "-r0x8000",
         "-O=.", "-o=reader.bin", "reader.asm"],
        [required_tool("z88dk-ticks"), "-mz80", "-l", "0x8000", "-pc", "8000",
         "-end", "0", "-counter", "1000000", "-output", "reader.ram", "reader.bin"],
    ):
        result = subprocess.run(command, cwd=work, text=True, capture_output=True)
        assert result.returncode == 0, result.stdout + result.stderr
    ram = (work / "reader.ram").read_bytes()
    assert ram[0xF800] == 0 and int.from_bytes(ram[0x10006:0x10008], "little") == 0, (
        f"Classic single-read vector failed: {ram[0xF800]}"
    )


# Each case: reads that fail first, whether the file starts open, then the
# expected esxDOS calls (open, seek, read, close), carry and loaded id.
LOAD_CASES = [
    (0, 0, (1, 1, 1, 0), 0, 1),   # clean load keeps the file open
    (1, 1, (1, 2, 2, 1), 0, 1),   # one failed read: close, reopen, retry
    (2, 1, (1, 2, 2, 2), 1, 255),  # retry fails too: give up, file closed
]

LOAD_HARNESS = """
SECTION code_user
    ld sp, 0xFF00
    ld a, 254
    ld (0xF800), a
    ld a, 0xC3
    ld (0x0008), a
    ld hl, load_rst8
    ld (0x0009), hl
    ld iy, 0x5C3A
{cases}
    xor a
    ld (0xF800), a
    jp 0
load_rst8:
    ex (sp), hl
    ld e, (hl)
    inc hl
    ex (sp), hl
    ld d, 0
    ld hl, load_calls - 0x9a
    add hl, de
    inc (hl)
    ld a, e
    cp 0x9d
    jr nz, load_rst8_ok
    ld hl, load_fail_reads
    ld a, (hl)
    or a
    jr z, load_rst8_ok
    dec (hl)
    ld bc, 0
    scf
    ret
load_rst8_ok:
    ld a, 0x42
    or a
    ret
_spectrum_uart_background_pump:
    ret
load_fail:
    ld a, 255
    ld (0xF800), a
    jp 0
load_fail_reads: defb 0
load_calls: defs 6                 ; 0x9a open .. 0x9f seek
"""


def check_load_retry(work, source):
    """A failed overlay read is retried once from a fresh open; a second
    failure gives up with the file closed and no overlay marked loaded."""
    cases = []
    for fails, start_open, calls, carry, loaded in LOAD_CASES:
        opens, seeks, reads, closes = calls
        cases.append(f"""
    ld hl, load_calls
    ld b, 6
    xor a
clear_{fails}:
    ld (hl), a
    inc hl
    djnz clear_{fails}
    ld (ovl_cache_ready), a
    ld a, {fails}
    ld (load_fail_reads), a
    ld a, {start_open}
    ld (ovl_file_open), a
    ld a, 0x42
    ld (ovl_handle), a
    ld a, 1
    ld (ovl_id), a
    call ovl_ensure_loaded
    ld a, 0
    rla
    cp {carry}
    jp nz, load_fail
    ld a, (_spectrum_overlay_loaded_id)
    cp {loaded}
    jp nz, load_fail
    ld a, (load_calls + 0)
    cp {opens}
    jp nz, load_fail
    ld a, (load_calls + 5)
    cp {seeks}
    jp nz, load_fail
    ld a, (load_calls + 3)
    cp {reads}
    jp nz, load_fail
    ld a, (load_calls + 1)
    cp {closes}
    jp nz, load_fail""")
    loader = "\n".join(line for line in source.splitlines()
                        if not line.startswith("EXTERN"))
    (work / "overlay_atlas_table.asm").write_text(
        "ovl_atlas_count EQU 2\novl_atlas_table:\n    DW 0, 100, 300\n")
    (work / "load.asm").write_text(
        LOAD_HARNESS.format(cases="".join(cases)) + loader)
    for command in (
        [required_tool("z88dk-z80asm"), "-b", "-m", "-r0x8000", "-I.",
         "-O=.", "-o=load.bin", "load.asm"],
        [required_tool("z88dk-ticks"), "-mz80", "-l", "0x8000", "-pc", "8000",
         "-end", "0", "-counter", "1000000", "-output", "load.ram", "load.bin"],
    ):
        result = subprocess.run(command, cwd=work, text=True, capture_output=True)
        assert result.returncode == 0, result.stdout + result.stderr
    ram = (work / "load.ram").read_bytes()
    assert ram[0xF800] == 0, f"Classic load-retry vector failed: {ram[0xF800]}"


# Classic overlay owning each traced call; T and E load nothing here.
MOVE_OVERLAYS = {"R": ("RULES",), "G": ("GUI_LOG",), "g": ("GUI_LOG",),
                 "N": ("GUI_LOG",), "J": ("SAVELOAD",), "S": ("STATUS",),
                 "K": ("BOARD", "RECOVERY"), "T": (), "E": ()}

MOVE_STUBS = r"""
#include <stdint.h>
#include <stdio.h>
#include "spectrum/render_status.h"
#define SPECTRUM_BOARD_CHECK_NONE 0u
#define SPECTRUM_BOARD_CHECK 1u
#define SPECTRUM_BOARD_CHECK_MATE 2u
#define SPECTRUM_BOARD_STALEMATE 3u
#define STATUS_PHASE_CONNECTED 3u
#define STATUS_PHASE_GAME 4u
#define SPECTRUM_GUI_TURN_WHITE 0u
#define SPECTRUM_GUI_TURN_BLACK 1u
#define SPECTRUM_GUI_TURN_WHITE_CHECK 2u
#define SPECTRUM_GUI_TURN_BLACK_CHECK 3u
#define NETCHESSZX_UI_PHASE_YOUR_TURN "Your turn"
#define notify_info_msg(id) putchar('N')
#define netchesszx_session_view() (&view)
static struct { uint8_t game_status_active; } view = { 1u };
static uint8_t state_in, white, local_turn, game_check_state, status_phase_current;
static uint8_t status_ok = 1u;
static const char *msg_checkmate_won = "w", *msg_checkmate_lost = "l",
                  *msg_stalemate = "s";
static uint8_t spectrum_board_san_append_suffix(char *s) { (void)s; putchar('R'); return state_in; }
static uint8_t spectrum_board_check_state(void) { putchar('R'); return state_in; }
static uint8_t netchesszx_notation_is_san(void) { return 1u; }
static const char *move_display_with_check_suffix(const char *m, char *o, uint8_t s) { (void)o; (void)s; return m; }
static void spectrum_gui_add_move(const char *p, const char *d) { (void)p; (void)d; putchar('G'); }
static void spectrum_gui_prepare_move_row(void) { putchar('g'); }
static void spectrum_journal_after_move(const char *m) { (void)m; putchar('J'); }
static void spectrum_gui_apply_move(const char *m) { (void)m; }
static void pending_local_clear(void) {}
static void end_game_over(const char *m) { (void)m; putchar('E'); }
static uint8_t side_to_move_is_white(void) { return white; }
static void spectrum_app_move_timer_reset(void) { putchar('T'); }
static void recovery_checkpoint(void) { putchar('K'); }
static uint8_t spectrum_gui_status_phase(uint8_t s) { (void)s; putchar('S'); return status_ok; }
static void spectrum_gui_set_turn_label(uint8_t m) { (void)m; }
static void cursor_hide(void) {}
static void cursor_show(void) {}
static void cursor_reset_default_square(void) {}
static void notify_info(const char *t) { (void)t; }
"""

MOVE_MAIN = r"""
int main(void)
{
    char san[8] = "e4";
    uint8_t game, local;

    for (game = 0u; game < 2u; ++game)
        for (state_in = 0u; state_in < 4u; ++state_in)
            for (white = 0u; white < 2u; ++white)
                for (local = 0u; local < 2u; ++local) {
                    status_phase_current = game ? STATUS_PHASE_GAME
                                                : STATUS_PHASE_CONNECTED;
                    finish_applied_move("1", "e2e4", san, 1u, "e4", local);
                    printf(" %u\n", (unsigned)game_check_state);
                }
    /* A GAME paint whose STATUS overlay failed to load is retried. */
    status_phase_current = STATUS_PHASE_CONNECTED;
    state_in = white = 0u;
    status_ok = 0u;
    finish_applied_move("1", "e2e4", san, 1u, "e4", 1u);
    status_ok = 1u;
    finish_applied_move("1", "e2e4", san, 1u, "e4", 1u);
    finish_applied_move("1", "e2e4", san, 1u, "e4", 1u);
    printf(" %u\n", (unsigned)status_phase_current);
    return 0;
}
"""


def classic_move_loads(trace):
    """Overlay loads in a single-slot Classic run; BOARD applied the move."""
    loaded, loads = "BOARD", 0
    for call in trace:
        for overlay in MOVE_OVERLAYS[call]:
            loads += overlay != loaded
            loaded = overlay
    return loads


def check_move_overlay_sequence(work, app):
    """Execute the real post-move bookkeeping with tracing callees: GUI_LOG's
    next row and the opponent's-turn notice follow the added move while that
    overlay is loaded, finished games get neither, and a GAME status already
    on screen is not reloaded."""
    code = "".join(block(app, start, end) for start, end in (
        ("static void status_show_phase(", "static void status_show_endpoint("),
        ("static void status_refresh_game(", "static void pending_local_clear("),
        ("static void turn_set_notice(", "static void local_controls_reset("),
        ("static void finish_applied_move(",
         "static const char *move_san_or_fallback("),
    ))
    (work / "move.c").write_text(MOVE_STUBS + code + MOVE_MAIN)
    exe = work / "move.exe"
    for command in (
        [required_tool("gcc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
         "-D__z88dk_fastcall=", f"-I{ROOT / 'src'}", "move.c", "-o", str(exe)],
        [str(exe)],
    ):
        result = subprocess.run(command, cwd=work, text=True, capture_output=True)
        assert result.returncode == 0, result.stdout + result.stderr
    lines = iter(result.stdout.split())
    for game in (0, 1):
        for state in range(4):
            for white in (0, 1):
                for local in (0, 1):
                    trace, check = next(lines), int(next(lines))
                    # The turn timer restarts as soon as the move is
                    # captured, so later bookkeeping counts for the next turn.
                    want = "RG" + ("g" if state <= 1 and white else "")
                    # The opponent's-turn notice is a GUI_LOG message too.
                    want += "N" if state == 0 and not local else ""
                    want += "JT"
                    if state >= 2:
                        want += "E"
                    else:
                        want += "" if game else "S"
                        want += "K"
                    assert trace == want, (game, state, white, local, trace)
                    assert check == (1 if state == 1 else 0), (state, check)
                    if game and state == 0 and not local:
                        assert classic_move_loads(trace) == 5, trace
    trace, phase = next(lines), int(next(lines))
    assert trace == "RGJTSK" * 2 + "RGJTK", trace
    assert phase == 4, phase


HOT_CLASSIFY_SRC = (
    "src/spectrum/config/session.c", "src/spectrum/session/identity.c",
    "src/spectrum/session/state.c",
    "src/common/protocol/direct_session_protocol.c",
    "src/common/protocol/game_protocol.c",
    "src/common/protocol/game_protocol_extra.c",
    "src/common/protocol/game_protocol_mach.c",
    "src/common/protocol/mqtt_session_protocol.c",
    "src/spectrum/transport/keepalive_protocol.c",
    "src/spectrum/session/mqtt.c", "src/spectrum/session/event.c",
)

HOT_CLASSIFY_MAIN = r"""
#include <stdio.h>
#include "spectrum/session/event.h"

netchesszx_session_event_t control_classify_real(const char *payload);
static unsigned control_calls;

netchesszx_session_event_t netchesszx_session_classify_game_payload(
    const char *payload)
{
    ++control_calls;
    return control_classify_real(payload);
}

static int expect(const char *payload, netchesszx_session_event_t event,
                  unsigned calls)
{
    netchesszx_session_event_t got;

    control_calls = 0u;
    got = netchesszx_session_classify_event(payload, 0u, 0u, 1u);
    if (got != event || control_calls != calls) {
        printf("%s: event %d (want %d), CONTROL calls %u (want %u)\n",
               payload, (int)got, (int)event, control_calls, calls);
        return 1;
    }
    return 0;
}

int main(void)
{
    return expect("MOVE 1 e2e4", NETCHESSZX_SESSION_EVENT_MOVE, 0u) |
           expect("CHAT hi", NETCHESSZX_SESSION_EVENT_CHAT, 0u) |
           expect("ACK 1", NETCHESSZX_SESSION_EVENT_ACK_MOVE, 0u) |
           expect("ACK 12345 e4", NETCHESSZX_SESSION_EVENT_ACK_MOVE, 0u) |
           expect("ACK 123456", NETCHESSZX_SESSION_EVENT_UNKNOWN, 1u) |
           expect("ACK GAME START", NETCHESSZX_SESSION_EVENT_ACK_GAME_START,
                  1u);
}
"""


def check_hot_classification(work):
    """On Classic a ply's MOVE and the ACK of a move (like CHAT) are
    classified without loading CONTROL; other ACKs still go through it."""
    (work / "hot.c").write_text(HOT_CLASSIFY_MAIN)
    gcc = required_tool("gcc")
    flags = ["-std=c99", "-Wall", "-Wextra", "-Werror", f"-I{ROOT / 'src'}"]
    exe = work / "hot.exe"
    for command in (
        [gcc, *flags, "-Dnetchesszx_session_classify_game_payload="
         "control_classify_real", "-c",
         str(ROOT / "src/spectrum/overlay/control_ovl.c"), "-o", "control.o"],
        [gcc, *flags, *(str(ROOT / s) for s in HOT_CLASSIFY_SRC), "control.o",
         "hot.c", "-o", str(exe)],
        [str(exe)],
    ):
        result = subprocess.run(command, cwd=work, text=True, capture_output=True)
        assert result.returncode == 0, result.stdout + result.stderr


def check_gui_log_chat_interrupt(source):
    function = block(source, "_gui_log_add_chat_ovl:", "chat_new_row:")
    assert re.fullmatch(
        r"_gui_log_add_chat_ovl:\s+IFDEF NETCHESSZX_SDCC_IY\s+"
        r"IFNDEF NETCHESSZX_SPECTRANEXT\s+(;[^\n]*\s+)*ei\s+ENDIF\s+ENDIF\s+.*",
        function,
        re.S,
    ), "Classic/Next GUI_LOG chat entry must enable interrupts before rendering"


def check_direct_read_interrupt(source):
    start = source.rindex("uint8_t direct_read_payload_ovl(")
    function = source[start:source.index(
        "return (uint8_t)SPECTRUM_NET_READ_TIMEOUT;", start)]
    assert re.search(
        r"NETCHESSZX_SDCC_IY.*?#asm\s+ei\s+#endasm.*?"
        r"direct_drain_uart_ovl\(",
        function,
        re.S,
    ), "Classic/Next DIRECT read must enable interrupts before draining UART"


def check_about_interrupt(source):
    start = source.rindex("_about_render_ovl_entry:")
    function = source[start:source.index("about_ok:", start)]
    assert re.search(
        r"push ix\s+push iy.*?IFDEF NETCHESSZX_SDCC_IY\s+"
        r"IFNDEF NETCHESSZX_SPECTRANEXT\s+ei\s+ENDIF\s+ENDIF\s+"
        r"call ovl_close_overlay_file",
        function,
        re.I | re.S,
    ), "Classic About must enable its resident frame interrupt before I/O"
    read_start = source.index("about_read_input:", start)
    read_end = source.index("about_close:", read_start)
    read_input = source[read_start:read_end]
    assert "halt" not in read_input.lower(), (
        "Classic About must not add per-group input latency"
    )
    assert re.search(r"call about_dzx0_standard", read_input, re.I)
    assert re.search(r"ld ix, about_directory", function, re.I)


def check_fileui_interrupts(source):
    render = block(
        source,
        "uint8_t fileui_render_ovl(",
        "/* Resolves the file name",
    )
    pick = source[source.index("uint8_t fileui_pick_ovl("):]
    guard = (
        r"NETCHESSZX_SDCC_IY.*?!defined\(NETCHESSZX_SPECTRANEXT\).*?"
        r"#asm\s+ei\s+#endasm"
    )
    assert re.search(guard, render, re.S), (
        "Classic/Next FILEUI render must enable interrupts before drawing and I/O"
    )
    assert re.search(guard, pick, re.S), (
        "Classic/Next FILEUI pick must enable interrupts before directory I/O"
    )


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        check_dispatch(work, (ROOT / "asm/esxdos/overlay_loader.asm").read_text())
        check_read(work, (ROOT / "asm/esxdos/overlay_loader.asm").read_text())
        check_load_retry(
            work, (ROOT / "asm/esxdos/overlay_loader.asm").read_text())
        check_move_overlay_sequence(
            work, (ROOT / "src/spectrum/app/app.c").read_text())
        check_hot_classification(work)
        check_gui_log_chat_interrupt(
            (ROOT / "asm/overlay/gui_log/entry_gui_log.asm").read_text()
        )
        check_direct_read_interrupt(
            (ROOT / "src/spectrum/overlay/direct_ovl.c").read_text()
        )
        check_about_interrupt(
            (ROOT / "asm/overlay/about/entry_about.asm").read_text()
        )
        check_fileui_interrupts(
            (ROOT / "src/spectrum/overlay/fileui_ovl.c").read_text()
        )
    print("[OK] Classic dispatcher, single-read loader and its retry, "
          "move overlay order "
          "and live-interrupt overlay seams")
