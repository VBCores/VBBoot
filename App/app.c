#include "app.h"

#include "bootloader_runtime.h"
#include "i2c.h"

static BootSession boot_session = {
    .state = BootStateIdle,
    .expected_size = 0U,
    .expected_crc32 = 0U,
    .received_size = 0U,
    .running_crc32 = 0U,
    .flash_prepared = false
};

BootSession* get_boot_session(void) {
    return &boot_session;
}

static bool is_valid_node_id(uint8_t node_id) {
    return (node_id >= 0x01U);
}

uint8_t node_id_read(void) {
    NodeIdRecord rec = {0};
    HAL_StatusTypeDef status;

    if (HAL_I2C_IsDeviceReady(&hi2c2, NODE_ID_EEPROM_I2C_DEV_ADDR << 1U, 2U, 100U) != HAL_OK) {
        return DEFAULT_NODE_ID;
    }

    status = HAL_I2C_Mem_Read(
        &hi2c2,
        NODE_ID_EEPROM_I2C_DEV_ADDR << 1U,
        NODE_ID_EEPROM_MEM_ADDR,
        I2C_MEMADD_SIZE_16BIT,
        (uint8_t*)&rec,
        (uint16_t)sizeof(rec),
        100U
    );

    if ((status == HAL_OK) && (rec.magic == NODE_ID_MAGIC) && is_valid_node_id(rec.node_id)) {
        return rec.node_id;
    }
    return DEFAULT_NODE_ID;
}

void boot_on_start(uint32_t size, uint32_t crc32) {
    boot_session.expected_size = 0U;
    boot_session.expected_crc32 = 0U;
    boot_session.received_size = 0U;
    boot_session.running_crc32 = 0U;
    boot_session.flash_prepared = false;
    boot_session.state = BootStateIdle;
    boot_session.expected_size = size;
    boot_session.expected_crc32 = crc32;
    boot_session.running_crc32 = 0xFFFFFFFFUL;
    if (boot_prepare_flash(size)) {
        boot_session.flash_prepared = true;
        boot_session.state = BootStateReceiving;
    } else {
        boot_session.state = BootStateError;
    }
}

bool boot_on_data(uint32_t offset, const uint8_t* data, uint8_t size) {
    if ((boot_session.state != BootStateReceiving) || (!boot_session.flash_prepared)) {
        return false;
    }
    if (!boot_write_chunk(offset, data, size, boot_session.expected_size)) {
        boot_session.state = BootStateError;
        return false;
    }
    boot_session.running_crc32 = crc32_update(boot_session.running_crc32, data, size);
    boot_session.received_size += size;
    return true;
}

bool boot_on_done(void) {
    uint32_t final_crc;
    bool ok;

    if (boot_session.state != BootStateReceiving) {
        return false;
    }
    boot_session.state = BootStateVerifyCrc;
    final_crc = boot_session.running_crc32 ^ 0xFFFFFFFFUL;
    ok = (boot_session.received_size == boot_session.expected_size) && (final_crc == boot_session.expected_crc32);
    if (!ok) {
        boot_session.state = BootStateError;
        return false;
    }
    return true;
}

bool is_application_valid(void) {
    uint32_t app_sp = *(const uint32_t*)APP_START_ADDR;
    uint32_t app_reset = *(const uint32_t*)(APP_START_ADDR + 4U);

    if ((app_sp == 0xFFFFFFFFUL) || (app_reset == 0xFFFFFFFFUL)) {
        return false;
    }

    return true;
}

void app(void) {
    if (is_application_valid()) {
        boot_jump_to_application();
    } else {
        start_transport();
        while (1) {
            transport_loop();
        }
    }
}
