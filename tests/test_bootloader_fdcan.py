import binascii
import struct
import time

import pytest

try:
    import can
except ImportError as exc:  # pragma: no cover
    raise RuntimeError("python-can is required: pip install python-can") from exc


BL_STATUS_DONE = 0xD0
BL_STATUS_ERR = 0xE0

BOOT_CMD_START = 1
BOOT_CMD_DATA = 2
BOOT_CMD_DONE = 3
BOOT_CMD_PING = 4
BOOT_CMD_GET_ID = 5


def debug_print(msg: str) -> None:
    print(f"[DEBUG] {msg}")


def test_flash_bootloader_via_can(fw_image: bytes, flashing_params: dict) -> None:
    cmd_id = flashing_params["node_id"]
    ack_id = flashing_params["node_id"]

    crc32 = binascii.crc32(fw_image) & 0xFFFFFFFF
    total_size = len(fw_image)
    data_chunk_size = 63
    start_timeout = flashing_params["ack_timeout"]
    if start_timeout < 5.0:
        start_timeout = 5.0

    debug_print("Инициализация CAN...")
    with can.Bus(
        interface=flashing_params["iface"],
        channel=flashing_params["channel"],
    ) as bus:
        debug_print(f"Размер прошивки: {total_size} байт")
        debug_print(f"CRC32: 0x{crc32:08X}")
        debug_print(f"CAN node_id/arbitration_id: 0x{cmd_id:03X}")
        start_payload = (
            bytes([BOOT_CMD_START, 0])
            + struct.pack("<I", total_size)
            + struct.pack("<H", crc32 & 0xFFFF)
        )
        debug_print(f"START[0]: {list(start_payload)}")
        _send_cmd_wait_ack(
            bus=bus,
            cmd_id=cmd_id,
            ack_id=ack_id,
            payload=start_payload,
            timeout_s=start_timeout,
        )
        start_payload = bytes([BOOT_CMD_START, 1]) + struct.pack("<H", (crc32 >> 16) & 0xFFFF)
        debug_print(f"START[1]: {list(start_payload)}")
        _send_cmd_wait_ack(
            bus=bus,
            cmd_id=cmd_id,
            ack_id=ack_id,
            payload=start_payload,
            timeout_s=start_timeout,
        )

        offset = 0
        while offset < total_size:
            chunk = fw_image[offset : offset + data_chunk_size]
            data_payload = bytes([BOOT_CMD_DATA]) + chunk
            debug_print(f"DATA[{offset // data_chunk_size}]: {list(data_payload)}")
            _send_cmd_wait_ack(
                bus=bus,
                cmd_id=cmd_id,
                ack_id=ack_id,
                payload=data_payload,
                timeout_s=flashing_params["ack_timeout"],
            )
            offset += len(chunk)

        debug_print("DONE")
        _send_cmd_wait_ack(
            bus=bus,
            cmd_id=cmd_id,
            ack_id=ack_id,
            payload=bytes([BOOT_CMD_DONE]),
            timeout_s=flashing_params["ack_timeout"],
        )


def _send_cmd_wait_ack(
    bus: can.BusABC,
    cmd_id: int,
    ack_id: int,
    payload: bytes,
    timeout_s: float,
) -> None:
    msg = can.Message(
        arbitration_id=cmd_id,
        is_extended_id=False,
        is_fd=True,
        bitrate_switch=False,
        data=payload,
    )
    bus.send(msg)

    deadline = time.monotonic() + timeout_s
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            pytest.fail(f"No ACK from bootloader for cmd payload {payload[:6].hex()}...")
        rx = bus.recv(timeout=remaining)
        if rx is None:
            continue
        if rx.is_extended_id or rx.arbitration_id != ack_id:
            continue
        if len(rx.data) < 1:
            pytest.fail("ACK frame received with empty payload")
        status = rx.data[0]
        if status == BL_STATUS_DONE:
            return
        if status == BL_STATUS_ERR:
            pytest.fail(f"Bootloader returned BL_STATUS_ERR for payload {payload[:6].hex()}...")
        # Ignore non-ACK frames with same ID and keep waiting.
        continue

