import pytest
from pathlib import Path


APP_START_ADDR = 0x08003000
APP_END_ADDR = 0x08020000


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--hex",
        action="store",
        default=None,
        help="Path to Intel HEX firmware file",
    )
    parser.addoption("--can-iface", action="store", default="socketcan", help="python-can interface")
    parser.addoption("--can-channel", action="store", default="can0", help="CAN channel, e.g. can0")
    parser.addoption("--ack-timeout", action="store", default="0.8", help="ACK timeout in seconds")
    parser.addoption(
        "--node-id",
        action="store",
        default="0x69",
        help="Bootloader CAN node ID (dec or hex, e.g. 105 or 0x69)",
    )


@pytest.fixture(scope="session")
def fw_image(pytestconfig: pytest.Config) -> bytes:
    hex_path = pytestconfig.getoption("--hex")
    if not hex_path:
        pytest.skip("Pass firmware path via --hex=/path/to/firmware.hex")

    image = _load_intel_hex(Path(hex_path))
    if not image:
        pytest.fail("HEX file resolved to empty image within app flash range")
    return image


@pytest.fixture(scope="session")
def flashing_params(pytestconfig: pytest.Config) -> dict:
    ack_timeout = float(pytestconfig.getoption("--ack-timeout"))
    raw_node_id = str(pytestconfig.getoption("--node-id"))
    node_id = int(raw_node_id, 0)

    if ack_timeout <= 0:
        pytest.fail("--ack-timeout must be > 0")
    if node_id <= 0 or node_id > 0x7FF:
        pytest.fail("--node-id must be in range 1..0x7FF")

    return {
        "iface": pytestconfig.getoption("--can-iface"),
        "channel": pytestconfig.getoption("--can-channel"),
        "ack_timeout": ack_timeout,
        "node_id": node_id,
    }


def _load_intel_hex(path: Path) -> bytes:
    if not path.exists():
        pytest.fail(f"HEX file not found: {path}")

    addr_base = 0
    data_map: dict[int, int] = {}

    for line_no, raw in enumerate(path.read_text(encoding="ascii").splitlines(), start=1):
        line = raw.strip()
        if not line:
            continue
        if not line.startswith(":"):
            pytest.fail(f"Invalid HEX line {line_no}: missing ':'")

        try:
            rec = bytes.fromhex(line[1:])
        except ValueError:
            pytest.fail(f"Invalid HEX line {line_no}: non-hex characters")

        if len(rec) < 5:
            pytest.fail(f"Invalid HEX line {line_no}: too short")

        count = rec[0]
        offset = (rec[1] << 8) | rec[2]
        rec_type = rec[3]
        payload = rec[4 : 4 + count]
        checksum = rec[4 + count] if len(rec) > (4 + count) else None

        if checksum is None:
            pytest.fail(f"Invalid HEX line {line_no}: missing checksum")
        if ((sum(rec[:-1]) + checksum) & 0xFF) != 0:
            pytest.fail(f"Invalid HEX line {line_no}: checksum mismatch")

        if rec_type == 0x00:  # data
            abs_addr = addr_base + offset
            for i, b in enumerate(payload):
                current = abs_addr + i
                if APP_START_ADDR <= current < APP_END_ADDR:
                    data_map[current] = b
        elif rec_type == 0x01:  # EOF
            break
        elif rec_type == 0x04:  # extended linear address
            if count != 2:
                pytest.fail(f"Invalid HEX line {line_no}: bad type-04 length")
            addr_base = ((payload[0] << 8) | payload[1]) << 16
        elif rec_type in (0x02, 0x03, 0x05):
            # Non-essential records for binary image extraction.
            continue
        else:
            pytest.fail(f"Unsupported HEX record type 0x{rec_type:02X} at line {line_no}")

    if not data_map:
        return b""

    max_addr = max(data_map.keys())
    size = (max_addr - APP_START_ADDR) + 1
    image = bytearray([0xFF] * size)
    for addr, val in data_map.items():
        image[addr - APP_START_ADDR] = val
    return bytes(image)

