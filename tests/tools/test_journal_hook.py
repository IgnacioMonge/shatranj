#!/usr/bin/env python3
"""Real resident journal hook, append and STJ codec under injected failures."""

from pathlib import Path
import tempfile

from test_spectrum_asm_vectors import (
    JOURNAL_SNAPSHOT,
    check_journal_stj,
    journal_body,
    journal_stj,
    run_journal_scenario,
)


def main() -> None:
    fail = ("poke", "test_overlay_fail", 1)
    recover = ("poke", "test_overlay_fail", 0)
    notify = ("notify",)
    with tempfile.TemporaryDirectory(prefix="journal-hook-") as directory:
        tmp = Path(directory)

        def run(name, steps, **kwargs):
            return run_journal_scenario(tmp, name, steps,
                                        resident_hook=True, **kwargs)

        state = run("missing_middle", [
            ("move", "e2e4"), fail, ("move", "e7e5"), notify, notify,
            recover, ("move", "g1f3"), ("save", "01MID"),
        ])
        check_journal_stj(state["STJ"], ["e2e4", "g1f3"], False, "middle")
        assert state["notices"][-1] == 1
        assert state["notice"] == b"History incomplete" and state["notice_error"] == 1

        state = run("failed_first_save", [
            fail, ("move", "e2e4"), ("save", "01FIRST"), notify,
        ], pend=0x80)
        check_journal_stj(state["STJ"], [], False, "first before retry")
        assert state["pend"] == 0xC0 and state["notices"][-1] == 1

        state = run("failed_first_retry", [
            fail, ("move", "e2e4"), recover, ("move", "e7e5"),
            ("save", "01RETRY"),
        ], pend=0x80)
        check_journal_stj(state["STJ"], ["e7e5"], False, "first after retry")

        loaded = journal_stj(JOURNAL_SNAPSHOT, journal_body(["e2e4", "e7e5"]))
        state = run("failed_loaded_append", [
            ("load", "LOAD"), ("arm",), fail, ("move", "g1f3"),
            ("save", "01LOADED"), notify,
        ], files={"LOAD": loaded})
        check_journal_stj(state["STJ"], ["e2e4", "e7e5"], False, "loaded")
        assert state["pend"] == 0xC1 and state["NEW"] == journal_body(["e2e4", "e7e5"])

        state = run("failed_undo", [
            ("move", "e2e4"), ("move", "e7e5"), fail, ("undo",),
            notify, ("undo",), notify, ("save", "01UNDO"),
        ])
        check_journal_stj(state["STJ"], ["e2e4", "e7e5"], False, "undo")
        assert state["notices"][-1] == 1 and state["warnings"][-1] == 2

        state = run("failed_repaint", [
            ("load", "LOAD"), ("arm",), fail, ("replay", 3),
            notify, ("save", "01REPLAY"),
        ], files={"LOAD": loaded})
        check_journal_stj(state["STJ"], ["e2e4", "e7e5"], True, "repaint")
        assert state["fd"] == 0 and state["pend"] == 0x81
        assert state["notices"][-1] == 0

        state = run("sync_failure", [
            ("poke", "fs_sync_fail", 1), ("move", "e2e4"), notify,
            ("move", "e7e5"), notify, ("save", "01SYNC"),
        ])
        check_journal_stj(state["STJ"], ["e2e4"], False, "sync")
        assert state["fd"] == 0x7F and state["notices"][-1] == 1

        state = run("once_per_game", [
            ("poke", "fs_create_fail", 1), ("move", "e2e4"), notify,
            ("move", "e7e5"), notify,
            ("poke", "_spectrum_journal_pend", 0x80), ("arm",),
            ("move", "d2d4"), notify,
        ])
        assert state["notices"][4] == 1 and state["notices"][-1] == 2
        assert state["warnings"][-1] == 2

        # A page-in failure skips capture entirely. Its old undo belongs to
        # a different ply and must not restore timings or remove a capture.
        steps = [
            ("poke16", "_netchesszx_session_state", 1),
            ("poke", "test_stats_time+10", 11),
            ("poke", "_spectrum_stats_undo+2", ord("q")), ("move", "e2e4"),
            ("poke16", "_netchesszx_session_state", 2),
            ("poke", "test_stats_time+10", 22),
            ("poke", "_spectrum_stats_undo+2", ord("n")), ("move", "e7e5"),
            fail, ("poke16", "_netchesszx_session_state", 3),
            ("poke", "_spectrum_stats_undo+2", ord("r")), ("move", "g1f3"),
            recover, ("poke16", "_netchesszx_session_state", 2), ("undo",),
        ]
        state = run("capture_missing_then_undo", steps, pend=0x80)
        samples = [sample for sample in state["stats_steps"] if sample is not None]
        markers = [marker for marker in state["capture_steps"] if marker is not None]
        # Layout: common/chess/game_stats.h (sides 0..15, base 16, flags 18,
        # undo 19..27, captures 28..32).
        assert samples[1][:16] == bytes((11, 0, 11, 0, 1, 11, 0, 1,
                                         22, 0, 22, 0, 1, 22, 0, 1))
        assert samples[1][28:33] == bytes((0, 16, 0, 0, 1))
        assert samples[2][18] == 0x41 and samples[2][:16] == samples[1][:16]
        assert markers == [0, 0, 1, 0], markers
        assert samples[3] == bytes(16) + bytes((2, 0, 1)) + bytes(14), samples[3].hex()

        # A later I/O failure also reaches the hook's failure handler, but
        # capture ran. Preserve its undo so TAKEBACK retains the earlier ply.
        steps = [
            ("poke16", "_netchesszx_session_state", 1),
            ("poke", "test_stats_time+10", 11),
            ("poke", "_spectrum_stats_undo+2", ord("q")), ("move", "e2e4"),
            ("poke", "fs_sync_fail", 1),
            ("poke16", "_netchesszx_session_state", 2),
            ("poke", "test_stats_time+10", 22),
            ("poke", "_spectrum_stats_undo+2", ord("n")), ("move", "e7e5"),
            ("poke", "fs_sync_fail", 0),
            ("poke16", "_netchesszx_session_state", 1), ("undo",),
        ]
        state = run("capture_io_failure_then_undo", steps, pend=0x80)
        samples = [sample for sample in state["stats_steps"] if sample is not None]
        outcomes = [step for step in state["steps"] if step is not None]
        assert outcomes[1][2] == 0x7F, outcomes
        assert samples[1][18] == 0x20, samples[1].hex()
        assert [marker for marker in state["capture_steps"] if marker is not None] == [0, 0, 0]
        assert samples[2][:16] == samples[0][:16], samples[2].hex()
        assert samples[2][16:19] == bytes(3), samples[2].hex()
        assert samples[2][28:33] == samples[0][28:33], samples[2].hex()
    print("[OK] journal resident hook: 10 failure/recovery scenarios, capture/undo lifetime")


if __name__ == "__main__":
    main()
