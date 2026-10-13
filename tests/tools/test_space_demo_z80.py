"""Run the built demo's real Z80 model/cursor/renderer; no hardware claim."""
from pathlib import Path
import subprocess

from test_spectrum_asm_vectors import required_tool, symbol_address

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build/space-demo"
mapping = (BUILD / "SPACEDEMO.map").read_text()
code = (BUILD / "SPACEDEMO_CODE.bin").read_bytes()
assert symbol_address(mapping, "__CODE_head") == 0x8000
assert symbol_address(mapping, "__BSS_END_tail") < 0xc000
assert not any(name in mapping for name in
               ("_spectrum_board_", "_netchesszx_session_", "_spectrum_net_"))


def call(name):
    return b"\xcd" + symbol_address(mapping, name).to_bytes(2, "little")


def model_call(name, argument, player, result):
    # SDCC stack arguments: byte player, then word cell/action pointer.
    return (b"\x3e" + bytes([player]) + b"\xf5\x33\x21" +
            argument.to_bytes(2, "little") + b"\xe5" + call(name) +
            b"\xc1\x33\x7d\x32" + result.to_bytes(2, "little"))


def pixel(ram, x, y):
    address = 0x4000 | ((y & 0xc0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2) | (x >> 3)
    return bool(ram[address] & (0x80 >> (x & 7)))


for mode in (0, 1):
    image = bytearray(65536)
    image[0x8000:0x8000 + len(code)] = code
    selected = 3 if mode else 1  # hidden from participant zero before activation
    image[0x6100:0x6102] = selected.to_bytes(2, "little")
    image[0x6102:0x6104] = (24 if mode else 120).to_bytes(2, "little")
    image[0x6104:0x6106] = b"\xff\xff"
    actions = model_call("_demo_view", selected, 0, 0x6200)
    actions += model_call("_demo_activate", selected, 0, 0x6201)
    actions += model_call("_demo_view", selected, 0, 0x6202)
    actions += model_call("_demo_apply_action", 0x6100, 1, 0x6203)
    for index, (pointer, participant) in enumerate(((0, 0), (0x6102, 0),
                                                  (0x6104, 0), (0x6100, 2))):
        actions += model_call("_demo_apply_action", pointer, participant, 0x6204 + index)
    # Match the emitted main's SDCC byte argument (PUSH AF / INC SP), then
    # call public reset/home operations and the actual private renderer.
    harness = (b"\x31\x00\xff\x3e" + bytes([mode]) + b"\xf5\x33" +
               call("_demo_reset") + b"\x33" + call("_demo_first") +
               call("_spectrum_cell_cursor_home") + actions + call("_redraw"))
    done = 0x7000 + len(harness)
    image[0x7000:done + 1] = harness + b"\x76"
    source = BUILD / f"geometry-{mode}.bin"
    output = BUILD / f"geometry-{mode}.ram"
    source.write_bytes(image)
    result = subprocess.run([required_tool("z88dk-ticks"), "-mz80", "-l", "0",
        "-pc", "7000", "-end", f"{done:04x}", "-counter", "20000000",
        "-output", str(output), str(source)], capture_output=True, text=True, check=True)
    ram = output.read_bytes()
    assert int.from_bytes(ram[0x10006:0x10008], "little") == done
    assert ram[0x6200:0x6208] == bytes([0, 1, 1 + selected % 3, 1, 0, 0, 0, 0])
    revealed = symbol_address(mapping, "_revealed")
    expected = bytearray(120)
    expected[selected] = 3
    assert ram[revealed:revealed + 120] == expected, "partial mutation or wrong participant"
    cells = 0
    for row in range(7 if mode else 10):
        for col in range(7 if mode else 12):
            if mode and (not 3 <= row + col <= 9 or (row, col) == (3, 3)):
                continue
            x = 8 + col * 16 + (row * 8 if mode else 0)
            y = 16 + row * (12 if mode else 16)
            assert pixel(ram, x + 7, y), (mode, row, col)
            cells += 1
    assert cells == (36 if mode else 120)
    if mode:
        assert not pixel(ram, 88, 60), "center hole was painted"
    assert ram[0x6000:0x6200] == image[0x6000:0x6200]
    assert ram[0x6208:0x7000] == image[0x6208:0x7000]
    assert ram[0xc000:0xe000] == image[0xc000:0xe000]
    print(f"Z80 geometry {mode}: actions/rejections, {cells} cells, return and RAM guards passed; {result.stdout.strip()} ticks")
