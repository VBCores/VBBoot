from pathlib import Path
import re


def test_default_node_id_is_controller_6() -> None:
    header = Path(__file__).resolve().parents[1] / "App" / "app.h"
    text = header.read_text(encoding="utf-8")
    match = re.search(r"#define\s+DEFAULT_NODE_ID\s+(0x[0-9A-Fa-f]+|\d+)U?", text)

    assert match is not None
    assert int(match.group(1), 0) == 0x06


def test_nominal_can_sample_point_matches_can0() -> None:
    source = Path(__file__).resolve().parents[1] / "App" / "state_manager.c"
    text = source.read_text(encoding="utf-8")

    seg1 = _assigned_int(text, "NominalTimeSeg1")
    seg2 = _assigned_int(text, "NominalTimeSeg2")

    assert (1 + seg1) / (1 + seg1 + seg2) == 0.75


def test_app_validity_checks_stack_and_reset_ranges() -> None:
    source = Path(__file__).resolve().parents[1] / "App" / "app.c"
    text = source.read_text(encoding="utf-8")

    assert "0x20000000UL" in text
    assert "app_sp <= 0x20008000UL" in text
    assert "APP_START_ADDR" in text
    assert "APP_END_ADDR" in text


def test_fdcan_dlc_uses_hal_dlc_indexes() -> None:
    source = Path(__file__).resolve().parents[1] / "App" / "communications.c"
    text = source.read_text(encoding="utf-8")

    assert "switch (dlc)" in text
    assert "dlc >> 16U" not in text
    assert "return (uint32_t)len;" in text
    assert "len << 16U" not in text


def _assigned_int(text: str, field_name: str) -> int:
    match = re.search(rf"Init\.{field_name}\s*=\s*(\d+)\s*;", text)
    assert match is not None
    return int(match.group(1))
