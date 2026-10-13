"""Adapt only SjASMPlus local-label spelling; Roller instructions stay intact."""

import re
import sys
from pathlib import Path


def adapt(source: str) -> str:
    scope = ""
    lines = []
    for line in source.splitlines(keepends=True):
        code, separator, comment = line.partition(";")
        label = re.match(r"^([A-Za-z_]\w*):", code)
        if label:
            scope = label[1]
        code = re.sub(r"\.(\w+)", lambda match: scope + "_" + match[1], code)
        lines.append(code + separator + comment)
    return "".join(lines)


if __name__ == "__main__":
    Path(sys.argv[2]).write_text(adapt(Path(sys.argv[1]).read_text()), encoding="ascii")
