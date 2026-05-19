cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_PROJECT_NAME VBBoot)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME} LANGUAGES C ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})

add_executable(${CMAKE_PROJECT_NAME})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${CMAKE_PROJECT_NAME}
    SUFFIX ".elf"
)
add_subdirectory(cmake/stm32cubemx)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    stm32cubemx
)

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    "LINKER:-Map=${CMAKE_PROJECT_NAME}.map"
)

set(USER_SOURCES
    App/app.c
    App/communications.c
    App/fdcan_msp.c
    App/state_manager.c
    App/bootloader_runtime.c
    App/runtime_stubs.c
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_SOURCES}
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    -DUSE_HAL_DRIVER
    -DSTM32G4
)

add_compile_options(
    -mfloat-abi=hard   # Use hardware FPU
    -mfpu=fpv4-sp-d16  # For Cortex-M4
)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>: -Os -flto -fomit-frame-pointer>
    $<$<CONFIG:RelWithDebInfo>: -Os -g0 -flto>
    $<$<CONFIG:Debug>: -Og -g3>
)

if (NOT CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
set_source_files_properties(App/bootloader_runtime.c PROPERTIES COMPILE_OPTIONS "-fno-lto")
# communications.c accesses FDCAN message RAM (hardware-written memory).
# LTO treats message RAM as always-zero (only CPU zeroes it; hardware writes are invisible),
# causing Identifier checks and boot_process_command to be eliminated as dead code.
set_source_files_properties(App/communications.c PROPERTIES COMPILE_OPTIONS "-fno-lto")
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:-flto>
    $<$<CONFIG:RelWithDebInfo>:-flto>
    $<$<CONFIG:Debug>:-flto>
)

set(
    CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -mfloat-abi=hard -mfpu=fpv4-sp-d16"
)

add_custom_command(
    POST_BUILD
    TARGET ${CMAKE_PROJECT_NAME}
    COMMAND arm-none-eabi-objcopy -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_COMMAND}
            -DBIN_FILE=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
            -DMAX_BOOT_SIZE=12000
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_boot_size.cmake
)

