"""Exercise the linked BOARD entry, including its real 16-bit context decoder.

No relink: pass a complete Classic or Next build directory. Paging/interrupts
are outside this test; the target's BOARD page is already mapped in the image.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

from test_spectrum_asm_vectors import required_tool, symbol_address

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("--next", action="store_true")
args = parser.parse_args()
mapping = (args.build / "SHATRANJ.map").read_text()
code = (args.build / "SHATRANJ_CODE.bin").read_bytes()
overlay = (args.build / "SHATRANJ_BOARD.OVL").read_bytes()
base = 0x6000 if args.next else 0x6800
assert overlay[0] == 4
entry = int.from_bytes(overlay[1:3], "little")
assert base <= entry < base + len(overlay)
state_addresses = [symbol_address(mapping, name) for name in
                   ("_side_to_move", "_castle_rights", "_ep_square")]
pieces = "PNBRQKpnbrqk"


def rules(cells):
    return bytes(0 if c == ord('.') else
                 (pieces.index(chr(c)) + 1 if chr(c).isupper() else
                  -(pieces.index(chr(c)) - 5)) & 255 for c in cells)


with tempfile.TemporaryDirectory(prefix="board-action-z80-") as directory:
    source = Path(directory) / "input.bin"
    output = Path(directory) / "output.ram"
    cases = [(suffix, False, False) for suffix in range(256)]
    cases += [(ord(p), True, False) for p in "qrbnQRBN"]
    cases += [(ord('q'), False, True)]
    for suffix, black, trailing in cases:
        image = bytearray(65536)
        origin = symbol_address(mapping, "__CODE_head")
        image[origin:origin + len(code)] = code
        image[base:base + len(overlay)] = overlay
        cells = bytearray(b'.' * 64)
        start, end = (54, 61) if black else (9, 2)
        cells[start] = ord('p' if black else 'P')
        cells[end] = ord('B' if black else 'b')
        cells[4], cells[60] = ord('k'), ord('K')
        image[0x5f60:0x5fa0] = cells
        image[0x5fa0:0x5fe0] = rules(cells)
        before_state = bytes([black, 15, 255])
        for address, value in zip(state_addresses, before_state):
            image[address] = value
        token = b'g2f1' if black else b'b7c8'
        image[0x5e20:0x5e26] = token + bytes([suffix, 120 if trailing else 0])
        image[0x5e00:0x5e08] = bytes([0x20, 0x5e, start // 8, start % 8,
                                     end // 8, end % 8, 0x40, 0x5e])
        image[0x5e40:0x5e45] = b'\xa5' * 5
        # z88dk_fastcall context in HL; save uint8 result L and final SP.
        harness = (b'\x31\x58\xff\x21\x00\x5e\xcd' + entry.to_bytes(2, 'little') +
                   b'\x7d\x32\x80\x5e\xed\x73\x81\x5e')
        done = 0x5000 + len(harness)
        image[0x5000:done + 1] = harness + b'\x76'
        source.write_bytes(image)
        subprocess.run([required_tool('z88dk-ticks'), '-mz80', '-l', '0',
                        '-pc', '5000', '-end', f'{done:x}', '-counter', '1000000',
                        '-output', str(output), str(source)],
                       capture_output=True, check=True)
        ram = output.read_bytes()
        assert int.from_bytes(ram[0x10006:0x10008], 'little') == done
        accepted = suffix in b'qrbnQRBN' and not trailing
        assert ram[0x5e80] == accepted, (suffix, black, trailing)
        assert ram[0x5e81:0x5e83] == b'\x58\xff', 'unbalanced stack'
        expected = cells.copy()
        expected_state = before_state
        expected_undo = b'\xa5' * 5
        if accepted:
            expected[start] = ord('.')
            expected[end] = ord(chr(suffix).lower() if black else chr(suffix).upper())
            expected_state = bytes([not black, 15, 255])
            expected_undo = bytes([start | 128, end, cells[end], 15, 255])
        assert ram[0x5f60:0x5fa0] == expected
        assert ram[0x5fa0:0x5fe0] == rules(expected)
        assert bytes(ram[a] for a in state_addresses) == expected_state
        assert ram[0x5e40:0x5e45] == expected_undo
        assert ram[0x5e00:0x5e08] == image[0x5e00:0x5e08], 'context changed'
        assert ram[0x5e20:0x5e26] == image[0x5e20:0x5e26], 'borrowed text changed'
    print(f"BOARD {'Next' if args.next else 'Classic'}: {len(cases)} real-entry cases passed; semantic state, rejection, undo and SP")
