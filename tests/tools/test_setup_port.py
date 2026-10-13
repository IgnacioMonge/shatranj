"""Host-side contract for the five-digit setup port editor."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SETUP = (ROOT / "asm/overlay/setup/entry_setup.asm").read_text(encoding="utf-8")
HEADER = (ROOT / "src/spectrum/ui/setup.h").read_text(encoding="utf-8")


def test_port_editor_accepts_uint16_max_and_rejects_overflow() -> None:
    assert "SETUP_PORT_INPUT_MAX" in HEADER
    assert "65535" not in SETUP  # parsed, not hard-coded
    assert "su_parse_port" in SETUP
    assert "_netchesszx_direct_port" in SETUP


def test_port_editor_is_bounded_to_five_digits() -> None:
    assert "SETUP_PORT_INPUT_MAX 5u" in HEADER
    assert "_setup_port_text" in SETUP
