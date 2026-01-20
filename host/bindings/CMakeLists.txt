cmake_minimum_required(VERSION 3.13)

# JavaScript API bindings library
add_library(mcujs_bindings STATIC
    bindings.c
    console.c
    gpio.c
    timers.c
    pwm.c
    i2c.c
    spi.c
    adc.c
    neopixel.c
    board.c
    fs.c
    process.c
    require.c
)

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
    ${CMAKE_CURRENT_BINARY_DIR}
)

pico_generate_pio_header(mcujs_bindings ${CMAKE_CURRENT_LIST_DIR}/neopixel.pio)

target_link_libraries(mcujs_bindings PUBLIC
    mcujs_jerry
    pico_stdlib
    pico_unique_id
    hardware_gpio
    hardware_pwm
    hardware_i2c
    hardware_spi
    hardware_adc
    hardware_timer
    hardware_pio
)
