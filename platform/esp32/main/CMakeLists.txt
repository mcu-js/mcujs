cmake_minimum_required(VERSION 3.16)

get_filename_component(MCUJS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(JERRY_ROOT "$ENV{JERRYSCRIPT_PATH}")
set(JERRY_BUILD_DIR "${CMAKE_BINARY_DIR}/jerry")

if(NOT EXISTS "${JERRY_ROOT}/tools/build.py")
    message(FATAL_ERROR "Set JERRYSCRIPT_PATH to the pinned JerryScript v3.0.0 checkout")
endif()

# Requirement discovery runs on the host first; only stage for the target pass.
if(CMAKE_CROSSCOMPILING)
    execute_process(
        COMMAND ${PYTHON} ${MCUJS_ROOT}/scripts/prepare-jerry-jobs.py
            ${JERRY_ROOT} ${CMAKE_BINARY_DIR}/jerry-source
        RESULT_VARIABLE MCUJS_JERRY_PREPARE_RESULT
    )
    if(NOT MCUJS_JERRY_PREPARE_RESULT EQUAL 0)
        message(FATAL_ERROR "Unable to stage the pinned JerryScript job-queue extension")
    endif()
    set(JERRY_ROOT "${CMAKE_BINARY_DIR}/jerry-source")
endif()

set(MCUJS_SOURCES
    board_power.c
    main.c
    boot.c
    jerry_port.c
    board_services.c
    ${MCUJS_ROOT}/src/repl.c
    ${MCUJS_ROOT}/src/usb/msc_ownership.c
    usb_cdc.c
    usb_msc.c
    usb_recovery.c
    filesystem.c
    bindings/board.c
    bindings/pin_policy.c
    bindings/gpio.c
    bindings/pwm.c
    bindings/i2c.c
    bindings/spi.c
    bindings/adc.c
    bindings/neopixel.c
    bindings/process.c
    bindings/timers.c
    ${MCUJS_ROOT}/host/engine.c
    ${MCUJS_ROOT}/host/module_loader.c
    ${MCUJS_ROOT}/host/runtime_registry.c
    ${MCUJS_ROOT}/host/bindings/bindings.c
    ${MCUJS_ROOT}/host/bindings/validation.c
    ${MCUJS_ROOT}/host/bindings/i2c_options.c
    ${MCUJS_ROOT}/host/bindings/spi_options.c
    ${MCUJS_ROOT}/host/bindings/neopixel_options.c
    ${MCUJS_ROOT}/host/bindings/pwm_policy.c
    ${MCUJS_ROOT}/host/bindings/board_registry.c
    ${MCUJS_ROOT}/host/bindings/console.c
    ${MCUJS_ROOT}/host/bindings/fs.c
    ${MCUJS_ROOT}/host/bindings/require.c
    ${MCUJS_ROOT}/host/bindings/builtin_modules.c
)

# Unqualified public buses are absent, not compiled with zero-sized buffers.
if(MCUJS_BOARD STREQUAL "waveshare_esp32s3_epaper_1.54_v2" OR MCUJS_BOARD STREQUAL "seeed_reterminal_sticky")
 list(REMOVE_ITEM MCUJS_SOURCES bindings/i2c.c bindings/spi.c)
endif()

if(MCUJS_BOARD STREQUAL "seeed_reterminal_sticky")
 list(REMOVE_ITEM MCUJS_SOURCES usb_cdc.c usb_msc.c)
 list(APPEND MCUJS_SOURCES serial_uart.c)
endif()
if(MCUJS_EXPERIMENTAL_CANVAS)
 if(NOT MCUJS_BOARD STREQUAL "waveshare_esp32s3_epaper_1.54_v2" AND NOT MCUJS_BOARD STREQUAL "seeed_reterminal_sticky")
  message(FATAL_ERROR "ESP Canvas only supports qualified experimental ePaper targets")
 endif()
 file(SHA256 "$ENV{CTX_PATH}/ctx.h" CTX_HEADER_SHA256)
 if(NOT CTX_HEADER_SHA256 STREQUAL "bf2b18bcea31bf191f7e82c9beef98bd717bd5ccd635908f07ab8a8cf1698bb3")
  message(FATAL_ERROR "Canvas requires pinned ctx 0.1.18")
 endif()
 list(APPEND MCUJS_SOURCES ${MCUJS_ROOT}/host/bindings/canvas_native.c
  ${MCUJS_ROOT}/host/bindings/canvas_renderer.c)
 if(MCUJS_BOARD STREQUAL "seeed_reterminal_sticky")
  list(APPEND MCUJS_SOURCES canvas_display_sticky.c)
 else()
  list(APPEND MCUJS_SOURCES canvas_display_epaper154.c)
 endif()
endif()

# IDF requirement discovery has no target board/profile variables yet.
if(CMAKE_CROSSCOMPILING)
include("${MCUJS_ROOT}/cmake/capability-manifest.cmake")
set(MCUJS_CONFIGURED_DISPLAY OFF)
if(MCUJS_EXPERIMENTAL_CANVAS AND CMAKE_CROSSCOMPILING)
 set(MCUJS_CONFIGURED_DISPLAY ON)
endif()
mcujs_write_capability_manifest("${MCUJS_ROOT}" "${MCUJS_BOARD}"
 "${CMAKE_BINARY_DIR}/mcujs-${MCUJS_VERSION}-${MCUJS_BOARD}.capabilities.json" "${MCUJS_CONFIGURED_DISPLAY}")
endif()

set(MCUJS_PACKAGED_MODULES events devices button)
if(MCUJS_EXPERIMENTAL_CANVAS)
 list(APPEND MCUJS_PACKAGED_MODULES canvas displays/st7789)
endif()
foreach(module ${MCUJS_PACKAGED_MODULES})
 get_filename_component(name "${module}" NAME)
 set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MCUJS_ROOT}/lib/${module}.js")
 file(READ "${MCUJS_ROOT}/lib/${module}.js" source_hex HEX)
 string(REGEX REPLACE "(..)" "0x\\1," source_bytes "${source_hex}")
 file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${name}_source.h" "static const jerry_char_t ${name}_source[] = {${source_bytes}};\n")
endforeach()

idf_component_register(
    SRCS ${MCUJS_SOURCES}
    INCLUDE_DIRS
        ${CMAKE_CURRENT_BINARY_DIR}
        .
        bindings
        ${MCUJS_ROOT}/host
        ${MCUJS_ROOT}/host/bindings
        ${MCUJS_ROOT}/board
        ${MCUJS_ROOT}/src
        ${MCUJS_ROOT}/src/filesystem
        ${MCUJS_ROOT}/src/usb
        ${JERRY_ROOT}/jerry-core/include
    REQUIRES driver esp_adc esp_driver_gpio esp_driver_i2c esp_driver_ledc
        esp_driver_spi esp_driver_tsens esp_driver_uart esp_timer esp_system fatfs
        wear_levelling nvs_flash heap log freertos vfs espressif__led_strip esp_psram
)

if(MCUJS_EXPERIMENTAL_CANVAS AND CMAKE_CROSSCOMPILING)
 target_include_directories(${COMPONENT_LIB} PRIVATE "$ENV{CTX_PATH}" "${CMAKE_CURRENT_BINARY_DIR}")
 target_compile_definitions(${COMPONENT_LIB} PRIVATE MCUJS_EXPERIMENTAL_CANVAS=1)
 if(MCUJS_BOARD STREQUAL "seeed_reterminal_sticky")
  target_compile_definitions(${COMPONENT_LIB} PRIVATE MCUJS_CANVAS_STICKY=1)
 else()
  target_compile_definitions(${COMPONENT_LIB} PRIVATE MCUJS_CANVAS_EPAPER154=1)
 endif()
endif()
# IDF evaluates component files once on the host to discover requirements and
# again with the target toolchain. Build/import Jerry only in the latter pass.
if(NOT CMAKE_CROSSCOMPILING)
    return()
endif()

set(JERRY_REPRO_COMPILE_FLAGS
    "-fdebug-prefix-map=${MCUJS_ROOT}=/MCUJS"
    "-fmacro-prefix-map=${MCUJS_ROOT}=/MCUJS"
    "-fdebug-prefix-map=${JERRY_ROOT}=/JERRYSCRIPT"
    "-fmacro-prefix-map=${JERRY_ROOT}=/JERRYSCRIPT"
    "-fdebug-prefix-map=$ENV{IDF_TOOLS_PATH}=/IDF_TOOLS"
    "-fmacro-prefix-map=$ENV{IDF_TOOLS_PATH}=/IDF_TOOLS"
    "-fdebug-prefix-map=${JERRY_BUILD_DIR}=/IDF_BUILD/jerry"
    "-fmacro-prefix-map=${JERRY_BUILD_DIR}=/IDF_BUILD/jerry"
)
string(REPLACE ";" " " JERRY_COMPILE_FLAGS
       "${COMPILE_OPTIONS};${C_COMPILE_OPTIONS};${JERRY_REPRO_COMPILE_FLAGS}")
# Board policy is resolved by ../js-memory.cmake for both engine and port.
execute_process(
    COMMAND ${PYTHON} ${JERRY_ROOT}/tools/build.py
        --clean
        --builddir=${JERRY_BUILD_DIR}
        --lto=OFF
        --strip=OFF
        --jerry-cmdline=OFF
        --jerry-ext=OFF
        --jerry-math=OFF
        --jerry-port=OFF
        --amalgam=ON
        --mem-heap=${MCUJS_JS_HEAP_KIB}
        --external-context=${MCUJS_JERRY_EXTERNAL_CONTEXT}
        --cpointer-32bit=OFF
        --profile=es.next
        --error-messages=ON
        --line-info=ON
        --vm-throw=ON
        --compile-flag=${JERRY_COMPILE_FLAGS}
        --toolchain=${CMAKE_TOOLCHAIN_FILE}
    WORKING_DIRECTORY ${JERRY_ROOT}
    RESULT_VARIABLE JERRY_BUILD_RESULT
)
if(NOT JERRY_BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "JerryScript cross-build failed: ${JERRY_BUILD_RESULT}")
endif()

add_library(mcujs_jerry_core STATIC IMPORTED GLOBAL)
set_target_properties(mcujs_jerry_core PROPERTIES
    IMPORTED_LOCATION "${JERRY_BUILD_DIR}/lib/libjerry-core.a"
)
# JerryScript calls the port functions supplied by this component. Keep the
# two archives in the same cyclic link relationship used by Jerry upstream's
# ESP-IDF target so static archive ordering cannot discard the port objects.
target_link_libraries(mcujs_jerry_core INTERFACE ${COMPONENT_LIB})
target_link_libraries(${COMPONENT_LIB} INTERFACE mcujs_jerry_core m)
if(MCUJS_EXPERIMENTAL_EPAPER_PARTIAL)
 target_compile_definitions(${COMPONENT_LIB} PRIVATE MCUJS_EXPERIMENTAL_EPAPER_PARTIAL=1)
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MCUJS_VERSION="${MCUJS_VERSION}"
    MCUJS_BUILD_ID="${MCUJS_BUILD_ID}"
    MCUJS_BOARD="${MCUJS_BOARD}"
    ${MCUJS_BOARD_DEFINE}=1
    MCUJS_PLATFORM_NAME="esp32"
    MCUJS_PLATFORM_ESP32=1
    MCUJS_JS_HEAP_KIB=${MCUJS_JS_HEAP_KIB}
    MCUJS_JS_HEAP_EXTERNAL=${MCUJS_JS_HEAP_EXTERNAL}
    JERRYSCRIPT_VERSION="3.0.0"
)
