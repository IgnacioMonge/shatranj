"""Offline checks for the DIRECT burst probe's script and burst builder."""

import unittest

from tools import direct_burst_probe as probe
from tools.spcx_direct_conformance import DEFAULT_MAX_FRAME_SIZE


class BurstProbeTests(unittest.TestCase):
    def test_script_is_coordinate_plies_for_both_sides(self):
        self.assertEqual(len(probe.SCRIPT) % 2, 0)
        for move in probe.SCRIPT:
            self.assertRegex(move, r"^[a-h][1-8][a-h][1-8]$")

    def test_burst_fills_the_frame_and_carries_pings(self):
        lines = probe.burst_lines(7, 12, 4, "PLAY E2E4")
        chats = [line for line in lines if line.startswith("CHAT ")]
        self.assertEqual(len(chats), 12)
        self.assertEqual(lines.count("PING"), 3)
        self.assertTrue(all(len(line) == DEFAULT_MAX_FRAME_SIZE for line in chats))
        self.assertTrue(chats[0].startswith("CHAT T0007"))
        self.assertIn("PLAY E2E4", chats[-1])
        # Past the Spectrum's receive buffering (256-byte ring + three frames).
        self.assertGreater(sum(len(line) + 1 for line in lines), 400)

    def test_hint_names_the_next_scripted_move(self):
        self.assertEqual(probe.hint_for(0), "PLAY E2E4")
        self.assertEqual(probe.hint_for(len(probe.SCRIPT)), "SCRIPT DONE")


if __name__ == "__main__":
    unittest.main()
