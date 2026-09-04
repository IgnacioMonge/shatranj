#!/usr/bin/env python3
"""Write a deterministic build-configuration stamp without needless mtime changes."""

from __future__ import annotations

import argparse
import json
import os
import tempfile
from pathlib import Path


def replace_if_changed(path: Path, data: bytes) -> bool:
    if path.exists() and path.read_bytes() == data:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
        os.replace(temp_name, path)
    except BaseException:
        Path(temp_name).unlink(missing_ok=True)
        raise
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--value", action="append", default=[])
    args = parser.parse_args()

    values: dict[str, str] = {}
    for item in args.value:
        key, separator, value = item.partition("=")
        if not separator or not key:
            raise SystemExit(f"invalid --value (expected KEY=VALUE): {item}")
        if key in values:
            raise SystemExit(f"duplicate configuration key: {key}")
        values[key] = value

    data = (json.dumps(values, indent=2, sort_keys=True) + "\n").encode("utf-8")
    replace_if_changed(args.out, data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
