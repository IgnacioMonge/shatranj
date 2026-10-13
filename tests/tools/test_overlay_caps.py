#!/usr/bin/env python3
"""Negative checks for overlay resident-symbol capabilities."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BANNED = (
    "spectrum_board_apply_trusted_move",
    "spectrum_gui_add_chat",
    "spectrum_gui_add_move",
    "netchesszx_session_format_busy",
)


def run_checker(fixture: Path, policy: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(ROOT / "tools" / "check_overlay_caps.py"),
            "--root",
            str(fixture),
            "--policy",
            str(policy),
        ],
        capture_output=True,
        text=True,
        check=False,
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        fixture = Path(tmp)
        session = fixture / "src" / "spectrum" / "session"
        session.mkdir(parents=True)
        connect = session / "mqtt_connect_actions_ovl.c"
        tx = session / "mqtt_tx_actions_ovl.c"
        connect.write_text("spectrum_append_text\n", encoding="utf-8")
        tx.write_text("spectrum_net_mqtt_presence_payload\n", encoding="utf-8")
        policy = fixture / "policy.json"
        policy.write_text(
            json.dumps(
                {
                    "overlays": {
                        "mqtt_connect": {"allowed_caps": ["text"]},
                        "mqtt_tx": {"allowed_caps": ["net"]},
                    }
                }
            ),
            encoding="utf-8",
        )

        result = run_checker(fixture, policy)
        assert result.returncode == 0, result.stderr

        setup = fixture / "src" / "spectrum" / "overlay" / "setup_ovl.c"
        setup.parent.mkdir(parents=True)
        setup.write_text("\n".join(BANNED), encoding="utf-8")
        setup_asm = fixture / "asm/overlay/setup/entry_setup.asm"
        setup_asm.parent.mkdir(parents=True)
        setup_asm.write_text(
            "EXTERN _netchesszx_session_format_busy\nEXTERN ovl_read_chunked\n",
            encoding="utf-8",
        )
        policy_data = json.loads(policy.read_text(encoding="utf-8"))
        policy_data["overlays"]["setup"] = {"allowed_caps": ["board", "gui"]}
        policy.write_text(json.dumps(policy_data), encoding="utf-8")
        connect.write_text(
            "spectrum_append_text\nspectrum_gui_set_status\n"
            + "\n".join(BANNED[:2]),
            encoding="utf-8",
        )
        tx.write_text(
            "spectrum_net_mqtt_presence_payload\n" + BANNED[2], encoding="utf-8"
        )
        result = run_checker(fixture, policy)
        assert result.returncode == 1
        for symbol, overlay, source in (
            (BANNED[0], "mqtt_connect", connect),
            (BANNED[1], "mqtt_connect", connect),
            (BANNED[2], "mqtt_tx", tx),
        ):
            rel = source.relative_to(fixture).as_posix()
            assert f"overlay {overlay}: {rel} imports {symbol} (banned)" in result.stderr
        setup_rel = setup.relative_to(fixture).as_posix()
        for symbol in BANNED:
            assert f"overlay setup: {setup_rel} imports {symbol} (banned)" in result.stderr
        assert (f"overlay setup: {setup_asm.relative_to(fixture).as_posix()} imports "
                "_netchesszx_session_format_busy (banned)") in result.stderr
        assert (f"overlay setup: {setup_asm.relative_to(fixture).as_posix()} imports "
                "ovl_read_chunked (resident)") in result.stderr
        assert (
            "overlay mqtt_connect: "
            f"{connect.relative_to(fixture).as_posix()} imports "
            "spectrum_gui_set_status (gui)"
        ) in result.stderr

        connect.write_text("spectrum_append_text\n", encoding="utf-8")
        tx.write_text("spectrum_net_mqtt_presence_payload\n", encoding="utf-8")
        setup.write_text("spectrum_gui_set_status\n", encoding="utf-8")
        setup_asm.write_text("", encoding="utf-8")
        unassigned = session / "future_actions_ovl.c"
        unassigned.write_text("spectrum_append_text\n", encoding="utf-8")
        result = run_checker(fixture, policy)
        assert result.returncode == 1
        assert (
            f"unassigned overlay source: {unassigned.relative_to(fixture).as_posix()}"
            in result.stderr
        )
    print("Overlay capability negative check ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
