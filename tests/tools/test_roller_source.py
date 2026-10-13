"""Prove the vendored GAME kernel is the unchanged Roller binary."""

import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from roller_z80asm import adapt


def main():
    source = ROOT / "asm/vendor/roller/roller.inc"
    assert hashlib.sha256(source.read_bytes()).hexdigest() == (
        "8ad9d35580034baa4009a3f7dc50eb48fa3dc6822a90e44195287b9b27ecfb3a")
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        (temp / "roller_z80.inc").write_text(adapt(source.read_text()), encoding="ascii")
        (temp / "original.asm").write_text(
            ' DEVICE ZXSPECTRUM48\n ORG $64ac\n DEFINE ROLLER_BUILD 2\n'
            f' INCLUDE "{source.as_posix()}"\n SAVEBIN "original.bin",roller_start,R_CODE_SIZE\n',
            encoding="ascii")
        subprocess.run([shutil.which("sjasmplus"), "--nologo", "original.asm"],
                       cwd=temp, check=True)
        subprocess.run([shutil.which("z80asm"), "-DROLLER_BUILD=2", "-b",
                        "-r0x64ac", "-oadapted.bin", "roller_z80.inc"],
                       cwd=temp, check=True)
        expected = (temp / "original.bin").read_bytes()
        assert len(expected) == 130
        assert expected == (temp / "adapted.bin").read_bytes()
    print("Roller GAME: original source SHA-256 and 130 opcode bytes match")


if __name__ == "__main__":
    main()
