import io
import json
import re
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from tools import asm_data, gen_assets, make_about_board


ROOT = Path(__file__).resolve().parents[2]
UI = ROOT / "assets/spectrum/ui_runtime_assets.asm"
PIECES = ROOT / "assets/spectrum/chess_pieces_16x16.asm"
SCREEN = ROOT / "asm/spectrum/screen.asm"
ABOUT = ROOT / "assets/spectrum/about_board.bin"
ZX_LOADER = ROOT / "asm/esxdos/overlay_loader.asm"
NEXT_LOADER = ROOT / "asm/next/overlay_loader_next.asm"
ABOUT_ENTRY = ROOT / "asm/overlay/about/entry_about.asm"


def asm_equ(path, name):
    text = path.read_text(encoding="ascii")
    match = re.search(rf"(?m)^{name}\s+EQU\s+([0-9]+)\s*$", text)
    if not match:
        raise AssertionError(f"{name} not found in {path}")
    return int(match.group(1), 10)


def run_generator(loader, output, *, is_next, is_spectranext=False, version=None):
    if version is None:
        version = (ROOT / "VERSION").read_text(encoding="ascii").strip()
    argv = [
        "gen_assets.py",
        str(UI),
        str(PIECES),
        str(SCREEN),
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
        self.assertEqual(self.payload, self.raw)
        self.assertEqual(len(self.payload), 5184)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_row_bytes"), 32)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_chunk_bytes"), 448)
        self.assertEqual(
            asm_equ(ABOUT_ENTRY, "about_pixel_chunks")
            * asm_equ(ABOUT_ENTRY, "about_chunk_rows")
            + asm_equ(ABOUT_ENTRY, "about_tail_rows"),
            144,
        )

    def test_credit_line_layout(self):
        self.assertEqual(
            [line[2] for line in make_about_board.TEXT_LINES],
            [16, 32, 40, 48, 56],
        )
        self.assertEqual(
            make_about_board.TEXT_LINES[2][0], "(C) 2026 M.I. MONGE"
        )

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
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_payload_offset"), 1196)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_payload_size"), 5184)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_input_size"), 448)
        self.assertEqual(gen_assets.ZX_EXTRA_PIECE_OFFSET, 6380)
        self.assertEqual(gen_assets.EXPECTED_ZX_DAT_BYTES, 7148)
        self.assertEqual(gen_assets.NEXT_EXTRA_PIECE_OFFSET, 1196)
        self.assertEqual(gen_assets.EXPECTED_NEXT_DAT_BYTES, 1964)
        self.assertEqual(gen_assets.parse_loader_about_size(
            ZX_LOADER.read_text(encoding="ascii")), 5184)
        self.assertEqual(gen_assets.parse_loader_about_size(
            NEXT_LOADER.read_text(encoding="ascii")), 0)

    def test_classic_file_reads_use_chunked_uart_pump(self):
        loader = ZX_LOADER.read_text(encoding="ascii")
        about = ABOUT_ENTRY.read_text(encoding="ascii")

        self.assertEqual(loader.lower().count("defb 0x9d"), 1)
        self.assertIn("call _spectrum_uart_background_pump", loader)
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
        self.assertEqual(len(zx_dat), gen_assets.EXPECTED_ZX_DAT_BYTES)
        self.assertEqual(
            zx_dat[gen_assets.ABOUT_PAYLOAD_OFFSET : gen_assets.ZX_EXTRA_PIECE_OFFSET],
            self.payload,
        )
        self.assertEqual(zx_dat[gen_assets.ZX_EXTRA_PIECE_OFFSET :], extras)
        self.assertEqual(len(next_dat), gen_assets.EXPECTED_NEXT_DAT_BYTES)
        self.assertEqual(next_dat[gen_assets.NEXT_EXTRA_PIECE_OFFSET :], extras)

    def test_version_is_consistent_across_products(self):
        version = (ROOT / "VERSION").read_text(encoding="ascii").strip()
        match = re.fullmatch(
            r"([0-9]+)\.([0-9]+)(?:\.([0-9]+))?(-dev(?:[0-9]{3}|ESP))?",
            version,
        )
        self.assertIsNotNone(match)
        launcher_version = f"{match[1]}.{match[2]}.{match[3] or '0'}{match[4] or ''}"
        descriptor = json.loads(
            (ROOT / "zxespemu-launchers.json").read_text(encoding="utf-8")
        )
        self.assertEqual(descriptor["version"], launcher_version)

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
        for name in (
            "board_theme_mark_inks",
            "board_theme_light_attrs",
            "board_theme_dark_attrs",
        ):
            offset = gen_assets.EXPECTED_UI_OFFSETS[name]
            self.assertEqual(
                spectranext[offset : offset + 5], classic[offset : offset + 5]
            )

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
                "asset_load_size EQU 1196\nabout_board_size EQU 1361\n",
                encoding="ascii",
            )
            with self.assertRaisesRegex(SystemExit, "expected 5184 or 0"):
                run_generator(loader, output, is_next=False)


if __name__ == "__main__":
    unittest.main()
