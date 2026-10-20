import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools import build_spectranext_pages as pages


class ReleaseNameTests(unittest.TestCase):
    def test_preview_keeps_unreleased_changelog_undated(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "VERSION").write_text("1.4\n")
            changelog = "## [Unreleased]\n\n## [1.3] - 2026-10-13 - Sealed Move\n"
            (root / "CHANGELOG.md").write_text(changelog)
            with patch.object(pages, "ROOT", root):
                self.assertEqual(pages.release_name("Open Match"), ("1.4", "Open Match"))
                with self.assertRaisesRegex(SystemExit, "CHANGELOG release 1.3 != VERSION 1.4"):
                    pages.release_name()
                with self.assertRaises(SystemExit):
                    pages.release_name(" ")
                self.assertEqual((root / "CHANGELOG.md").read_text(), changelog)

                (root / "VERSION").write_text("1.4-dev000\n")
                with self.assertRaises(SystemExit):
                    pages.release_name("Open Match")

    def test_published_release_uses_changelog_name(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "VERSION").write_text("1.4\n")
            (root / "CHANGELOG.md").write_text(
                "## [Unreleased]\n\n## [1.4] - 2026-10-20 - Open Match\n"
            )
            with patch.object(pages, "ROOT", root):
                self.assertEqual(pages.release_name(), ("1.4", "Open Match"))


if __name__ == "__main__":
    unittest.main()
