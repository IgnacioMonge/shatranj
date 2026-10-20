#!/usr/bin/env python3
"""Execute the production SpectraNext BYE close-wait ASM."""
import re
import tempfile
from pathlib import Path
from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)

HARNESS = r"""
SECTION code_user
test_start:
    ld sp, 0xff00
    ld ix, 0x1234
    ld iy, 0x5c3a
    ld hl, poll_script
    ld (poll_cursor), hl
    ld hl, budget
    call _direct_close_wait_ovl
    ld (return_value), hl
    ld (saved_sp), sp
    ld (saved_ix), ix
    ld (saved_iy), iy
    xor a
    ld (test_result), a
test_done:
    jp test_done
_spxn_poll:
    ld hl, poll_calls
    inc (hl)
    ld hl, (poll_cursor)
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld (poll_cursor), hl
    ex de, hl
    ld bc, 0xabcd
    ret
_spxn_recv:
    pop de
    pop hl
    ld (recv_buffer), hl
    pop bc
    ld (recv_capacity), bc
    push de
    ld hl, recv_calls
    inc (hl)
    ld hl, (recv_result)
    ld bc, 0xabcd
    ret
_net_wait_frame:
    ld hl, wait_calls
    inc (hl)
    ld bc, 0xabcd
    ld de, 0xeeee
    ld hl, 0xdddd
    ret
_direct_close_ovl:
    ld hl, close_calls
    inc (hl)
    ld hl, 1
    ret
test_result: DEFB 255
budget: DEFB 0
poll_script: DEFS 16
poll_cursor: DEFW 0
recv_result: DEFW 3
poll_calls: DEFB 0
recv_calls: DEFB 0
wait_calls: DEFB 0
close_calls: DEFB 0
recv_buffer: DEFW 0
recv_capacity: DEFW 0
return_value: DEFW 0
saved_sp: DEFW 0
saved_ix: DEFW 0
saved_iy: DEFW 0
"""

def main():
    source = (ROOT / "asm/overlay/direct/entry_direct_spectranext.asm").read_text()
    routine = source[source.index("_direct_close_wait_ovl:"):
                     source.index("direct_close_wait_end:")]
    lowram = (ROOT / "src/spectrum/lowram_map.h").read_text()
    packet = (ROOT / "src/spectrum/transport/mqtt_min.h").read_text()
    assert re.search(r"PENDING_DATA_ADDR\s+0x652du", lowram)
    assert re.search(r"SPECTRUM_MQTT_PACKET_MAX\s+160u", packet)
    cases = (
        ("zero", 0, [0], 3, 0, 0, 0),
        ("hup", 3, [2], 3, 1, 0, 0),
        ("hup-with-data", 3, [6], 3, 1, 0, 0),
        ("invalid", 3, [128], 3, 1, 0, 0),
        ("error", 3, [-1], 3, 1, 0, 0),
        ("drain", 3, [4, 2], 3, 2, 1, 1),
        ("timeout", 3, [0, 0, 0], 3, 3, 0, 3),
        ("recv-eof", 3, [4], 0, 1, 1, 0),
        ("recv-error", 3, [4], -1, 1, 1, 0),
    )
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "close_wait.asm"
        harness.write_text(HARNESS + "\n" + routine, encoding="ascii")
        binary = temp / "close_wait.bin"
        symbols = assemble_vector(required_tool("z80asm"), harness, [], binary)
        address = lambda label: symbol_address(symbols, label)
        original = binary.read_bytes()
        size = len(original) - (address("_direct_close_wait_ovl") - 0x8000)
        for name, budget, polls, recv, poll_count, recv_count, waits in cases:
            image = bytearray(original)
            def patch(label, value):
                offset = address(label) - 0x8000
                image[offset:offset + len(value)] = value
            patch("budget", bytes((budget,)))
            patch("poll_script", b"".join((p & 0xffff).to_bytes(2, "little") for p in polls))
            patch("recv_result", (recv & 0xffff).to_bytes(2, "little"))
            binary.write_bytes(image)
            result, _, pc, _, _ = execute_vector(
                required_tool("z88dk-ticks"), binary, symbols, counter=100000)
            ram = binary.with_suffix(".ram").read_bytes()
            assert result == 0 and pc == address("test_done"), name
            for label, want in (("poll_calls", poll_count), ("recv_calls", recv_count),
                                ("wait_calls", waits), ("close_calls", 1)):
                assert ram[address(label)] == want, (name, label, ram[address(label)])
            for label, want in (("saved_sp", 0xff00), ("saved_ix", 0x1234),
                                ("saved_iy", 0x5c3a), ("return_value", 1)):
                got = int.from_bytes(ram[address(label):address(label)+2], "little")
                assert got == want, (name, label, got)
            if recv_count:
                for label, want in (("recv_buffer", 0x652d), ("recv_capacity", 160)):
                    got = int.from_bytes(ram[address(label):address(label)+2], "little")
                    assert got == want, (name, label, got)
    print(f"DIRECT close-wait ASM vector passed ({len(cases)} cases, {size} bytes)")

if __name__ == "__main__":
    main()
