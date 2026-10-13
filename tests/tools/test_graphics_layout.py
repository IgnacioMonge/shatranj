"""Target composition and the C/ASM layout authority."""
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from asm_source import read_asm
from gen_ui_layout import layout_asm, layout_values


def main():
    classic = ROOT / "asm/spectrum/screen.asm"
    next_root = ROOT / "asm/next/screen.asm"
    assert 'asm/spectrum/screen.asm' not in next_root.read_text()
    assert 'asm/spectrum/screen.asm' not in (ROOT / "asm/next/extension_bank_next.asm").read_text()
    assert 'asm/next/' not in read_asm(classic)
    original = read_asm(classic)
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        (temp / "log_contract.h").write_text((ROOT / "src/spectrum/ui/log_contract.h").read_text())
        layout = temp / "layout_next.h"
        source = (ROOT / "src/spectrum/ui/layout_next.h").read_text()
        layout.write_text(source.replace('NETCHESSZX_INFO_PANEL_COL 18u', 'NETCHESSZX_INFO_PANEL_COL 19u'))
        assert layout_values(layout)['NETCHESSZX_INFO_PANEL_COL'] == '19'
        assert 'NETCHESSZX_INFO_PANEL_COL EQU 19' in read_asm(next_root, layout=layout)
        assert read_asm(classic) == original
        assert 'NETCHESSZX_MOVE_SLOT_SIZE EQU 32' in layout_asm(layout)
        layout.write_text(source + '\n#define INVALID call()!\n')
        try:
            layout_asm(layout)
        except ValueError:
            pass
        else:
            raise AssertionError('invalid assembler expression accepted')

    # Each resident stub must address the jump-table slot for its own entry.
    bank = (ROOT / "asm/next/extension_bank_next.asm").read_text()
    table = re.findall(r'^\s+jp (\w+)', bank, re.M)
    equs = (ROOT / "asm/next/extension_bank_layout.asm").read_text()
    slots = {name: int(offset or 0) // 3 for name, offset in re.findall(
        r'^(next_ext_\w+)\s+EQU next_extension_org(?: \+ (\d+))?$', equs, re.M)}
    stubs = ''.join((ROOT / f'asm/next/screen_extension_stubs_{part}.asm').read_text()
                    for part in ('top', 'tail'))
    pairs = re.findall(r'^(\w+):\s+call next_extension_restore\s+jp (next_ext_\w+)', stubs, re.M)
    assert len(table) == len(slots) == 49
    retired = {'next_ext_render_game_timer_char', 'next_ext_render_menu_timer_char'}
    assert len(pairs) == 47
    assert {target for _, target in pairs} == set(slots) - retired
    for target in retired:
        assert table[slots[target]] == 'next_retired_timer_char'
    for entry, target in pairs:
        assert table[slots[target]] == entry, (entry, target)
    print('Graphics layout ownership and 49 extension entries OK')


if __name__ == '__main__':
    main()
