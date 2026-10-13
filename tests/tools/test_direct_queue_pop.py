#!/usr/bin/env python3
"""Execute the productive resident DIRECT queue pop routine on Z80."""

import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)


HARNESS = r"""
SECTION code_user

direct_rx_spill EQU 0x64fa

test_start:
    ld sp, 0xff00
    ld hl, payload_seed
    ld de, _direct_rx_payload
    ld bc, 48
    ldir
    ld hl, payload2_seed
    ld de, _direct_rx_payload2
    ld bc, 48
    ldir
    ld hl, spill_seed
    ld de, direct_rx_spill
    ld bc, 49
    ldir
    ld a, (head_seed)
    ld (_direct_rx_head), a
    ld a, (count_seed)
    ld (_direct_rx_count), a
    ld a, (cap_seed)
    ld (_spectrum_overlay_context + 2), a
    ld hl, output
    ld (_spectrum_overlay_context), hl
    ld hl, output_seed
    ld de, output
    ld bc, 8
    ldir
    ld ix, 0x1234
    ld iy, 0x5c3a
measure_call:
    call _netchesszx_asm_direct_pop
measure_done:
    ld a, l
    ld (return_value), a
    ld (saved_sp), sp
    ld (saved_ix), ix
    ld (saved_iy), iy
    xor a
    ld (test_result), a
test_done:
    jp test_done

test_result: DEFB 255
head_seed: DEFB 0
count_seed: DEFB 0
cap_seed: DEFB 0
payload_seed: DEFS 48
payload2_seed: DEFS 48
spill_seed: DEFS 49
output_seed: DEFS 8
return_value: DEFB 0
output: DEFS 8
saved_sp: DEFW 0
saved_ix: DEFW 0
saved_iy: DEFW 0
_direct_rx_payload: DEFS 48
_direct_rx_payload2: DEFS 48
_direct_rx_link: DEFB 1
_direct_rx_link2: DEFB 2
_direct_rx_count: DEFB 0
_direct_rx_head: DEFB 0
_spectrum_overlay_context: DEFS 8
"""


def main() -> None:
    product = (ROOT / "asm/spectrum/shrink_kernels.asm").read_text(
        encoding="ascii"
    )
    routine = product[product.index("_netchesszx_asm_direct_pop:"):
                      product.index("netchesszx_asm_direct_pop_end:")]
    cases = (
        ("first", 0, 3, 8, b"ONE", 1, b"ONE\0", 1, 2),
        ("second-truncated", 1, 2, 3, b"THREE", 2, b"TH\0", 2, 1),
        ("spill-zero-cap", 2, 1, 0, b"LAST", 3, b"XXXXXXXX", 0, 0),
        ("spill-nul-only", 2, 1, 1, b"LAST", 3, b"\0XXXXXXX", 0, 0),
    )
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "direct_queue_pop.asm"
        harness.write_text(HARNESS + "\n" + routine, encoding="ascii")
        binary = temp / "direct_queue_pop.bin"
        symbols = assemble_vector(required_tool("z80asm"), harness, [], binary)
        address = lambda name: symbol_address(symbols, name)
        original = binary.read_bytes()

        for name, head, count, cap, payload, link, expected, next_head, left in cases:
            image = bytearray(original)

            def patch(label: str, value: bytes) -> None:
                offset = address(label) - 0x8000
                image[offset:offset + len(value)] = value

            slots = [bytearray(48), bytearray(48), bytearray(49)]
            slots[head][:len(payload)] = payload
            slots[head][len(payload)] = 0
            slots[2][48] = link
            patch("payload_seed", slots[0])
            patch("payload2_seed", slots[1])
            patch("spill_seed", slots[2])
            patch("output_seed", b"XXXXXXXX")
            patch("head_seed", bytes((head,)))
            patch("count_seed", bytes((count,)))
            patch("cap_seed", bytes((cap,)))
            binary.write_bytes(image)
            result, _, pc, _, _ = execute_vector(
                required_tool("z88dk-ticks"), binary, symbols, counter=100000
            )
            ram = binary.with_suffix(".ram").read_bytes()
            label = f"DIRECT resident pop {name}"
            assert result == 0 and pc == address("test_done"), label
            assert ram[address("return_value")] == link, label
            assert ram[address("output"):address("output") + 8] == expected.ljust(
                8, b"X"
            ), label
            assert ram[address("_direct_rx_head")] == next_head, label
            assert ram[address("_direct_rx_count")] == left, label
            for saved, expected_value in (
                ("saved_sp", 0xff00),
                ("saved_ix", 0x1234),
                ("saved_iy", 0x5c3a),
            ):
                got = int.from_bytes(
                    ram[address(saved):address(saved) + 2], "little"
                )
                assert got == expected_value, (label, saved, got)
    print(f"Resident DIRECT queue pop ASM vector passed ({len(cases)} cases)")


if __name__ == "__main__":
    main()
