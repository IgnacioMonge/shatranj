#!/usr/bin/env python3
"""Package a direct HTTP(S) launch using the canonical SpectraNext tools.

Writes build/spectranext-direct/; publishes nothing.
"""

import shutil
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: build_spectranext_direct.py SPXN_DIR")
    driver = Path(sys.argv[1]).resolve()
    sys.path.insert(0, str(driver.parent / "tools"))
    from build_installer import build_compact_tape_launcher
    from package_resource import stage

    root = Path(__file__).resolve().parents[1]
    build = root / "build"
    build.mkdir(exist_ok=True)
    (build / "spectranext-boot.zx").write_bytes(
        build_compact_tape_launcher("SHATRANJ.tap", "SHATRANJ")
    )
    output = build / "spectranext-direct"
    staging = build / "spectranext-direct.stage"
    previous = build / "spectranext-direct.previous"
    shutil.rmtree(staging, ignore_errors=True)
    files = stage(root / "packaging/spectranext/direct.json", staging)
    shutil.rmtree(previous, ignore_errors=True)
    if output.exists():
        output.rename(previous)
    try:
        staging.rename(output)
    except OSError:
        if previous.exists():
            previous.rename(output)
        raise
    shutil.rmtree(previous, ignore_errors=True)
    print(f"Direct HTTP(S) resource: {output} ({len(files)} files)")


if __name__ == "__main__":
    main()
