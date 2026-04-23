#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "main.h"
#include "stm32g4xx_hal.h"

#define DEFAULT_NODE_ID 0x69U
#define BL_CAN_STD_ID 0x69U

#define APP_START_ADDR 0x08003000UL
#define APP_END_ADDR 0x08020000UL
#define BOOT_FLASH_PAGE_SIZE 0x800UL
#define NODE_ID_EEPROM_I2C_DEV_ADDR 0x50U
#define NODE_ID_EEPROM_MEM_ADDR 0x0000U
#define NODE_ID_MAGIC 0x424C4E49UL

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t node_id;
    uint8_t reserved[3];
} NodeIdRecord;

#define BL_STATUS_DONE 0xD0U
#define BL_STATUS_ERR  0xE0U

typedef enum {
    BOOT_CMD_START = 1,
    BOOT_CMD_DATA = 2,
    BOOT_CMD_DONE = 3,
    BOOT_CMD_GET_ID = 5
} BootCommand;

typedef enum {
    BootStateIdle = 0,
    BootStateReceiving = 1,
    BootStateVerifyCrc = 2,
    BootStateError = 3
} BootState;

typedef struct {
    BootState state;
    uint32_t expected_size;
    uint32_t expected_crc32;
    uint32_t received_size;
    uint32_t running_crc32;
    bool flash_prepared;
} BootSession;

extern FDCAN_HandleTypeDef hfdcan1;

BootSession* get_boot_session(void);
uint8_t node_id_read(void);

void configure_fdcan(FDCAN_HandleTypeDef* hfdcan, uint32_t node_id);
void start_transport(void);
void transport_loop(void);

void boot_on_start(uint32_t size, uint32_t crc32);
bool boot_on_data(uint32_t offset, const uint8_t* data, uint8_t size);
bool boot_on_done(void);
void boot_send_ack(uint8_t status);

bool is_application_valid(void);

