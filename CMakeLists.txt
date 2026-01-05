cmake_minimum_required(VERSION 3.13)

# Read version from file
file(READ "${CMAKE_CURRENT_LIST_DIR}/version.txt" MCUJS_VERSION)
string(STRIP "${MCUJS_VERSION}" MCUJS_VERSION)

# Board selection (default to pico)
if(NOT DEFINED BOARD)
    set(BOARD "pico")
endif()

message(STATUS "Building mcujs v${MCUJS_VERSION} for board: ${BOARD}")

# Include board-specific configuration
include(${CMAKE_CURRENT_LIST_DIR}/board/${BOARD}/board_config.cmake)

# Force picotool to be fetched from git (required for SDK 2.2.0+)
set(PICOTOOL_FORCE_FETCH_FROM_GIT ON CACHE BOOL "Force picotool fetch from git")

# Initialize Pico SDK
include(pico_sdk_import.cmake)

project(mcujs VERSION ${MCUJS_VERSION} LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize Pico SDK (must be after project())
pico_sdk_init()

# Global compile definitions
add_compile_definitions(
    MCUJS_VERSION="${MCUJS_VERSION}"
    MCUJS_BOARD="${BOARD}"
    JERRYSCRIPT_VERSION="3.0.0"
    PICO_SDK_VERSION="2.2.0"
    TINYUSB_VERSION="0.18.0"
)

# Add subdirectories
add_subdirectory(javascript)  # JerryScript engine
add_subdirectory(host)        # mcujs host runtime (bindings, engine wrapper)
add_subdirectory(src)

# Main executable - include jerry_port.c directly to avoid LTO stripping
add_executable(mcujs
    src/main.c
    ${JERRY_PORT_SOURCE}
)

target_link_libraries(mcujs
    mcujs_core
    mcujs_host
    mcujs_bindings
    mcujs_jerry
    pico_stdlib
    hardware_flash
    hardware_sync
    hardware_gpio
    hardware_pwm
    hardware_i2c
    hardware_spi
    tinyusb_device
    tinyusb_board
)

target_include_directories(mcujs PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/usb
    ${CMAKE_CURRENT_LIST_DIR}/board
    ${CMAKE_CURRENT_LIST_DIR}/board/${BOARD}
    ${CMAKE_CURRENT_LIST_DIR}/host
    ${CMAKE_CURRENT_LIST_DIR}/javascript
    ${CMAKE_CURRENT_LIST_DIR}/javascript/jerryscript-core
)

# Disable SDK's USB stdio - we use TinyUSB directly
pico_enable_stdio_usb(mcujs 0)
pico_enable_stdio_uart(mcujs 0)

# Set output name with version and board
set_target_properties(mcujs PROPERTIES
    OUTPUT_NAME "mcujs-${MCUJS_VERSION}-${BOARD}"
)

# Generate UF2 and other outputs
pico_add_extra_outputs(mcujs)

# Copy UF2 to build directory in project root
add_custom_command(TARGET mcujs POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/mcujs-${MCUJS_VERSION}-${BOARD}.uf2
        ${CMAKE_CURRENT_LIST_DIR}/build/mcujs-${MCUJS_VERSION}-${BOARD}.uf2
    COMMENT "Copying UF2 to build directory"
)
