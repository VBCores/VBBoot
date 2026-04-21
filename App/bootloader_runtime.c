#include "bootloader_runtime.h"

#include "app.h"

#define CRC32_POLY 0xEDB88320UL

bool boot_prepare_flash(uint32_t firmware_size) {
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status;

    if ((firmware_size == 0U) || ((APP_START_ADDR + firmware_size) > APP_END_ADDR)) {
        return false;
    }

    HAL_FLASH_Unlock();

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.Page = (APP_START_ADDR - FLASH_BASE) / BOOT_FLASH_PAGE_SIZE;
    erase.NbPages = (firmware_size + BOOT_FLASH_PAGE_SIZE - 1U) / BOOT_FLASH_PAGE_SIZE;

    status = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();

    return status == HAL_OK;
}

bool boot_write_chunk(uint32_t offset, const uint8_t* data, uint8_t len, uint32_t firmware_size) {
    uint32_t write_addr;
    uint32_t i;
    bool is_last;

    if ((data == NULL) || (len == 0U)) {
        return false;
    }
    if (offset != get_boot_session()->received_size) {
        return false;
    }
    if ((offset + len) > firmware_size) {
        return false;
    }

    write_addr = APP_START_ADDR + offset;
    is_last = ((offset + len) == firmware_size);
    if (((write_addr & 0x7U) != 0U) || ((!is_last) && ((len & 0x7U) != 0U))) {
        return false;
    }

    HAL_FLASH_Unlock();
    i = 0U;
    while (i < len) {
        uint64_t dw = 0xFFFFFFFFFFFFFFFFULL;
        uint8_t* dst = (uint8_t*)&dw;
        uint8_t copy = ((len - i) >= 8U) ? 8U : (uint8_t)(len - i);
        uint8_t j;
        for (j = 0U; j < copy; ++j) {
            dst[j] = data[i + j];
        }
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, write_addr + i, dw) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
        i += copy;
    }
    HAL_FLASH_Lock();
    return true;
}

uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    uint32_t out = crc;
    size_t i;

    for (i = 0U; i < len; ++i) {
        uint8_t bit;
        out ^= data[i];
        for (bit = 0U; bit < 8U; ++bit) {
            uint32_t mask = (uint32_t)(-(int32_t)(out & 1U));
            out = (out >> 1U) ^ (CRC32_POLY & mask);
        }
    }
    return out;
}

void boot_jump_to_application(void) {
    uint32_t app_sp = *(const uint32_t*)APP_START_ADDR;
    uint32_t app_reset = *(const uint32_t*)(APP_START_ADDR + 4U);
    void (*entry)(void) = (void (*)(void))app_reset;

    __disable_irq();
    HAL_DeInit();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    SCB->VTOR = APP_START_ADDR;
    __set_MSP(app_sp);
    entry();
}
