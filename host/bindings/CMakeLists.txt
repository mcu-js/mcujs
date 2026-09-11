cmake_minimum_required(VERSION 3.13)

# Base source files for JavaScript API bindings
set(MCUJS_PICOJPEG_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/picojpeg.c")
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
    builtin_modules.c
    graphics.c
    image.c
    jpeg.c
    ${MCUJS_PLATFORM_USB_BINDING_SOURCES}
    screen.c
    ${MCUJS_PICOJPEG_SOURCE}
    ${MCUJS_PLATFORM_OPTIONAL_BINDING_SOURCES}
)

# This first RP2350 2.8 port intentionally omits the legacy image API.
# image.c enforces its registry gate at compile time (no unavailable stub).
if(BOARD STREQUAL "waveshare_rp2350_touch_lcd_2.8")
    list(REMOVE_ITEM BINDINGS_SOURCES image.c jpeg.c ${MCUJS_PICOJPEG_SOURCE})
else()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/scripts/prepare-picojpeg.py"
        "$ENV{PICOJPEG_PATH}/picojpeg.c" "$ENV{PICOJPEG_PATH}/picojpeg.h")
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/scripts/prepare-picojpeg.py"
        "$ENV{PICOJPEG_PATH}/picojpeg.c" "${MCUJS_PICOJPEG_SOURCE}"
        RESULT_VARIABLE MCUJS_PICOJPEG_RESULT)
    if(NOT MCUJS_PICOJPEG_RESULT EQUAL 0)
        message(FATAL_ERROR "Pinned picojpeg preparation failed")
    endif()
endif()

# JavaScript API bindings library
add_library(mcujs_bindings STATIC ${BINDINGS_SOURCES})
set(MCUJS_PACKAGED_MODULES events devices button buzzer speaker)
if(BOARD STREQUAL "waveshare_rp2350_touch_lcd_2.8")
    target_sources(mcujs_bindings PRIVATE speaker_wav.c
        ${CMAKE_SOURCE_DIR}/platform/rp2/bindings/speaker.c)
    pico_generate_pio_header(mcujs_bindings
        ${CMAKE_SOURCE_DIR}/platform/rp2/bindings/speaker_i2s.pio)
endif()
if(MCUJS_EXPERIMENTAL_CANVAS)
    list(APPEND MCUJS_PACKAGED_MODULES canvas displays/st7789)
endif()
foreach(module ${MCUJS_PACKAGED_MODULES})
    get_filename_component(name "${module}" NAME)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/lib/${module}.js")
    file(READ "${CMAKE_SOURCE_DIR}/lib/${module}.js" source_hex HEX)
    string(REGEX REPLACE "(..)" "0x\\1," source_bytes "${source_hex}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${name}_source.h"
        "static const jerry_char_t ${name}_source[] = {${source_bytes}};\n")
endforeach()
if(MCUJS_EXPERIMENTAL_CANVAS)
    target_sources(mcujs_bindings PRIVATE canvas_native.c canvas_renderer.c
                   ${MCUJS_PLATFORM_CANVAS_BINDING_SOURCES})
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
