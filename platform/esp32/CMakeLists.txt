cmake_minimum_required(VERSION 3.16)

get_filename_component(MCUJS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
file(READ "${MCUJS_ROOT}/version.txt" MCUJS_VERSION)
string(STRIP "${MCUJS_VERSION}" MCUJS_VERSION)
# A Docker snapshot of a linked worktree cannot resolve its external .git file,
# so the non-flashing container lane passes the already-validated source SHA.
set(MCUJS_GIT_SHA "$ENV{MCUJS_BUILD_GIT_SHA}")
if(NOT MCUJS_GIT_SHA)
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY "${MCUJS_ROOT}"
        OUTPUT_VARIABLE MCUJS_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT MCUJS_GIT_SHA)
    set(MCUJS_GIT_SHA "unknown")
endif()
set(MCUJS_BUILD_ID "${MCUJS_VERSION}+${MCUJS_GIT_SHA}")
# Keep ESP-IDF's application descriptor independent of Git worktree shape and
# aligned with the build identity printed by the runtime.
set(PROJECT_VER "${MCUJS_BUILD_ID}")

if(DEFINED IDF_TARGET AND NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "The Milestone 2 backend supports only IDF_TARGET=esp32s3")
endif()
set(IDF_TARGET "esp32s3" CACHE STRING "ESP-IDF target" FORCE)
set(MCUJS_BOARD "$ENV{MCUJS_BOARD}" CACHE STRING "MCU.js ESP board")
if(NOT MCUJS_BOARD)
 set(MCUJS_BOARD "seeed_xiao_esp32s3")
endif()
if(MCUJS_BOARD STREQUAL "waveshare_esp32s3_epaper_1.54_v2")
 set(MCUJS_BOARD_DEFINE MCUJS_BOARD_WAVESHARE_ESP32S3_EPAPER_1_54_V2)
elseif(MCUJS_BOARD STREQUAL "seeed_xiao_esp32s3")
 set(MCUJS_BOARD_DEFINE MCUJS_BOARD_SEEED_XIAO_ESP32S3)
else()
 message(FATAL_ERROR "Unsupported ESP board: ${MCUJS_BOARD}")
endif()
option(MCUJS_EXPERIMENTAL_CANVAS "Experimental shared Canvas" OFF)
if("$ENV{MCUJS_EXPERIMENTAL_CANVAS}" STREQUAL "1")
 set(MCUJS_EXPERIMENTAL_CANVAS ON)
endif()
set(COMPONENTS main)
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")

if(MCUJS_BOARD STREQUAL "waveshare_esp32s3_epaper_1.54_v2")
 list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.epaper154")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mcujs-esp32s3)

# ESP-IDF's generic "boot default" lookup prefers a factory app whenever one
# exists. Our factory partition intentionally contains TinyUF2; MCU.js belongs
# in ota_0. Retarget the generated flash metadata while retaining TinyUF2 as a
# bootable factory image for the later software-entry milestone.
if(MCUJS_BOARD STREQUAL "seeed_xiao_esp32s3")
partition_table_get_partition_info(MCUJS_APP_OFFSET "--partition-name ota_0" "offset")
partition_table_get_partition_info(IDF_DEFAULT_APP_OFFSET "--partition-boot-default" "offset")
if(NOT MCUJS_APP_OFFSET STREQUAL "0x10000")
    message(FATAL_ERROR "Expected ota_0 at 0x10000, got ${MCUJS_APP_OFFSET}")
endif()

foreach(target_name flash app-flash)
    foreach(property_name FLASH_FILE FLASH_ENTRY IMAGES)
        get_property(property_value TARGET ${target_name} PROPERTY ${property_name})
        if(property_value)
            string(REPLACE "${IDF_DEFAULT_APP_OFFSET}" "${MCUJS_APP_OFFSET}"
                   property_value "${property_value}")
            set_property(TARGET ${target_name} PROPERTY ${property_name} ${property_value})
        endif()
    endforeach()
endforeach()
endif()
