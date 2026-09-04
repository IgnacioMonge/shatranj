#!/usr/bin/env python3
"""Read labelled DEFB data blocks from Z80 assembly sources."""

from __future__ import annotations

import re


def label_block(text: str, label: str, end_label: str | None = None) -> str:
    start = re.search(rf"(?m)^{re.escape(label)}:\s*$", text)
    if not start:
        raise SystemExit(f"label not found: {label}")
    body = text[start.end() :]
    if end_label is not None:
        end = re.search(rf"(?m)^{re.escape(end_label)}:\s*$", body)
        if not end:
            raise SystemExit(f"end label not found: {end_label}")
        body = body[: end.start()]
    return body


def parse_defb_with_offsets(text: str) -> tuple[bytes, dict[str, int]]:
    data = bytearray()
    offsets: dict[str, int] = {}
    for raw in text.splitlines():
        line = raw.split(";", 1)[0].strip()
        label = re.match(r"^([A-Za-z_][A-Za-z0-9_]*):$", line)
        if label:
            offsets[label.group(1)] = len(data)
            continue
        if not line.upper().startswith("DEFB"):
            continue
        for token in line[4:].strip().split(","):
            token = token.strip()
            if not token:
                continue
            if token.startswith('"') and token.endswith('"'):
                data.extend(token[1:-1].encode("ascii"))
            else:
                data.append(int(token, 0) & 0xFF)
    return bytes(data), offsets


def parse_defb_data(text: str) -> bytes:
    return parse_defb_with_offsets(text)[0]


def parse_defb_block(text: str, label: str, end_label: str) -> bytes:
    return parse_defb_data(label_block(text, label, end_label))
