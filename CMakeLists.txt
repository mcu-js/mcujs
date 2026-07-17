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

# Platform selection (current release boards all use the RP2 backend)
if(NOT DEFINED MCUJS_PLATFORM)
    set(MCUJS_PLATFORM "rp2")
endif()

set(MCUJS_PLATFORM_FILE
    ${CMAKE_CURRENT_LIST_DIR}/platform/${MCUJS_PLATFORM}/platform.cmake
)
if(NOT EXISTS ${MCUJS_PLATFORM_FILE})
    message(FATAL_ERROR "Unsupported MCUJS_PLATFORM: ${MCUJS_PLATFORM}")
endif()
include(${MCUJS_PLATFORM_FILE})

project(mcujs VERSION ${MCUJS_VERSION} LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the selected platform SDK (must be after project())
mcujs_platform_sdk_init()

# Derive build ID from git
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE MCUJS_GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT MCUJS_GIT_SHA)
    set(MCUJS_GIT_SHA "unknown")
endif()
set(MCUJS_BUILD_ID "${MCUJS_VERSION}+${MCUJS_GIT_SHA}")

# Global compile definitions
add_compile_definitions(
    MCUJS_VERSION="${MCUJS_VERSION}"
    MCUJS_BUILD_ID="${MCUJS_BUILD_ID}"
    MCUJS_BOARD="${BOARD}"
    JERRYSCRIPT_VERSION="3.0.0"
)

# Add subdirectories
add_subdirectory(javascript)  # JerryScript engine
add_subdirectory(host)        # mcujs host runtime (bindings, engine wrapper)
add_subdirectory(src)

# Main executable - include jerry_port.c directly to avoid LTO stripping
add_executable(mcujs
    ${MCUJS_PLATFORM_MAIN_SOURCE}
    ${MCUJS_PLATFORM_JERRY_PORT_SOURCE}
)

target_link_libraries(mcujs PRIVATE
    mcujs_core
    mcujs_host
    mcujs_bindings
    mcujs_jerry
)

target_include_directories(mcujs PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/usb
    ${CMAKE_CURRENT_LIST_DIR}/board
    ${CMAKE_CURRENT_LIST_DIR}/board/${BOARD}
    ${CMAKE_CURRENT_LIST_DIR}/host
    ${CMAKE_CURRENT_LIST_DIR}/javascript
    ${CMAKE_CURRENT_LIST_DIR}/javascript/jerryscript-core
    ${MCUJS_PLATFORM_INCLUDE_DIRS}
)

# Set output name with version and board
set_target_properties(mcujs PROPERTIES
    OUTPUT_NAME "mcujs-${MCUJS_VERSION}-${BOARD}"
)

mcujs_platform_configure_executable(mcujs)
