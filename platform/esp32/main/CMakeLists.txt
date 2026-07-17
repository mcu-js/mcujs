cmake_minimum_required(VERSION 3.16)

get_filename_component(MCUJS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(JERRY_ROOT "$ENV{JERRYSCRIPT_PATH}")
set(JERRY_BUILD_DIR "${CMAKE_BINARY_DIR}/jerry")

if(NOT EXISTS "${JERRY_ROOT}/tools/build.py")
    message(FATAL_ERROR "Set JERRYSCRIPT_PATH to the pinned JerryScript v3.0.0 checkout")
endif()

set(MCUJS_SOURCES
    main.c
    jerry_port.c
    repl.c
    usb_cdc.c
    bindings/board.c
    bindings/gpio.c
    bindings/process.c
    bindings/timers.c
    ${MCUJS_ROOT}/host/engine.c
    ${MCUJS_ROOT}/host/bindings/bindings.c
    ${MCUJS_ROOT}/host/bindings/console.c
)

idf_component_register(
    SRCS ${MCUJS_SOURCES}
    INCLUDE_DIRS
        .
        bindings
        ${MCUJS_ROOT}/host
        ${MCUJS_ROOT}/host/bindings
        ${MCUJS_ROOT}/src
        ${MCUJS_ROOT}/src/usb
        ${JERRY_ROOT}/jerry-core/include
    REQUIRES esp_driver_gpio esp_driver_usb_serial_jtag esp_timer esp_system heap log freertos vfs
)

# IDF evaluates component files once on the host to discover requirements and
# again with the target toolchain. Build/import Jerry only in the latter pass.
if(NOT CMAKE_CROSSCOMPILING)
    return()
endif()

string(REPLACE ";" " " JERRY_COMPILE_FLAGS "${COMPILE_OPTIONS};${C_COMPILE_OPTIONS}")
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
    MCUJS_PLATFORM_NAME="esp32"
    MCUJS_PLATFORM_ESP32=1
    JERRYSCRIPT_VERSION="3.0.0"
    MCUJS_FEATURE_MODULE_LOADER=0
    MCUJS_FEATURE_CONSOLE=1
    MCUJS_FEATURE_TIMERS=1
    MCUJS_FEATURE_BOARD=1
    MCUJS_FEATURE_GPIO=1
    MCUJS_FEATURE_PWM=0
    MCUJS_FEATURE_I2C=0
    MCUJS_FEATURE_SPI=0
    MCUJS_FEATURE_ADC=0
    MCUJS_FEATURE_NEOPIXEL=0
    MCUJS_FEATURE_PROCESS=1
    MCUJS_FEATURE_REQUIRE=0
    MCUJS_FEATURE_GRAPHICS=0
    MCUJS_FEATURE_SCREEN=0
)
