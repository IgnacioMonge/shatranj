#!/usr/bin/env python3
"""Execute production session approval helpers with independent C-policy oracles."""

import itertools
import re
import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)


def asm_body(source, name, required=True):
    match = re.search(
        rf"static (?:uint8_t|void) {name}\([^)]*\)[^{{;]*\{{[^{{}}]*?#asm\n(.*?)#endasm",
        source, re.S,
    )
    if required:
        assert match, f"missing production {name} ASM"
    return match.group(1) if match else None


COMMON = """SECTION code_user
test_start:
    ld sp,0xff00
    ld ix,0x1234
    ld iy,0x5c3a
    ld hl,(frame_seed)
    ld (0x5c78),hl
    {argument}
    call {target}
    ld a,l
    ld (return_value),a
    ld (saved_ix),ix
    ld (saved_iy),iy
    ld (saved_sp),sp
    xor a
    ld (test_result),a
test_done:
    jp test_done
test_result: defb 255
return_value: defb 255
frame_seed: defw 0
arg_seed: defw 0
saved_ix: defw 0
saved_iy: defw 0
saved_sp: defw 0
schedule: defs 7
_netchesszx_session_state: defs 23
_takeback_snapshot_local: defb 0
_confirm_action: defb 0
_net_uart_direct_idle_ticks: defb 0
_netchesszx_transport: defb 0
board_guard: defs 64,0xa5
"""


class Vector:
    def __init__(self, temp, name, body, target, argument="ld hl,(arg_seed)", sources=None):
        harness = temp / f"{name}.asm"
        harness.write_text(COMMON.format(argument=argument, target=target) + body)
        self.binary = temp / f"{name}.bin"
        self.symbols = assemble_vector(required_tool("z80asm"), harness, sources or [], self.binary)
        self.original = self.binary.read_bytes()

    def address(self, name):
        return symbol_address(self.symbols, name)

    def run(self, label, **seeds):
        image = bytearray(self.original)
        for name, value in seeds.items():
            offset = self.address(name) - 0x8000
            if isinstance(value, int):
                value = bytes([value])
            image[offset:offset + len(value)] = value
        self.binary.write_bytes(image)
        result, _, pc, _, _ = execute_vector(
            required_tool("z88dk-ticks"), self.binary, self.symbols, counter=20000,
        )
        assert result == 0 and pc == self.address("test_done"), label
        self.ram = self.binary.with_suffix(".ram").read_bytes()
        for name, expected in (("saved_ix", 0x1234), ("saved_iy", 0x5c3a),
                               ("saved_sp", 0xff00)):
            assert int.from_bytes(self.read(name, 2), "little") == expected, (label, name)
        assert self.read("board_guard", 64) == bytes([0xa5]) * 64, label
        return self.read("return_value")[0]

    def read(self, name, size=1):
        address = self.address(name)
        return self.ram[address:address + size]


def word(value):
    return (value & 65535).to_bytes(2, "little")


def kind_oracle(pending, local, ply, restore, confirmation):
    if pending & 128:
        return 0
    if pending in (1, 4):
        return 1
    if pending in (2, 6):
        return 2
    if local and ply:
        return 3
    if restore & 128:
        return 4
    if confirmation == 8:
        return 7
    if confirmation == 9:
        return 8
    if confirmation == 3:
        return 6 if pending == 3 else 5
    return 0


def check_kind(temp, body):
    vector = Vector(temp, "kind", "_session_decision_kind:\n" + body,
                    "_session_decision_kind")
    count = 0
    for pending, local, ply, restore, confirmation in itertools.product(
        (0, 1, 2, 3, 4, 6, 0x81, 0x82, 0x83), (0, 1), (0, 0x1234),
        (0, 0x10, 0x40, 0x80), (0, 3, 8, 9),
    ):
        state = bytearray(23)
        state[4:6] = word(ply)
        state[10], state[14] = restore, pending
        case = (pending, local, ply, restore, confirmation)
        result = vector.run(case, _netchesszx_session_state=state,
                            _takeback_snapshot_local=local, _confirm_action=confirmation)
        assert result == kind_oracle(*case), (case, result)
        assert vector.read("_netchesszx_session_state", 23) == state, case
        assert vector.read("_confirm_action")[0] == confirmation, case
        count += 1
    return count


def check_deadline(temp, body):
    count = 0
    # The classifier is independently executed above; this stub isolates arithmetic.
    stub = "_session_decision_kind:\n ld a,(kind_seed)\n ld l,a\n ret\nkind_seed: defb 0\n"
    for platform, next_, spectra, default_frames, default_reply, idle, transport, frames, reply in (
        ("classic50", 0, 0, 3000, 63, 0, 0, 3000, 63),
        ("classic60", 0, 0, 3600, 75, 0, 0, 3600, 75),
        ("next50", 1, 0, 3000, 63, 75, 0, 3000, 63),
        ("next60", 1, 0, 3000, 63, 90, 1, 3600, 75),
        ("spectra50direct", 0, 1, 3000, 63, 150, 0, 3000, 125),
        ("spectra60direct", 0, 1, 3000, 63, 180, 0, 3600, 150),
        ("spectra50mqtt", 0, 1, 3000, 63, 150, 1, 3000, 63),
        ("spectra60mqtt", 0, 1, 3000, 63, 180, 1, 3600, 75),
    ):
        defines = (f"defc SESSION_ASM_NEXT={next_}\n"
                   f"defc SESSION_ASM_SPECTRANEXT={spectra}\n"
                   f"defc SESSION_ASM_DEFAULT_FRAMES={default_frames}\n"
                   f"defc SESSION_ASM_DEFAULT_REPLY={default_reply}\n")
        vector = Vector(temp, platform, defines + stub + "_session_deadline_step:\n" + body,
                        "_session_deadline_step", "ld hl,schedule")
        for kind, selection, now, deadline in itertools.product(
            range(9), range(3), (0, 100, 65530), (0, 100, 65530),
        ):
            oldkind = (0, kind, (kind + 1) % 9)[selection]
            initial = bytearray([17, 23, 4, 5, *word(deadline), oldkind])
            expected = initial.copy()
            result = 0
            if not kind:
                expected[6] = 0
            elif kind != oldkind:
                expected[2:7] = bytes([reply, 0, *word(now + frames), kind])
            elif (now - deadline) & 65535 < 32768:
                result = kind
            case = (platform, kind, oldkind, now, deadline)
            got = vector.run(case, schedule=initial, frame_seed=word(now),
                             kind_seed=kind, _net_uart_direct_idle_ticks=idle,
                             _netchesszx_transport=transport)
            assert got == result, (case, got, result)
            assert vector.read("schedule", 7) == expected, (case, vector.read("schedule", 7), expected)
            assert vector.read("_netchesszx_session_state", 23) == bytes(23), case
            count += 1
        for elapsed in (0, 1, 299, frames - 1, frames, frames + 1, 32000):
            # Busy work and retries must retain the original absolute deadline.
            start = 65000
            deadline = (start + frames) & 65535
            initial = bytearray([17, 23, 1, 4, *word(deadline), 7])
            case = (platform, "elapsed", elapsed)
            got = vector.run(case, schedule=initial, frame_seed=word(start + elapsed),
                             kind_seed=7, _net_uart_direct_idle_ticks=idle,
                             _netchesszx_transport=transport)
            expected = 7 if ((start + elapsed - deadline) & 65535) < 32768 else 0
            assert got == expected, case
            assert vector.read("schedule", 7) == initial, case
            count += 1
    return count


def check_takeback(temp, body):
    if body is None:
        return 0
    stubs = """_session_tcp_move_reply:
    pop de
    ld hl,0
    add hl,sp
    ld a,(hl)
    ld (reply_accepted),a
    inc hl
    ld c,(hl)
    inc hl
    ld b,(hl)
    inc hl
    ld sp,hl
    push de
    ld (reply_ply),bc
    ld hl,reply_calls
    inc (hl)
    ld bc,0x5678
    ld de,0x9abc
    ld a,(tx_ok)
    ld l,a
    ret
_notify_control_timeout:
    ld a,l
    ld (notice_kind),a
    ld hl,notice_calls
    inc (hl)
    ret
tx_ok: defb 1
reply_calls: defb 0
reply_accepted: defb 255
reply_ply: defw 0
notice_calls: defb 0
notice_kind: defb 0
"""
    vector = Vector(temp, "takeback", stubs + "_session_cancel_remote_takeback:\n" + body,
                    "_session_cancel_remote_takeback")
    count = 0
    for ply, pending, accepted, confirmation, tx in itertools.product(
        (0, 1, 256, 65535), (0, 1, 256), (0, 1, 256), (0, 3, 8), (0, 1),
    ):
        state = bytearray(range(23))
        state[4:6], state[6:8] = word(pending), word(accepted)
        expected = state.copy()
        matched = bool(ply and confirmation == 8 and ply == pending)
        if matched:
            expected[4:6] = word(0)
        case = (ply, pending, accepted, confirmation, tx)
        got = vector.run(case, arg_seed=word(ply), _netchesszx_session_state=state,
                         _confirm_action=confirmation, tx_ok=tx)
        assert got == (2 if ply and not tx else 1), (case, got)
        assert vector.read("_netchesszx_session_state", 23) == expected, case
        assert vector.read("_confirm_action")[0] == (0 if matched else confirmation), case
        assert vector.read("reply_calls")[0] == bool(ply), case
        if ply:
            assert vector.read("reply_ply", 2) == word(ply), case
            assert vector.read("reply_accepted")[0] == (ply == accepted), case
        assert vector.read("notice_calls")[0] == (matched and bool(tx)), case
        if matched and tx:
            assert vector.read("notice_kind")[0] == 3, case
        count += 1
    return count


CONTROL_STUBS = """_NETCHESS_PROTO_NACK_RESET: defm "NACK RESET"
    defb 0
_session_tcp_reply:
    ld (reply_text),hl
    ld hl,reset_reply_calls
    inc (hl)
    jr reply_result
_send_draw_reply:
    ld a,l
    ld (reply_accepted),a
    ld hl,draw_reply_calls
    inc (hl)
reply_result:
    ld bc,0x5678
    ld de,0x9abc
    ld a,(tx_ok)
    ld l,a
    ret
_notify_control_timeout:
    ld a,l
    ld (notice_kind),a
    ld hl,notice_calls
    inc (hl)
    ret
_session_game_over_idle:
    ld hl,over_calls
    inc (hl)
    ret
tx_ok: defb 1
reset_reply_calls: defb 0
draw_reply_calls: defb 0
reply_accepted: defb 255
reply_text: defw 0
notice_calls: defb 0
notice_kind: defb 0
over_calls: defb 0
"""


def check_control(temp, body):
    if body is None:
        return 0
    vector = Vector(temp, "control", CONTROL_STUBS + "_session_cancel_remote_control:\n" + body,
                    "_session_cancel_remote_control")
    count = 0
    for reset, pending, accepted, confirmation, over, tx in itertools.product(
        (0, 1), (0, 1, 3, 0x81), (0, 1, 2, 3), (0, 3, 8), (0, 1), (0, 1),
    ):
        state = bytearray(range(23))
        state[8], state[12], state[14] = accepted, over, pending
        expected = state.copy()
        matched = (confirmation == 3 and pending == 0) if reset else pending == 3
        replay = not matched and accepted == (1 if reset else 2)
        if matched:
            expected[14] = 0
        case = (reset, pending, accepted, confirmation, over, tx)
        got = vector.run(case, arg_seed=word(reset), _netchesszx_session_state=state,
                         _confirm_action=confirmation, tx_ok=tx)
        assert got == (1 if tx else 2), (case, got)
        assert vector.read("_netchesszx_session_state", 23) == expected, case
        assert vector.read("_confirm_action")[0] == (0 if matched else confirmation), case
        assert vector.read("reset_reply_calls")[0] == reset, case
        assert vector.read("draw_reply_calls")[0] == (not reset), case
        if reset:
            address = vector.address("_NETCHESS_PROTO_NACK_RESET") + replay
            assert vector.read("reply_text", 2) == word(address), case
        else:
            assert vector.read("reply_accepted")[0] == replay, case
        over_callback = matched and tx and reset and over
        notice = matched and tx and not over_callback
        assert vector.read("over_calls")[0] == over_callback, case
        assert vector.read("notice_calls")[0] == notice, case
        if notice:
            assert vector.read("notice_kind")[0] == (1 if reset else 2), case
        count += 1
    return count


def check_notice(temp, source):
    body = asm_body(source, "notify_control_timeout", False)
    if body is None:
        return 0
    strings = ""
    for symbol, value in (("NETCHESS_PROTO_TAKEBACK", "TAKEBACK"),
                          ("NETCHESS_PROTO_TAKEBACK_PREFIX", "TAKEBACK "),
                          ("NETCHESS_PROTO_RESET", "RESET"),
                          ("NETCHESS_PROTO_NACK_RESET", "NACK RESET"),
                          ("NETCHESS_PROTO_DRAW", "DRAW")):
        strings += f'_{symbol}: defm "{value}"\n defb 0\n'
    for symbol in ("msg_control_expired", "msg_control_cancelled"):
        match = re.search(rf'static const char {symbol}\[\] = "([^"\\]*)";', source)
        assert match, symbol
        strings += f'_{symbol}: defm "{match.group(1)}"\n defb 0\n'
    stubs = """EXTERN _spectrum_append_text
_spectrum_net_payload_scratch:
    ld bc,0x789a
    ld de,0xbcde
    ld hl,payload
    ret
_notify_info:
    push ix
    ld ix,0
    add ix,sp
    ld l,(ix+4)
    ld h,(ix+5)
    ld (notice_pointer),hl
    pop ix
    ld hl,notice_calls
    inc (hl)
    ld bc,0x5678
    ld de,0x9abc
    ret
notice_calls: defb 0
notice_pointer: defw 0
payload: defs 64,0xee
"""
    vector = Vector(temp, "notice", strings + stubs + "_notify_control_timeout:\n" + body,
                    "_notify_control_timeout", sources=[ROOT / "asm/spectrum/text.asm"])
    count = 0
    for kind in (0, 1, 2, 3, 4, 0x80, 0x81, 0x82, 0x83, 255):
        state = bytes(range(23))
        vector.run(kind, arg_seed=word(kind), _netchesszx_session_state=state)
        prefix = {1: "RESET", 3: "TAKEBACK"}.get(kind & 3, "DRAW")
        suffix = " cancelled: no response" if kind & 128 else " request withdrawn"
        expected = (prefix + suffix).encode("ascii") + b"\0"
        assert vector.read("payload", len(expected)) == expected, kind
        assert vector.read("payload", 64)[len(expected):] == bytes([0xee]) * (64 - len(expected)), kind
        assert vector.read("notice_pointer", 2) == word(vector.address("payload")), kind
        assert vector.read("notice_calls")[0] == 1, kind
        assert vector.read("_netchesszx_session_state", 23) == state, kind
        count += 1
    return count


def check_expiry(temp, body):
    stubs = """_msg_restore_rn: defm "RN"
    defb 0
_msg_load_cancelled: defm "Load cancelled"
    defb 0
_retry_pending_outgoing:
    ld a,(_netchesszx_session_state+14)
    ld (retry_control),a
    ld hl,retry_calls
    inc (hl)
    ld a,(tx_ok)
    ld l,a
    ret
_tcp_required:
    ld a,l
    ld (required_argument),a
    ld a,(required_calls)
    inc a
    ld (required_calls),a
    ld bc,0x5678
    ld de,0x9abc
    ret
_session_tcp_send:
    ld (send_pointer),hl
    ld hl,send_calls
    inc (hl)
    ld a,(tx_ok)
    ld l,a
    ret
_notify_info:
    push ix
    ld ix,0
    add ix,sp
    ld l,(ix+4)
    ld h,(ix+5)
    ld (info_pointer),hl
    pop ix
    ld hl,info_calls
    inc (hl)
    ret
_notify_control_timeout:
    ld a,l
    ld (notice_kind),a
    ld hl,notice_calls
    inc (hl)
    ret
_session_cancel_remote_takeback:
    ld (takeback_ply),hl
    ld hl,takeback_calls
    inc (hl)
    jr cancel_result
_session_cancel_remote_control:
    ld a,l
    ld (reset_argument),a
    ld hl,control_calls
    inc (hl)
cancel_result:
    ld bc,0x5678
    ld de,0x9abc
    ld a,(cancel_status)
    ld l,a
    ret
_session_tcp_reply:
    ld (reply_pointer),hl
    ld hl,reply_calls
    inc (hl)
    ld bc,0x5678
    ld de,0x9abc
    ld a,(tx_ok)
    ld l,a
    ret
_spectrum_gui_notify_msg:
    ld (gui_argument),hl
    ld hl,gui_calls
    inc (hl)
    ret
cancel_status: defb 1
tx_ok: defb 1
takeback_calls: defb 0
takeback_ply: defw 0
control_calls: defb 0
reset_argument: defb 255
reply_calls: defb 0
reply_pointer: defw 0
gui_calls: defb 0
gui_argument: defw 0
retry_calls: defb 0
retry_control: defb 0
required_calls: defb 0
required_argument: defb 255
send_calls: defb 0
send_pointer: defw 0
info_calls: defb 0
info_pointer: defw 0
notice_calls: defb 0
notice_kind: defb 0
"""
    vector = Vector(temp, "expiry", stubs + "_session_expire_decision:\n" + body,
                    "_session_expire_decision")
    count = 0
    for kind, status, tx in itertools.product(range(1, 9), (1, 2), (0, 1)):
        state = bytearray(range(23))
        state[4:6] = word(0x1234)
        expected = state.copy()
        ok = bool(tx) if kind <= 4 or kind == 8 else status != 2
        if kind < 4:
            expected[14] = kind | 128
        if kind == 6 and ok:
            expected[8] = 3
        if kind in (4, 8) and ok:
            expected[10] = 0
        case = (kind, status, tx)
        got = vector.run(case, arg_seed=word(kind), _netchesszx_session_state=state,
                         _confirm_action=9, cancel_status=status, tx_ok=tx)
        assert got == ok, (case, got)
        assert vector.read("_netchesszx_session_state", 23) == expected, case
        assert vector.read("_confirm_action")[0] == (0 if kind == 8 and ok else 9), case
        assert vector.read("takeback_calls")[0] == (kind == 7), case
        assert vector.read("control_calls")[0] == (kind in (5, 6)), case
        assert vector.read("reply_calls")[0] == (kind == 8), case
        assert vector.read("gui_calls")[0] == (kind == 8 and ok), case
        assert vector.read("retry_calls")[0] == (kind < 4), case
        assert vector.read("required_calls")[0] == (kind < 4), case
        assert vector.read("send_calls")[0] == (kind == 4), case
        assert vector.read("info_calls")[0] == (kind == 4 and ok), case
        assert vector.read("notice_calls")[0] == (kind < 4 and ok), case
        if kind < 4:
            assert vector.read("retry_control")[0] == kind | 128, case
            assert vector.read("required_argument")[0] == tx, case
            if ok:
                assert vector.read("notice_kind")[0] == kind | 128, case
        if kind == 4:
            assert vector.read("send_pointer", 2) == word(vector.address("_msg_restore_rn")), case
            if ok:
                assert vector.read("info_pointer", 2) == word(vector.address("_msg_load_cancelled")), case
        if kind == 7:
            assert vector.read("takeback_ply", 2) == word(0x1234), case
        if kind in (5, 6):
            assert vector.read("reset_argument")[0] == (kind == 5), case
        if kind == 8:
            assert vector.read("reply_pointer", 2) == word(vector.address("_msg_restore_rn")), case
            if ok:
                assert vector.read("gui_argument", 2) == word(0x0123), case
        count += 1
    return count


def main():
    source = (ROOT / "src/spectrum/session/transitions.inc").read_text()
    for name, values in (("DEFAULT_FRAMES", [3600, 3000]), ("DEFAULT_REPLY", [75, 63]),
                         ("NEXT", [1, 0]), ("SPECTRANEXT", [1, 0])):
        actual = [int(value) for value in re.findall(
            rf"^#define SESSION_ASM_{name} (\d+)$", source, re.M)]
        assert actual == values, (name, actual, values)
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        kinds = check_kind(temp, asm_body(source, "session_decision_kind"))
        deadlines = check_deadline(temp, asm_body(source, "session_deadline_step"))
        takebacks = check_takeback(temp, asm_body(source, "session_cancel_remote_takeback", False))
        controls = check_control(temp, asm_body(source, "session_cancel_remote_control", False))
        notices = check_notice(temp, (ROOT / "src/spectrum/app/app.c").read_text())
        expiries = check_expiry(temp, asm_body(source, "session_expire_decision"))
    print(f"Session Z80: {kinds} classifier, {deadlines} deadline, "
          f"{takebacks} TAKEBACK, {controls} RESET/DRAW, {notices} notices, {expiries} expiries passed")


if __name__ == "__main__":
    main()
