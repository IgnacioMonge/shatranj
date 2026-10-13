#!/usr/bin/env python3
"""Check the assembled Spectranext Pages site (build/spectranext-pages)."""

import json
import re
from pathlib import Path


def index_names(folder):
    names = []
    for line in (folder / "index.txt").read_text().splitlines():
        match = re.fullmatch(r"type=file name=(\S+) size=(\d+)", line)
        assert match, line
        assert (folder / match[1]).stat().st_size == int(match[2]), match[1]
        names.append(match[1])
    return set(names)


def main():
    root = Path(__file__).resolve().parents[2]
    site = root / "build/spectranext-pages"
    version = (root / "VERSION").read_text().strip()

    # Root: the direct-launch resource, byte-identical to its staged folder.
    direct = root / "build/spectranext-direct"
    payloads = {"boot.zx", "SHATRANJ.tap", "SHATRANJ.OVL", "SHATRANJ.DAT"}
    assert index_names(site) == payloads | {"package.json"}
    for name in payloads | {"package.json", "index.txt", ".nojekyll"}:
        assert (site / name).read_bytes() == (direct / name).read_bytes(), name
    assert json.loads((site / "package.json").read_text())["version"] == version

    # /install/: the guided installer, unchanged.
    installer = root / "build/spectranext-resource"
    install = site / "install"
    assert {p.name for p in install.iterdir()} == {p.name for p in installer.iterdir()}
    for path in installer.iterdir():
        assert (install / path.name).read_bytes() == path.read_bytes(), path.name
    assert "name=SHATRANJ.PKG " in (install / "index.txt").read_text()

    # Landing page: self-contained, every local reference present.
    page = (site / "index.html").read_text(encoding="utf-8")
    assert not re.search(r"@[A-Z]+@", page)
    assert f"v{version} " in page
    assert not re.search(r"<(script|link|img)\b[^>]*\b(src|href)=\"(https?:)?//", page)
    for reference in re.findall(r'(?:src|href)="(img/[^"]+)"', page) + re.findall(
        r'url\("(img/[^"]+)"\)', page
    ):
        assert (site / reference).is_file(), reference
    assert "https://ignaciomonge.github.io/shatranj/install/" in page.replace("<wbr>", "")
    print("Spectranext Pages site: direct resource at /, installer at /install/, "
          "self-contained landing page PASS")


if __name__ == "__main__":
    main()
