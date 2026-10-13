import io
import json
import re
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

from tools import asm_data, gen_assets, make_about_board


ROOT = Path(__file__).resolve().parents[2]
UI = ROOT / "assets/spectrum/ui_runtime_assets.asm"
PIECES = ROOT / "assets/spectrum/chess_pieces_16x16.asm"
SCREEN = ROOT / "asm/spectrum/screen.asm"
ABOUT = ROOT / "assets/spectrum/about_board.bin"
ZX_LOADER = ROOT / "asm/esxdos/overlay_loader.asm"
NEXT_LOADER = ROOT / "asm/next/overlay_loader_next.asm"
ABOUT_ENTRY = ROOT / "asm/overlay/about/entry_about.asm"


def asm_equ(path, name, defines=frozenset()):
    from tools.check_lowmem_layout import parse_const
    return parse_const(path, name, defines)


def run_generator(loader, output, *, is_next, is_spectranext=False, version=None, ui_path=None):
    if version is None:
        version = (ROOT / "VERSION").read_text(encoding="ascii").strip()
    argv = [
        "gen_assets.py",
        str(ui_path or (ROOT / "assets/next/ui_runtime_assets.asm" if is_next else UI)),
        str(PIECES),
        str(ROOT / "asm/next/screen.asm" if is_next else SCREEN),
        str(loader),
        str(ABOUT),
        str(output),
        "--version",
        version,
    ]
    if is_next:
        argv.append("--next")
    if is_spectranext:
        argv.append("--spectranext")
    with redirect_stdout(io.StringIO()):
        gen_assets.main(argv)


class AboutStructuralTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = ABOUT.read_bytes()
        cls.payload = gen_assets.build_about_payload(cls.raw)
        pieces_text = PIECES.read_text(encoding="ascii")
        cls.pieces, _ = gen_assets.parse_defb(
            gen_assets.block_from_label(
                pieces_text, "netchesszx_piece_sprites_16x16"
            )
        )

    def test_payload_is_full_width_ula_band(self):
        self.assertEqual(len(self.raw), gen_assets.EXPECTED_ABOUT_BOARD_BYTES)
        self.assertEqual(gen_assets.unpack_about_payload(self.payload), self.raw)
        self.assertEqual(len(self.payload), 2858)
        packed_sizes = [
            int.from_bytes(self.payload[index : index + 2], "little")
            for index in range(0, gen_assets.ABOUT_DIRECTORY_BYTES, 2)
        ]
        self.assertEqual(
            packed_sizes,
            [120, 207, 235, 261, 192, 252, 279, 375, 383, 274, 83, 126, 45],
        )
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_row_bytes"), 32)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_chunk_bytes"), 448)
        self.assertEqual(
            asm_equ(ABOUT_ENTRY, "about_pixel_chunks")
            * asm_equ(ABOUT_ENTRY, "about_chunk_rows")
            + asm_equ(ABOUT_ENTRY, "about_tail_rows"),
            152,
        )
        self.assertEqual(
            asm_equ(ABOUT_ENTRY, "about_chunk_rows")
            + asm_equ(ABOUT_ENTRY, "about_attr_tail_rows"), 19)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_attr_tail_bytes"), 160)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_board_attr_top"), 3)

    def test_packed_chunks_fit_the_classic_in_place_workspace(self):
        input_start = asm_equ(ABOUT_ENTRY, "about_input")
        delta = asm_equ(ABOUT_ENTRY, "about_zx0_delta")
        directory_start = input_start + max(gen_assets.ABOUT_CHUNK_BYTES) + delta
        self.assertEqual(delta, gen_assets.ABOUT_ZX0_DELTA_LIMIT)
        self.assertEqual(
            directory_start - input_start,
            max(gen_assets.ABOUT_CHUNK_BYTES) + delta,
        )
        for index, raw_size in enumerate(gen_assets.ABOUT_CHUNK_BYTES):
            packed_size = int.from_bytes(
                self.payload[2 * index : 2 * index + 2], "little"
            )
            source_start = raw_size + delta - packed_size
            self.assertGreaterEqual(source_start, delta)
            self.assertEqual(source_start + packed_size, raw_size + delta)
        self.assertEqual(
            asm_equ(ABOUT_ENTRY, "about_input_size"),
            max(gen_assets.ABOUT_CHUNK_BYTES)
            + delta
            + gen_assets.ABOUT_DIRECTORY_BYTES,
        )

    def test_credit_line_layout(self):
        self.assertEqual(
            [line[2] for line in make_about_board.TEXT_LINES],
            [16, 32, 40, 48],
        )
        self.assertEqual(
            make_about_board.TEXT_LINES[1][0], "(C) 2026 M.I. MONGE"
        )
        self.assertEqual(
            make_about_board.HINT_LINE[::2],
            ("PRESS C FOR CHANGELOG, ANY TO CLOSE", 144),
        )

    def test_changelog_page_is_recognised_by_its_title_attribute(self):
        # app.c closes on C only when this screen cell shows the title
        # colour; About must never paint it there.
        header = (ROOT / "src/spectrum/ui/presentation_zx.h").read_text()
        page = (ROOT / "asm/overlay/menu_logic/changelog.asm").read_text()
        addr = int(re.search(r"CHLOG_TITLE_ATTR_ADDR (0x[0-9a-f]+)u", header)[1], 16)
        attr = int(re.search(r"CHLOG_TITLE_ATTR (0x[0-9a-f]+)u", header)[1], 16)
        equ = lambda name: int(
            re.search(rf"(?m)^{name} EQU (\w+)", page)[1], 0)
        self.assertEqual(equ("chlog_attr_title"), attr)
        self.assertEqual(
            0x5800 + equ("chlog_title_row") * 32 + equ("chlog_text_col") // 2,
            addr,
        )
        top = asm_equ(ABOUT_ENTRY, "about_board_attr_top")
        cell = gen_assets.ABOUT_PIXEL_BYTES + addr - 0x5800 - top * 32
        self.assertNotEqual(self.raw[cell], attr)

    def test_shared_defb_parser_preserves_data_offsets_and_errors(self):
        source = (
            "before:\n"
            "    DEFB 1\n"
            "start:\n"
            "    DEFB 0x02, \"AB\" ; comment\n"
            "middle:\n"
            "    defb 260\n"
            "end:\n"
            "    DEFB 9\n"
        )
        data, offsets = asm_data.parse_defb_with_offsets(source)
        self.assertEqual(data, b"\x01\x02AB\x04\x09")
        self.assertEqual(offsets, {"before": 0, "start": 1, "middle": 4, "end": 5})
        self.assertEqual(
            asm_data.parse_defb_block(source, "start", "end"),
            b"\x02AB\x04",
        )
        with self.assertRaisesRegex(SystemExit, "label not found: missing"):
            asm_data.label_block(source, "missing")
        with self.assertRaisesRegex(SystemExit, "end label not found: missing"):
            asm_data.label_block(source, "start", "missing")

    def test_committed_payload_matches_source_generator(self):
        self.assertEqual(
            make_about_board.build_board(
                ROOT / "assets/spectrum/about_classic.scr", UI
            ),
            self.raw,
        )

    def test_runtime_constants_match_payload(self):
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_payload_offset"), 2041)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_input_size"), 477)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_directory_bytes"), 26)
        spectranext = frozenset(("NETCHESSZX_SPECTRANEXT",))
        self.assertEqual(
            asm_equ(ABOUT_ENTRY, "about_payload_offset", spectranext), 1273
        )
        self.assertEqual(
            asm_equ(ABOUT_ENTRY, "about_input_size", spectranext), 448
        )
        self.assertEqual(gen_assets.ZX_EXTRA_PIECE_OFFSET, 1273)
        self.assertEqual(gen_assets.ABOUT_PAYLOAD_OFFSET, 2041)
        self.assertEqual(gen_assets.ABOUT_PAYLOAD_OFFSET + len(self.payload), 4899)
        self.assertEqual(gen_assets.NEXT_EXTRA_PIECE_OFFSET, 1273)
        self.assertEqual(gen_assets.EXPECTED_NEXT_DAT_BYTES, 2041)
        self.assertEqual(gen_assets.parse_loader_about_size(
            ZX_LOADER.read_text(encoding="ascii")), 5472)
        self.assertEqual(gen_assets.parse_loader_about_size(
            NEXT_LOADER.read_text(encoding="ascii")), 0)

    def test_classic_file_reads_share_one_reader(self):
        # Behaviour (one F_READ, one pump) is executed in
        # test_classic_overlay_dispatch.py; About must keep reusing it.
        loader = ZX_LOADER.read_text(encoding="ascii")
        about = ABOUT_ENTRY.read_text(encoding="ascii")

        self.assertEqual(loader.lower().count("defb 0x9d"), 1)
        reader = loader[loader.index("ovl_read_chunked:"):
                        loader.index("ovl_load_fail_close:")]
        self.assertIn("call _spectrum_uart_background_pump", reader)
        self.assertIn("EXTERN ovl_read_chunked", about)
        self.assertNotIn("defb 0x9d", about.lower())

    def test_next_ui_flag_does_not_omit_about_from_esxdos_dat(self):
        with tempfile.TemporaryDirectory() as temp:
            zx_out = Path(temp) / "zx.dat"
            next_out = Path(temp) / "next.dat"
            run_generator(ZX_LOADER, zx_out, is_next=True)
            run_generator(NEXT_LOADER, next_out, is_next=True)
            zx_dat = zx_out.read_bytes()
            next_dat = next_out.read_bytes()

        extras = self.pieces[gen_assets.RUNTIME_PIECE_BYTES :]
        self.assertEqual(len(zx_dat), 4899)
        self.assertEqual(
            zx_dat[
                gen_assets.ZX_EXTRA_PIECE_OFFSET : gen_assets.ABOUT_PAYLOAD_OFFSET
            ],
            extras,
        )
        self.assertEqual(
            gen_assets.unpack_about_payload(
                zx_dat[gen_assets.ABOUT_PAYLOAD_OFFSET :]
            ),
            self.raw,
        )
        self.assertEqual(len(next_dat), gen_assets.EXPECTED_NEXT_DAT_BYTES)
        self.assertEqual(next_dat[gen_assets.NEXT_EXTRA_PIECE_OFFSET :], extras)

    def test_version_is_consistent_across_products(self):
        version = (ROOT / "VERSION").read_text(encoding="ascii").strip()
        match = re.fullmatch(
            r"([0-9]+)\.([0-9]+)(?:\.([0-9]+))?(-dev(?:[0-9]{3}|ESP))?",
            version,
        )
        self.assertIsNotNone(match)
        launcher_version_core = f"{match[1]}.{match[2]}.{match[3] or '0'}"
        descriptor = json.loads(
            (ROOT / "zxespemu-launchers.json").read_text(encoding="utf-8")
        )
        self.assertEqual(
            descriptor["version"].split("-", 1)[0].split("+", 1)[0],
            launcher_version_core,
        )

        with tempfile.TemporaryDirectory() as temp:
            classic_out = Path(temp) / "classic.dat"
            next_out = Path(temp) / "next.dat"
            run_generator(ZX_LOADER, classic_out, is_next=False)
            run_generator(NEXT_LOADER, next_out, is_next=True)
            start = gen_assets.EXPECTED_UI_OFFSETS["version_banner_msg"]
            end = start + gen_assets.VERSION_SLOT
            classic_banner = classic_out.read_bytes()[start:end].rstrip(b"\0")
            next_banner = next_out.read_bytes()[start:end].rstrip(b"\0")

        expected_banner = (version if "-dev" in version else f"version {version}").encode(
            "ascii"
        )
        self.assertEqual(classic_banner, expected_banner)
        self.assertEqual(next_banner, expected_banner)

        source = (ROOT / "src/pc/client/main_window.cpp").read_text(encoding="utf-8")
        self.assertRegex(
            source,
            r'setWindowTitle\(QStringLiteral\("Shatranj %1"\)\.arg\(\s*'
            r"QString::fromLatin1\(kAppVersion\)\)\);",
        )
        self.assertNotRegex(source, r'setWindowTitle\([^;]*"Shatranj [0-9]')

    def test_platform_banner_and_board_palette_are_independent(self):
        with tempfile.TemporaryDirectory() as temp:
            classic_out = Path(temp) / "classic.dat"
            next_out = Path(temp) / "next.dat"
            spectranext_out = Path(temp) / "spectranext.dat"
            run_generator(ZX_LOADER, classic_out, is_next=False)
            run_generator(NEXT_LOADER, next_out, is_next=True)
            run_generator(
                ZX_LOADER,
                spectranext_out,
                is_next=False,
                is_spectranext=True,
            )

            start = gen_assets.EXPECTED_UI_OFFSETS["banner_info_top_msg"]
            end = start + gen_assets.BANNER_INFO_SLOT
            classic = classic_out.read_bytes()
            next_data = next_out.read_bytes()
            spectranext = spectranext_out.read_bytes()

        self.assertEqual(
            classic[start:end].rstrip(b"\0"), b"ONLINE CHESS FOR ZX SPECTRUM"
        )
        self.assertEqual(
            next_data[start:end].rstrip(b"\0"), b"ONLINE CHESS FOR SPECTRUM NEXT"
        )
        self.assertEqual(
            spectranext[start:end].rstrip(b"\0"),
            b"ONLINE CHESS FOR SPECTRANEXT",
        )
        extras = self.pieces[gen_assets.RUNTIME_PIECE_BYTES :]
        self.assertEqual(len(spectranext), 7513)
        self.assertEqual(
            spectranext[
                gen_assets.SPECTRANEXT_ABOUT_PAYLOAD_OFFSET :
                gen_assets.SPECTRANEXT_EXTRA_PIECE_OFFSET
            ],
            self.raw,
        )
        self.assertEqual(
            spectranext[gen_assets.SPECTRANEXT_EXTRA_PIECE_OFFSET :], extras
        )
        for name in (
            "board_theme_mark_inks",
            "board_theme_light_attrs",
            "board_theme_dark_attrs",
        ):
            offset = gen_assets.EXPECTED_UI_OFFSETS[name]
            self.assertEqual(
                spectranext[offset : offset + 5], classic[offset : offset + 5]
            )
        expected = bytearray(classic[:gen_assets.EXPECTED_UI_BYTES])
        gen_assets.patch_slot(expected, start, gen_assets.BANNER_INFO_SLOT,
                              "ONLINE CHESS FOR SPECTRUM NEXT", "banner")
        for name, values in (
            ("board_theme_mark_inks", [2, 2, 1, 0, 0]),
            ("board_theme_light_attrs", [0x78, 0x6f, 0x66, 0x77, 0x37]),
            ("board_theme_dark_attrs", [0x07, 0x4d, 0x20, 0x56, 0x52]),
        ):
            offset = gen_assets.EXPECTED_UI_OFFSETS[name]
            expected[offset:offset + 5] = bytes(values)
        self.assertEqual(next_data[:len(expected)], expected)

    def test_next_theme_wrong_width_is_rejected_before_output(self):
        with tempfile.TemporaryDirectory() as temp:
            theme = Path(temp) / "theme.asm"
            output = Path(temp) / "next.dat"
            theme.write_text(gen_assets.NEXT_UI_THEME.read_text().replace(
                "DEFB 2, 2, 1, 0, 0", "DEFB 2, 2, 1, 0"), encoding="ascii")
            with patch.object(gen_assets, "NEXT_UI_THEME", theme):
                with self.assertRaisesRegex(SystemExit, "must contain 5 bytes"):
                    run_generator(NEXT_LOADER, output, is_next=True)
            self.assertFalse(output.exists())

    def test_next_font_can_change_without_changing_classic_dat(self):
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            classic, before, after = (temp / n for n in ("zx.dat", "before.dat", "after.dat"))
            run_generator(ZX_LOADER, classic, is_next=False)
            original_classic = classic.read_bytes()
            run_generator(NEXT_LOADER, before, is_next=True)
            source = (ROOT / "assets/next/ui_runtime_assets.asm").read_text()
            start = source.index("font_packed:")
            match = re.search(r"0x[0-9a-fA-F]+", source[start:])
            pos = start + match.start()
            source = source[:pos] + hex(int(match[0], 16) ^ 1) + source[pos + len(match[0]):]
            font = temp / "ui.asm"
            font.write_text(source, encoding="ascii")
            run_generator(NEXT_LOADER, after, is_next=True, ui_path=font)
            changed = [i for i, pair in enumerate(zip(before.read_bytes(), after.read_bytes()))
                       if pair[0] != pair[1]]
            self.assertEqual(changed, [gen_assets.EXPECTED_UI_OFFSETS["font_packed"]])
            run_generator(ZX_LOADER, classic, is_next=False)
            self.assertEqual(classic.read_bytes(), original_classic)

    def test_development_version_keeps_suffix_in_banner(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "dev.dat"
            run_generator(
                ZX_LOADER, output, is_next=False, version="1.1.0-dev001"
            )
            start = gen_assets.EXPECTED_UI_OFFSETS["version_banner_msg"]
            banner = output.read_bytes()[start : start + gen_assets.VERSION_SLOT]

        self.assertEqual(banner.rstrip(b"\0"), b"1.1.0-dev001")

    def test_esp_development_version_keeps_suffix_in_banner(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "dev-esp.dat"
            run_generator(
                ZX_LOADER, output, is_next=False, version="1.1.0-devESP"
            )
            start = gen_assets.EXPECTED_UI_OFFSETS["version_banner_msg"]
            banner = output.read_bytes()[start : start + gen_assets.VERSION_SLOT]

        self.assertEqual(banner.rstrip(b"\0"), b"1.1.0-devESP")

    def test_invalid_product_version_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "invalid.dat"
            with self.assertRaisesRegex(SystemExit, "version must contain"):
                run_generator(
                    ZX_LOADER, output, is_next=False, version="build-dev001"
                )

    def test_unknown_about_layout_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            loader = Path(temp) / "loader.asm"
            output = Path(temp) / "bad.dat"
            loader.write_text(
                "asset_load_size EQU 1273\nabout_board_size EQU 1361\n",
                encoding="ascii",
            )
            with self.assertRaisesRegex(SystemExit, "expected 5472 or 0"):
                run_generator(loader, output, is_next=False)


if __name__ == "__main__":
    unittest.main()
