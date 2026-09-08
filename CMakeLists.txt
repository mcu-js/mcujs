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

option(MCUJS_EXPERIMENTAL_CANVAS "Experimental PiZero Canvas subset (not a release API)" OFF)
if(MCUJS_EXPERIMENTAL_CANVAS)
    if(NOT MCUJS_HAS_DVI)
        message(FATAL_ERROR "Experimental Canvas currently requires the PiZero DVI backend")
    endif()
    if(NOT EXISTS "$ENV{CTX_PATH}/ctx.h")
        message(FATAL_ERROR "Canvas requires ctx 0.1.18 in the Docker environment (CTX_PATH)")
    endif()
    file(SHA256 "$ENV{CTX_PATH}/ctx.h" CTX_HEADER_SHA256)
    if(NOT CTX_HEADER_SHA256 STREQUAL "bf2b18bcea31bf191f7e82c9beef98bd717bd5ccd635908f07ab8a8cf1698bb3")
        message(FATAL_ERROR "Unexpected ctx header: use the pinned 0.1.18 dependency")
    endif()
    add_compile_definitions(MCUJS_EXPERIMENTAL_CANVAS=1 PICO_STACK_SIZE=12288)
endif()

project(mcujs VERSION ${MCUJS_VERSION} LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the selected platform SDK (must be after project())
mcujs_platform_sdk_init()

# Docker snapshots of linked worktrees cannot resolve their external .git file,
# so the host passes its already-validated source SHA into the container.
set(MCUJS_GIT_SHA "$ENV{MCUJS_BUILD_GIT_SHA}")
if(NOT MCUJS_GIT_SHA)
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE MCUJS_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
string(LENGTH "${MCUJS_GIT_SHA}" MCUJS_GIT_SHA_LENGTH)
if(NOT MCUJS_GIT_SHA MATCHES "^[0-9a-f]+$" OR
   MCUJS_GIT_SHA_LENGTH LESS 7 OR MCUJS_GIT_SHA_LENGTH GREATER 40)
    message(FATAL_ERROR "Could not determine a valid source Git SHA for build identity")
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
if(MCUJS_EXPERIMENTAL_CANVAS)
    pico_set_linker_script(mcujs ${CMAKE_CURRENT_LIST_DIR}/platform/rp2/canvas_memmap.ld)
endif()
