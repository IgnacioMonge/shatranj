#!/usr/bin/env python3
"""Static integration checks for the consumer-owned SpectraNext build seam."""

import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.gen_overlay_atlas import ORDER  # noqa: E402


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


class SpectraNextBuildTests(unittest.TestCase):
    def test_overlay_block_size_is_target_selectable(self):
        atlas = ROOT / "tools" / "gen_overlay_atlas.py"
        command = [
            sys.executable,
            str(atlas),
            "--build-dir",
            "build",
            "--name",
            "SHATRANJ",
            "--out",
            "atlas.OVL",
        ]

        def environment(cap=None):
            env = os.environ.copy()
            if cap is None:
                env.pop("OVERLAY_BLOCK_SIZE", None)
            else:
                env["OVERLAY_BLOCK_SIZE"] = str(cap)
            return env

        def builder_size(cap=None):
            result = subprocess.run(
                [
                    sys.executable,
                    "-c",
                    "from tools.build_overlays import BLOCK_SIZE; print(BLOCK_SIZE)",
                ],
                cwd=ROOT,
                env=environment(cap),
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            return result.stdout.strip()

        with tempfile.TemporaryDirectory() as tmp_name:
            root = Path(tmp_name)
            build = root / "build"
            build.mkdir()

            def run(size, cap=None):
                for name in ORDER:
                    (build / f"SHATRANJ_{name}.OVL").write_bytes(b"x" * size)
                return subprocess.run(
                    command,
                    cwd=root,
                    env=environment(cap),
                    capture_output=True,
                    text=True,
                    check=False,
                )

            self.assertEqual(builder_size(), "2048")
            self.assertNotEqual(run(2049).returncode, 0)
            self.assertEqual(builder_size(4096), "4096")
            accepted = run(2049, 4096)
            self.assertEqual(accepted.returncode, 0, accepted.stderr)
            self.assertNotEqual(run(4097, 4096).returncode, 0)

    def test_current_backend_composition(self):
        makefile = source("Makefile")

        for token in (
            "$(SPXN_DIR_ABS)/spxn.c",
            "$(SPXN_DIR_ABS)/spxresolve.c",
            "$(SPXN_DIR_ABS)/spxn_stream.c",
            "$(SPXN_DIR_ABS)/spxn_rom.asm",
            "$(SPXN_DIR_ABS)/spxudp.c",
            "$(SPXN_DIR_ABS)/spxtime.c",
            "$(SPXN_DIR_ABS)/spxudp.h",
            "$(SPXN_DIR_ABS)/spxtime.h",
            "$(SPXN_DIR_ABS)/spxn_rom.h",
            "$(SPXN_DIR_ABS)/spxn.h",
            "$(SPXN_DIR_ABS)/adapters/xfs_compat.asm",
            "NET_BACKEND=spectranext",
            "FS_BACKEND=xfs",
            "UART_BACKEND=none",
            "DAT_PLATFORM_FLAG := --spectranext",
            "RELEASE_DIR=$(RELEASE_DIR)/Spectranext",
        ):
            self.assertIn(token, makefile)
        resident_c = re.search(
            r"SPXN_RESIDENT_C := (?P<body>.*?)(?:\n[^ \t]|\Z)",
            makefile,
            re.DOTALL,
        ).group("body")
        resident_asm = re.search(
            r"SPXN_RESIDENT_ASM := (?P<body>.*?)(?:\n[^ \t]|\Z)",
            makefile,
            re.DOTALL,
        ).group("body")
        self.assertNotIn("spxresolve.c", resident_c)
        self.assertNotIn("xfs_compat.asm", resident_asm)
        self.assertIn("xfs_loader_spectranext.asm", resident_asm)
        self.assertIn("SPXN_RESOLVE_C", source("tools/build_overlays.py"))
        self.assertIn("SPXN_XFS_OVERLAY_ASM", source("tools/build_overlays.py"))
        self.assertNotIn("netchesszx_xfs.asm", makefile)
        self.assertNotIn("gen_spectranext_installer.py", makefile)
        tap_recipe = makefile[
            makefile.index("tap-spectranext:"):
            makefile.index("spectranext-size-report:")
        ]
        self.assertNotIn("max-allocs-per-node", tap_recipe)
        self.assertNotIn("max-allocs-per-node", makefile)

    def test_pageb_overlay_backend_contract(self):
        makefile = source("Makefile")
        loader = source("asm/esxdos/overlay_loader.asm").lower()
        platform = source("src/spectrum/platform/platform.c").lower()
        time_config = source("asm/overlay/time_config/entry_time_config.asm").lower()

        self.assertIn("overlay_block_size=4096", makefile.lower())
        self.assertIn("overlay_size_limit=4096", makefile.lower())
        self.assertIn("-dspxn_rom_held", makefile.lower())
        self.assertIn("-dspxn_rom_held_external", makefile.lower())
        for token in (
            "_overlay_code_slot equ 0x2000",
            "_spxn_overlay_page_table equ 0x6800",
            "org 0x6e00",
            "spxn_overlay_stage equ 0x6820",
            "spxn_overlay_chunk equ 1502",
            "spxn_overlay_chunk_size equ spxn_overlay_stage + spxn_overlay_chunk",
            "call spxn_reserve_page",
            "call ovl_read_chunked",
            "call spxn_set_page_b",
            "call spxn_push_page_b",
            "call spxn_pop_page_b",
            "call spxn_free_page",
            "ld bc, ovl_spxn_return",
            "time_config_probe_key equ 0xff",
        ):
            self.assertIn(token, loader)
        self.assertNotIn("spxn_service", loader)
        self.assertIn("_spxn_rom_held equ ovl_atlas_file_fingerprint + 3", loader)
        self.assertIn("call _spxn_xfs_init", loader)
        self.assertIn("--low-code-bin", makefile.lower())
        self.assertIn("--clearaddr 28159", makefile.lower())
        self.assertNotRegex(loader, r"\bout\s*\([^)]*0[013]3b")
        self.assertRegex(loader, r"ovl_spxn_map:[\s\S]*?call spxn_pagein[\s\S]*?call spxn_push_page_b")
        self.assertRegex(loader, r"ovl_spxn_return:[\s\S]*?call spxn_pop_page_b[\s\S]*?call spxn_pageout")
        self.assertIn(
            "ld (_spxn_rom_held), a\novl_spxn_frame_done:", loader
        )
        return_block = loader[loader.index("ovl_spxn_return:"):
                              loader.index("_spectrum_spxn_frame_wait:")]
        self.assertIn("ld (ovl_spxn_return_value), hl", return_block)
        self.assertIn("ld hl, (ovl_spxn_return_value)", return_block)
        self.assertNotIn("ld h, 0", return_block)
        self.assertIn("spectrum_spxn_frame_wait();", platform)
        self.assertLess(
            loader.index("ld a, (_spxn_rom_held)"),
            loader.index("ovl_args_canonical:"),
        )
        self.assertIn(
            "call spxn_pagein\n    ld a, (ovl_spxn_page_count)\n    ld b, a",
            loader,
        )
        probe = re.search(
            r"ifdef netchesszx_spectranext\s+.*?tc_probe:\s+ld l, 0\s+ret\s+else",
            time_config,
            re.DOTALL,
        )
        self.assertIsNotNone(probe)

    def test_manifest_routes_current_driver(self):
        manifest = json.loads(source("packaging/spectranext/port.json"))

        self.assertEqual(manifest["schema"], 2)
        self.assertEqual(manifest["product"]["stem"], "SHATRANJ")
        self.assertEqual(
            manifest["checks"]["spectranext"],
            [["make", "tap-spectranext", "SPXN_DIR={spectranext}/driver"]],
        )
        self.assertTrue(all(manifest["capabilities"].values()))

    def test_spectranext_clock_startup_contract(self):
        app = source("src/spectrum/app/app.c")
        makefile = source("Makefile")

        clock_ui = re.search(
            r"#ifndef NETCHESSZX_SPECTRANEXT\s+"
            r"static void preflight_clock_line.*?CLOCK WAIT.*?"
            r"CLOCK OK.*?CLOCK FAIL.*?#endif",
            app,
            re.DOTALL,
        )
        self.assertIsNotNone(clock_ui)

        connection_start = re.search(
            r"static\s+uint8_t\s+connection_preflight_run\s*\(", app
        )
        self.assertIsNotNone(connection_start)
        connection_end = re.search(
            r"static\s+void\s+status_show_phase\s*\(",
            app[connection_start.start() :],
        )
        self.assertIsNotNone(connection_end)
        connection = app[
            connection_start.start() : connection_start.start() + connection_end.start()
        ]
        self.assertRegex(connection, r"if\s*\(\s*!quiet\s*&&")
        self.assertRegex(connection, r"\bclock_sync_run\s*\(")
        self.assertRegex(
            connection,
            r"#ifdef NETCHESSZX_SPECTRANEXT\s+"
            r"\(void\)clock_sync_run\(\);\s+#else\s+"
            r"preflight_clock_run\(\);\s+#endif",
        )

        main = app[app.index("int main(void)") :]
        setup = re.search(
            r"config_state\s*=\s*session_setup_run\s*\(\s*config_state\s*\)\s*;",
            main,
        )
        preflight_call = re.search(
            r"while\s*\(\s*!\s*connection_preflight_run\s*\(\s*warm_restart\s*\)\s*\)",
            main,
        )
        self.assertIsNotNone(setup)
        self.assertIsNotNone(preflight_call)
        self.assertLess(preflight_call.start(), setup.start())

        after_setup = main[setup.end() :]
        before_setup = main[: setup.start()]
        self.assertNotIn("clock_sync_run", after_setup)
        self.assertIn("spectrum_link_clock_retry_start();", before_setup)
        self.assertIn("spectrum_link_clock_retry_cancel();", after_setup)
        self.assertNotIn("netchesszx_timezone != warm_restart", after_setup)

        self.assertRegex(
            makefile,
            r"(?m)^\s*TIME_HOST\s*\?=\s*time\.google\.com\s*$",
        )
        spectranext_flags = re.search(
            r"ifeq\s*\(\s*\$\(NET_BACKEND\)\s*,\s*spectranext\s*\)"
            r".*?^\s*ZX_TARGET_CFLAGS\s*\+=.*?^\s*endif\s*$",
            makefile,
            re.DOTALL | re.MULTILINE,
        )
        self.assertIsNotNone(spectranext_flags)
        self.assertIn("-DNETCHESSZX_TZ=0", spectranext_flags.group(0))
        self.assertEqual(makefile.count("-DNETCHESSZX_TZ=0"), 1)

    def test_connection_setup_restores_board_before_side_panels(self):
        app = source("src/spectrum/app/app.c")
        main = app[app.index("int main(void)") :]
        setup_match = re.search(
            r"connection_setup:\s*(?P<body>.*?)"
            r"(?=\bif\s*\(\s*netchesszx_transport_is_mqtt\s*\(\s*\)\s*\))",
            main,
            re.DOTALL,
        )
        self.assertIsNotNone(setup_match)
        setup = setup_match.group("body")

        session = re.search(
            r"\bconfig_state\s*=\s*session_setup_run\s*"
            r"\(\s*config_state\s*\)\s*;",
            setup,
        )
        self.assertIsNotNone(session)
        after_session = setup[session.end() :]

        board = re.search(
            r"\bspectrum_board_reset\s*\(\s*\)\s*;.*?"
            r"\bspectrum_gui_set_board_snapshot\s*\(\s*"
            r"spectrum_board_cells\s*\(\s*\)\s*\)\s*;",
            after_session,
            re.DOTALL,
        )
        self.assertIsNotNone(board)
        after_board = after_session[board.end() :]

        restore_board = re.search(
            r"\bspectrum_gui_restore_board_area\s*\(\s*\)\s*;",
            after_board,
        )
        restore_side = re.search(
            r"\bspectrum_gui_restore_side_panels\s*\(\s*\)\s*;",
            after_board,
        )
        self.assertIsNotNone(restore_board)
        self.assertIsNotNone(restore_side)
        self.assertLess(restore_board.start(), restore_side.start())

    def test_installer_output_is_replaced_only_after_staging_succeeds(self):
        makefile = source("Makefile")
        target = re.search(
            r"(?ms)^spectranext-port-build:.*?\n(?P<body>(?:\t.*\n)+)",
            makefile,
        )
        self.assertIsNotNone(target)
        body = target.group("body")
        self.assertIn('stage="$$out.stage"', body)
        self.assertIn('previous="$$out.previous"', body)
        self.assertLess(body.index('installer \\'), body.index('mv "$$out" "$$previous"'))
        self.assertIn('if mv "$$stage" "$$out"', body)
        self.assertIn('mv "$$previous" "$$out"', body)


if __name__ == "__main__":
    unittest.main()
