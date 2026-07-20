cmake_minimum_required(VERSION 3.16)

get_filename_component(MCUJS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(JERRY_ROOT "$ENV{JERRYSCRIPT_PATH}")
set(JERRY_BUILD_DIR "${CMAKE_BINARY_DIR}/jerry")

if(NOT EXISTS "${JERRY_ROOT}/tools/build.py")
    message(FATAL_ERROR "Set JERRYSCRIPT_PATH to the pinned JerryScript v3.0.0 checkout")
endif()

set(MCUJS_SOURCES
    main.c
    boot.c
    jerry_port.c
    board_services.c
    ${MCUJS_ROOT}/src/repl.c
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
)

idf_component_register(
    SRCS ${MCUJS_SOURCES}
    INCLUDE_DIRS
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
        wear_levelling nvs_flash heap log freertos vfs espressif__led_strip
)

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
        --mem-heap=64
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

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MCUJS_VERSION="${MCUJS_VERSION}"
    MCUJS_BUILD_ID="${MCUJS_BUILD_ID}"
    MCUJS_BOARD="seeed_xiao_esp32s3"
    MCUJS_BOARD_SEEED_XIAO_ESP32S3=1
    MCUJS_PLATFORM_NAME="esp32"
    MCUJS_PLATFORM_ESP32=1
    JERRYSCRIPT_VERSION="3.0.0"
)
