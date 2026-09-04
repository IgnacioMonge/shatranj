#!/usr/bin/env python3
"""Build Shatranj app icons from a square master painting.

Reads a generated 1:1 knight-on-badge image, keys out the light canvas,
and writes the packaged PNG/ICO/ICNS plus the runtime asset.
"""

from __future__ import annotations

import argparse
import io
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_PNG = ROOT / "assets" / "pc-client" / "about" / "app-icon.png"
DEFAULT_SOURCE = RUNTIME_PNG
LINUX_PNG = ROOT / "client" / "shatranj.png"
WINDOWS_ICO = ROOT / "client" / "shatranj.ico"
MAC_ICNS = ROOT / "client" / "shatranj.icns"
PREVIEW = ROOT / "build" / "pc-build" / "app-icon-preview.png"

ICO_SIZES = (16, 20, 24, 32, 48, 64, 128, 256)
ICNS_TYPES = (
    ("icp4", 16),
    ("icp5", 32),
    ("icp6", 64),
    ("ic07", 128),
    ("ic08", 256),
    ("ic09", 512),
    ("ic10", 1024),
    ("ic11", 32),
    ("ic12", 64),
    ("ic13", 256),
    ("ic14", 512),
)


def is_canvas(r: int, g: int, b: int) -> bool:
    mx, mn = max(r, g, b), min(r, g, b)
    return mn > 188 and (mx - mn) < 22


def key_canvas(src: Image.Image) -> Image.Image:
    rgba = src.convert("RGBA")
    pixels = rgba.load()
    w, h = rgba.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if is_canvas(r, g, b):
                pixels[x, y] = (r, g, b, 0)
    alpha = rgba.getchannel("A").filter(ImageFilter.GaussianBlur(0.6))
    rgba.putalpha(alpha.point(lambda p: 0 if p < 24 else (255 if p > 140 else p)))
    return rgba


def scale_icon(master: Image.Image, size: int) -> Image.Image:
    icon = master.resize((size, size), Image.Resampling.LANCZOS)
    if size > 32:
        return icon
    # Keep a readable gold rim after downscale.
    rim = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(rim)
    inset = 0.5 if size >= 20 else 0.0
    width = 1.0 if size < 24 else 1.25
    draw.rounded_rectangle(
        [inset, inset, size - 1 - inset, size - 1 - inset],
        radius=max(3.0, size * 0.22),
        outline=(196, 165, 116, 230),
        width=max(1, int(round(width))),
    )
    return Image.alpha_composite(icon, rim)


def png_bytes(image: Image.Image) -> bytes:
    buf = io.BytesIO()
    image.save(buf, format="PNG")
    return buf.getvalue()


def write_icns(path: Path, master: Image.Image) -> None:
    chunks = []
    for code, size in ICNS_TYPES:
        data = png_bytes(scale_icon(master, size))
        chunks.append(code.encode("ascii") + struct.pack(">I", 8 + len(data)) + data)
    body = b"".join(chunks)
    path.write_bytes(b"icns" + struct.pack(">I", 8 + len(body)) + body)


def write_ico(path: Path, master: Image.Image) -> None:
    payloads = []
    for size in ICO_SIZES:
        payloads.append(png_bytes(scale_icon(master, size)))
    offset = 6 + 16 * len(payloads)
    directory = struct.pack("<HHH", 0, 1, len(payloads))
    for size, payload in zip(ICO_SIZES, payloads):
        directory += struct.pack(
            "<BBBBHHII",
            size if size < 256 else 0,
            size if size < 256 else 0,
            0,
            0,
            1,
            32,
            len(payload),
            offset,
        )
        offset += len(payload)
    path.write_bytes(directory + b"".join(payloads))


def write_preview(path: Path, master: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    cells = (16, 24, 32, 48, 64, 128)
    gap = 16
    cell = 140
    canvas = Image.new("RGB", (gap + (cell + gap) * len(cells), cell * 2 + gap * 3), (0, 0, 0))
    dark = (22, 22, 32)
    light = (236, 236, 232)
    draw = ImageDraw.Draw(canvas)
    draw.rectangle([0, 0, canvas.width, cell + gap * 2], fill=dark)
    draw.rectangle([0, cell + gap * 2, canvas.width, canvas.height], fill=light)
    x = gap
    for size in cells:
        icon = scale_icon(master, size)
        for row, ground in ((0, dark), (1, light)):
            box = Image.new("RGBA", (cell, cell), ground + (255,))
            ox = (cell - size) // 2
            oy = (cell - size) // 2
            box.alpha_composite(icon, (ox, oy))
            canvas.paste(box.convert("RGB"), (x, gap + row * (cell + gap)))
        x += cell + gap
    canvas.save(path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    args = parser.parse_args()
    source = args.source
    if not source.is_file():
        raise SystemExit(f"missing source image: {source}")

    master = key_canvas(Image.open(source))
    # Store a 512 runtime/Linux master; keep 1024 pixels for ICO/ICNS.
    runtime = master.resize((512, 512), Image.Resampling.LANCZOS)
    RUNTIME_PNG.parent.mkdir(parents=True, exist_ok=True)
    runtime.save(RUNTIME_PNG)
    runtime.save(LINUX_PNG)
    write_ico(WINDOWS_ICO, master)
    write_icns(MAC_ICNS, master)
    write_preview(PREVIEW, master)
    print(f"wrote {RUNTIME_PNG}")
    print(f"wrote {LINUX_PNG}")
    print(f"wrote {WINDOWS_ICO}")
    print(f"wrote {MAC_ICNS}")
    print(f"wrote {PREVIEW}")


if __name__ == "__main__":
    main()
