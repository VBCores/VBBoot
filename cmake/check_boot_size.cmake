if(NOT DEFINED BIN_FILE)
    message(FATAL_ERROR "BIN_FILE is not provided")
endif()

if(NOT DEFINED MAX_BOOT_SIZE)
    message(FATAL_ERROR "MAX_BOOT_SIZE is not provided")
endif()

file(SIZE "${BIN_FILE}" BOOT_BIN_SIZE)
message(STATUS "Bootloader binary size: ${BOOT_BIN_SIZE} bytes")

if(BOOT_BIN_SIZE GREATER MAX_BOOT_SIZE)
    message(FATAL_ERROR "Bootloader exceeds ${MAX_BOOT_SIZE} bytes: ${BOOT_BIN_SIZE} bytes")
endif()
