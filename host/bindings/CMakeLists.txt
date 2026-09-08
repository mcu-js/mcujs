cmake_minimum_required(VERSION 3.13)

# Base source files for JavaScript API bindings
set(BINDINGS_SOURCES
    bindings.c
    validation.c
    i2c_options.c
    spi_options.c
    neopixel_options.c
    pwm_policy.c
    board_registry.c
    console.c
    ${MCUJS_PLATFORM_HARDWARE_BINDING_SOURCES}
    fs.c
    process.c
    require.c
    graphics.c
    image.c
    ${MCUJS_PLATFORM_USB_BINDING_SOURCES}
    screen.c
    $ENV{PICOJPEG_PATH}/picojpeg.c
    ${MCUJS_PLATFORM_OPTIONAL_BINDING_SOURCES}
)

# JavaScript API bindings library
add_library(mcujs_bindings STATIC ${BINDINGS_SOURCES})
if(MCUJS_EXPERIMENTAL_CANVAS)
    # Embed the reviewed JS source in the experimental UF2; no FS installation.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/lib/canvas.js")
    file(READ "${CMAKE_SOURCE_DIR}/lib/canvas.js" CANVAS_SOURCE_HEX HEX)
    string(REGEX REPLACE "(..)" "0x\\1," CANVAS_SOURCE_BYTES "${CANVAS_SOURCE_HEX}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/canvas_source.h"
        "static const jerry_char_t canvas_source[] = {${CANVAS_SOURCE_BYTES}};\n")
    target_sources(mcujs_bindings PRIVATE canvas_native.c canvas_renderer.c canvas_module.c)
    target_include_directories(mcujs_bindings PRIVATE $ENV{CTX_PATH})
endif()

target_include_directories(mcujs_bindings PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/usb
    ${CMAKE_SOURCE_DIR}/src/filesystem
    ${CMAKE_SOURCE_DIR}/board
    ${CMAKE_SOURCE_DIR}/board/${BOARD}
    ${CMAKE_SOURCE_DIR}/javascript
    ${CMAKE_SOURCE_DIR}/javascript/jerryscript-core
    $ENV{PICOJPEG_PATH}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${MCUJS_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(mcujs_bindings PUBLIC
    mcujs_jerry
)

mcujs_platform_configure_bindings(mcujs_bindings)
