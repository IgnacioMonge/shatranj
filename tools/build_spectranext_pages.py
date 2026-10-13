#!/usr/bin/env python3
"""Assemble the Spectranext GitHub Pages site; publishes nothing.

Layout, as SpecTalkZX publishes it:
  /          direct-launch resource (build/spectranext-direct) whose index.html
             is the landing page from packaging/spectranext/site/
  /install/  guided installer (build/spectranext-resource)
  /img/      landing-page images, derived from the repository artwork
Writes build/spectranext-pages/.
"""

import html
import json
import re
import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
SITE = ROOT / "packaging/spectranext/site"

# Landing gallery: screenshot, English caption, Spanish caption.
SHOTS = (
    ("shatranj-1.3-classic-stats.png", "End-of-game statistics",
     "Estadísticas de fin de partida"),
    ("shatranj-1.3-classic-stockfish.png", "Playing Stockfish from a Spectrum",
     "Jugando contra Stockfish desde un Spectrum"),
    ("shatranj-1.3-next-names-menu.png", "Spectrum Next: names and rolling menu",
     "Spectrum Next: nombres y menú rotatorio"),
    ("shatranj-1.3-next-resume.png", "Resuming after a power cut",
     "Reanudar tras un corte de corriente"),
)


def release_name():
    """Version and codename of the newest CHANGELOG release heading."""
    for line in (ROOT / "CHANGELOG.md").read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"## \[([0-9.]+)\] - [0-9-]+ - (.+)", line)
        if match:
            version = (ROOT / "VERSION").read_text(encoding="ascii").strip()
            if match[1] != version:
                raise SystemExit(f"CHANGELOG release {match[1]} != VERSION {version}")
            return match[1], match[2]
    raise SystemExit("CHANGELOG.md has no release heading")


def save_resized(source, target, width, **options):
    with Image.open(source) as image:
        if image.width > width:
            image = image.resize((width, round(image.height * width / image.width)),
                                 Image.LANCZOS)
        if target.suffix == ".jpg":
            image = image.convert("RGB")
        image.save(target, optimize=True, **options)


def landing(version, codename):
    figures = []
    for name, english, spanish in SHOTS:
        figures.append(
            f'<figure><img class="shot" src="img/{name}" width="640" height="480" '
            f'loading="lazy" alt="{html.escape(english)}">'
            f'<figcaption><span lang="en">{html.escape(english)}</span>'
            f'<span lang="es">{html.escape(spanish)}</span></figcaption></figure>'
        )
    page = (SITE / "index.html").read_text(encoding="utf-8")
    page = page.replace("@SHOTS@", "\n      ".join(figures))
    page = page.replace("@VERSION@", version).replace("@CODENAME@", html.escape(codename))
    if re.search(r"@[A-Z]+@", page):
        raise SystemExit("unresolved placeholder in the landing page")
    return page


def main():
    version, codename = release_name()
    direct = BUILD / "spectranext-direct"
    installer = BUILD / "spectranext-resource"
    for folder, target in ((direct, "make spectranext-direct"),
                           (installer, "make spectranext-port-build")):
        if not (folder / "package.json").is_file():
            raise SystemExit(f"missing {folder}; run {target} first")
    for folder in (direct, installer):
        built = json.loads((folder / "package.json").read_text())["version"]
        if built != version:
            raise SystemExit(f"{folder.name} is version {built}, expected {version}")

    output = BUILD / "spectranext-pages"
    staging = BUILD / "spectranext-pages.stage"
    previous = BUILD / "spectranext-pages.previous"
    shutil.rmtree(staging, ignore_errors=True)
    shutil.copytree(direct, staging)
    shutil.copytree(installer, staging / "install")
    (staging / "index.html").write_text(landing(version, codename), encoding="utf-8")
    images = staging / "img"
    images.mkdir()
    save_resized(ROOT / "docs/assets/shatranj-logo-dark.png", images / "logo.png", 1040)
    save_resized(ROOT / "assets/pc-client/about/banner-board.png", images / "hero.jpg",
                 1196, quality=84, progressive=True)
    save_resized(ROOT / "assets/pc-client/about/app-icon.png", images / "icon.png", 64)
    for name, _, _ in SHOTS:
        shutil.copyfile(ROOT / "docs/screenshots" / name, images / name)

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
    print(f"Spectranext Pages site {version} {codename}: {output}")


if __name__ == "__main__":
    main()
