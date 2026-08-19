set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_ID GNU)

# Some default GCC settings
# arm-none-eabi- must be part of path environment
set(TOOLCHAIN_PREFIX                arm-none-eabi-)

set(CMAKE_C_COMPILER                ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER              ${CMAKE_C_COMPILER})
set(CMAKE_LINKER                    ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY                   ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE                      ${TOOLCHAIN_PREFIX}size)

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -fdata-sections -ffunction-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector")
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3")
endif()
if(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -g0")
endif()

set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")

set(CMAKE_C_LINK_FLAGS "${TARGET_FLAGS}")
if(NOT DEFINED BOOTLOADER_LINKER_SCRIPT)
    set(BOOTLOADER_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/stm32g431vbtx_flash.ld")
endif()
if(BOOTLOADER_MCU STREQUAL "STM32G474xx")
    file(READ "${CMAKE_SOURCE_DIR}/stm32g431vbtx_flash.ld" BOOTLOADER_LINKER_CONTENT)
    string(REPLACE "RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 32K" "RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K" BOOTLOADER_LINKER_CONTENT "${BOOTLOADER_LINKER_CONTENT}")
    string(REPLACE "FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 128K" "FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 512K" BOOTLOADER_LINKER_CONTENT "${BOOTLOADER_LINKER_CONTENT}")
    set(BOOTLOADER_LINKER_SCRIPT "${CMAKE_BINARY_DIR}/stm32g474_boot_flash.ld")
    file(WRITE "${BOOTLOADER_LINKER_SCRIPT}" "${BOOTLOADER_LINKER_CONTENT}")
endif()
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -T \"${BOOTLOADER_LINKER_SCRIPT}\"")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -nostdlib -Wl,--start-group -lgcc -Wl,--end-group")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--print-memory-usage")
