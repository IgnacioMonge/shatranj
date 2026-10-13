#!/usr/bin/env python3
"""Recovery link kept by the product journal append (SAVELOAD entry 3).

_spectrum_journal_count/_crc must always describe the durable SHATJRNL.TMP
prefix, and an applied snapshot (HL = 1/3) must consume the armed switch at
once so the checkpoint written next already names the live TMP.
"""

from pathlib import Path
import tempfile

from test_spectrum_asm_vectors import (
    JOURNAL_LOADED,
    journal_body,
    journal_crc16,
    journal_stj,
    run_journal_scenario,
)


def link(moves: list[str | None]) -> tuple[int, int]:
    body = journal_body(moves)
    return len(body) // 2, journal_crc16(body)


def main() -> None:
    loaded = ["e2e4", "c7c5", None, "g1f3"]
    loaded_stj = journal_stj(JOURNAL_LOADED, journal_body(loaded))
    with tempfile.TemporaryDirectory(prefix="journal-link-") as directory:
        tmp = Path(directory)

        def run(name, steps, **kwargs):
            return run_journal_scenario(tmp, name, steps, recovery=True, **kwargs)

        # New game: every synced record is counted, TAKEBACK included.
        moves = ["e2e4", "e7e5", None, "g8f6", "a7a8n"]
        s = run("new_game", [("move", m) if m else ("undo",) for m in moves],
                pend=0x80)
        assert s["TMP"] == journal_body(moves), s["TMP"].hex()
        assert (s["count"], s["crc"]) == link(moves), s

        # Apply-time call: replay the staged body, then adopt it immediately.
        s = run("apply_adopts", [
            ("move", "a2a3"), ("load", "saved"), ("arm",), ("replay",),
        ], files={"LOAD": loaded_stj}, pend=0x80)
        replay = s["steps"][3]
        assert replay[0] == 1 and replay[3] == 0, replay
        assert s["TMP"] == journal_body(loaded) and s["NEW"] is None, s
        assert s["fd"] & 0x3F not in (0, 0x3F) and not s["fd"] & 0x40, s
        assert (s["count"], s["crc"]) == link(loaded), s
        assert (s["move_rows"], s["last_ply"]) == (1, 2), s["move_log"][:32]
        s = run("apply_then_move", [
            ("load", "saved"), ("arm",), ("replay",), ("move", "d7d6"),
            ("save", "01"),
        ], files={"LOAD": loaded_stj}, pend=0x80)
        assert s["TMP"] == journal_body(loaded + ["d7d6"]), s["TMP"].hex()
        assert (s["count"], s["crc"]) == link(loaded + ["d7d6"]), s
        assert s["steps"][2][3] == 0, s["steps"][2]

        # A partial staged body is adopted but not painted.
        partial = journal_stj(JOURNAL_LOADED, journal_body(loaded), False)
        s = run("apply_partial", [
            ("load", "saved"), ("arm",), ("replay", 3),
        ], files={"LOAD": partial}, pend=0x80)
        assert s["TMP"] == journal_body(loaded) and s["fd"] & 0x40, s
        assert (s["count"], s["crc"]) == link(loaded), s
        assert s["move_rows"] == 1, s["move_rows"]

        # A remote RESTORE (0xC0) starts an empty partial TMP at apply time.
        s = run("apply_remote", [
            ("move", "e2e4"), ("poke", "_spectrum_journal_pend", 0xC0),
            ("replay", 3), ("move", "e7e5"),
        ], pend=0x80)
        assert s["TMP"] == journal_body(["e7e5"]) and s["fd"] & 0x40, s
        assert (s["count"], s["crc"]) == link(["e7e5"]), s

        # Nothing armed: the apply call is a no-op for the link.
        s = run("apply_idle", [("move", "e2e4"), ("replay",)], pend=0x80)
        assert s["TMP"] == journal_body(["e2e4"]) and s["steps"][1][0] == 1, s
        assert (s["count"], s["crc"]) == link(["e2e4"]), s

        # A failed record leaves the link on the durable prefix.
        s = run("sync_failure", [
            ("move", "e2e4"), ("move", "e7e5"),
            ("poke", "fs_sync_fail", 1), ("move", "g1f3"), ("move", "b8c6"),
        ], pend=0x80)
        assert s["fd"] == 0x7F, s["fd"]
        assert (s["count"], s["crc"]) == link(["e2e4", "e7e5"]), s
        assert s["TMP"][:4] == journal_body(["e2e4", "e7e5"]), s["TMP"].hex()

        # Adoption syncs TMP before deleting NEW; a failed sync keeps NEW.
        s = run("apply_adopt_sync_failure", [
            ("load", "saved"), ("arm",), ("poke", "fs_sync_fail", 1),
            ("replay",),
        ], files={"LOAD": loaded_stj}, pend=0x80)
        assert s["NEW"] is not None and (s["fd"], s["warning"]) == (0x7F, 1), s

        # An apply whose TMP cannot be created: empty link, visible warning.
        s = run("apply_create_failure", [
            ("move", "e2e4"), ("poke", "_spectrum_journal_pend", 0xC0),
            ("poke", "fs_create_fail", 1), ("replay", 3),
        ], pend=0x80)
        assert (s["fd"], s["pend"], s["warning"]) == (0x40, 0, 1), s
        assert (s["count"], s["crc"]) == (0, 0xFFFF), s

        # A lost NEW at apply time is not linked to the old TMP.
        s = run("apply_new_lost", [
            ("move", "e2e4"), ("poke", "_spectrum_journal_pend", 0x81),
            ("replay", 3),
        ], pend=0x80)
        assert (s["fd"], s["warning"]) == (0x40, 1), s
        assert (s["count"], s["crc"]) == (0, 0xFFFF), s

        # Adoption on the first ply (apply call lost) still links correctly.
        s = run("deferred_adopt", [
            ("load", "saved"), ("arm",), ("move", "d7d6"),
        ], files={"LOAD": loaded_stj}, pend=0x80)
        assert (s["count"], s["crc"]) == link(loaded + ["d7d6"]), s

        # Long bodies adopt across several scratch chunks.
        long = ["a2a3", "a7a6"] * 60 + [None]
        s = run("apply_long", [("load", "saved"), ("arm",), ("replay",)],
                files={"LOAD": journal_stj(JOURNAL_LOADED, journal_body(long))},
                pend=0x80)
        assert s["TMP"] == journal_body(long), len(s["TMP"])
        assert (s["count"], s["crc"]) == link(long), s
    print("[OK] journal recovery link: 12 scenarios")


if __name__ == "__main__":
    main()
