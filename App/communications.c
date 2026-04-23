#include "app.h"

static uint8_t g_rx_data[64];
static uint32_t g_write_offset = 0U;
static bool g_start_part0_ready = false;
static uint32_t g_start_size = 0U;
static uint16_t g_start_crc_low = 0U;
static uint8_t g_data_buf[8];
static uint8_t g_data_buf_len = 0U;
static uint32_t g_boot_can_id = BL_CAN_STD_ID;

static uint32_t read_u32_le(const uint8_t* p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) |
           ((uint32_t)p[3] << 24U);
}

static size_t fdcan_dlc_to_len(uint32_t dlc) {
    switch (dlc & 0xFU) {
    case FDCAN_DLC_BYTES_0: return 0U;
    case FDCAN_DLC_BYTES_1: return 1U;
    case FDCAN_DLC_BYTES_2: return 2U;
    case FDCAN_DLC_BYTES_3: return 3U;
    case FDCAN_DLC_BYTES_4: return 4U;
    case FDCAN_DLC_BYTES_5: return 5U;
    case FDCAN_DLC_BYTES_6: return 6U;
    case FDCAN_DLC_BYTES_7: return 7U;
    case FDCAN_DLC_BYTES_8: return 8U;
    case FDCAN_DLC_BYTES_12: return 12U;
    case FDCAN_DLC_BYTES_16: return 16U;
    case FDCAN_DLC_BYTES_20: return 20U;
    case FDCAN_DLC_BYTES_24: return 24U;
    case FDCAN_DLC_BYTES_32: return 32U;
    case FDCAN_DLC_BYTES_48: return 48U;
    case FDCAN_DLC_BYTES_64: return 64U;
    default:                return 0U;
    }
}

static uint32_t fdcan_len_to_dlc(size_t len) {
    if (len > 8U) {
        len = 8U;
    }
    return (uint32_t)len;
}

static uint32_t boot_can_id(void) {
    return g_boot_can_id;
}

void boot_send_ack(uint8_t status) {
    FDCAN_TxHeaderTypeDef txh = {0};
    uint8_t payload[1];
    payload[0] = status;
    txh.Identifier = boot_can_id();
    txh.IdType = FDCAN_STANDARD_ID;
    txh.TxFrameType = FDCAN_DATA_FRAME;
    txh.DataLength = fdcan_len_to_dlc(1U);
    txh.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txh.BitRateSwitch = FDCAN_BRS_OFF;
    txh.FDFormat = FDCAN_CLASSIC_CAN;
    txh.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    txh.MessageMarker = 0U;
    (void)HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txh, payload);
}

static void process_command(const uint8_t* p, size_t payload_size) {
    uint8_t cmd;

    if ((p == NULL) || (payload_size < 1U)) {
        return;
    }

    cmd = p[0];
    switch ((BootCommand)cmd) {
    case BOOT_CMD_START: {
        uint8_t part;
        if (payload_size < 2U) {
            boot_send_ack(BL_STATUS_ERR);
            break;
        }
        part = p[1];
        if (part == 0U) {
            if (payload_size < 8U) {
                boot_send_ack(BL_STATUS_ERR);
                return;
            }
            g_start_size = read_u32_le(&p[2]);
            g_start_crc_low = (uint16_t)p[6] | ((uint16_t)p[7] << 8U);
            g_start_part0_ready = true;
            g_write_offset = 0U;
            g_data_buf_len = 0U;
            boot_send_ack(BL_STATUS_DONE);
            break;
        }
        if (part == 1U) {
            uint32_t crc;
            uint16_t crc_high;
            if ((payload_size < 4U) || (!g_start_part0_ready)) {
                boot_send_ack(BL_STATUS_ERR);
                return;
            }
            crc_high = (uint16_t)p[2] | ((uint16_t)p[3] << 8U);
            crc = ((uint32_t)crc_high << 16U) | (uint32_t)g_start_crc_low;
            g_start_part0_ready = false;
            g_write_offset = 0U;
            g_data_buf_len = 0U;
            boot_on_start(g_start_size, crc);
            boot_send_ack((get_boot_session()->state == BootStateReceiving) ? BL_STATUS_DONE : BL_STATUS_ERR);
            break;
        }
        boot_send_ack(BL_STATUS_ERR);
        break;
    }
    case BOOT_CMD_DATA: {
        size_t idx;
        size_t remaining;
        bool ok = true;
        if (payload_size < 2U) {
            boot_send_ack(BL_STATUS_ERR);
            break;
        }
        idx = 1U;
        remaining = payload_size - 1U;
        while (remaining > 0U) {
            size_t space = 8U - g_data_buf_len;
            size_t take = (remaining < space) ? remaining : space;
            size_t j;
            for (j = 0U; j < take; ++j) {
                g_data_buf[g_data_buf_len + j] = p[idx + j];
            }
            g_data_buf_len = (uint8_t)(g_data_buf_len + take);
            idx += take;
            remaining -= take;
            if (g_data_buf_len == 8U) {
                if (!boot_on_data(g_write_offset, g_data_buf, 8U)) {
                    ok = false;
                    boot_send_ack(BL_STATUS_ERR);
                    break;
                }
                g_write_offset += 8U;
                g_data_buf_len = 0U;
            }
        }
        if (ok) {
            boot_send_ack(BL_STATUS_DONE);
        }
        break;
    }
    case BOOT_CMD_DONE:
        if (g_data_buf_len > 0U) {
            BootSession* session = get_boot_session();
            if ((g_write_offset + g_data_buf_len) != session->expected_size) {
                boot_send_ack(BL_STATUS_ERR);
                break;
            }
            if (!boot_on_data(g_write_offset, g_data_buf, g_data_buf_len)) {
                boot_send_ack(BL_STATUS_ERR);
                break;
            }
            g_write_offset += g_data_buf_len;
            g_data_buf_len = 0U;
        }
        if (boot_on_done()) {
            boot_send_ack(BL_STATUS_DONE);
            while (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) != 3U) {
            }
            boot_jump_to_application();
        } else {
            boot_send_ack(BL_STATUS_ERR);
        }
        break;
    default:
        boot_send_ack(BL_STATUS_ERR);
        break;
    }
}


void start_transport(void) {
    g_boot_can_id = (uint32_t)node_id_read();
    configure_fdcan(&hfdcan1, g_boot_can_id);
    (void)HAL_FDCAN_Start(&hfdcan1);
}

void transport_loop(void) {
    while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) != 0U) {
        FDCAN_RxHeaderTypeDef rxh = {0};
        size_t rx_len;
        if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rxh, g_rx_data) != HAL_OK) {
            break;
        }
        if ((rxh.RxFrameType != FDCAN_DATA_FRAME) ||
            (rxh.IdType != FDCAN_STANDARD_ID) ||
            (rxh.Identifier != boot_can_id())) {
            continue;
        }
        rx_len = fdcan_dlc_to_len(rxh.DataLength);
        if (rx_len > sizeof(g_rx_data)) {
            continue;
        }
        process_command(g_rx_data, rx_len);
    }
}
