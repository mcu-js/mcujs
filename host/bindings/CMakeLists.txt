cmake_minimum_required(VERSION 3.13)

# Base source files for JavaScript API bindings
set(BINDINGS_SOURCES
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
    graphics.c
    image.c
    keyboard.c
    mouse.c
    screen.c
    $ENV{PICOJPEG_PATH}/picojpeg.c
)

# Add DVI support if board has it
if(MCUJS_HAS_DVI)
    list(APPEND BINDINGS_SOURCES dvi.c)
    message(STATUS "DVI support enabled")
endif()

# JavaScript API bindings library
add_library(mcujs_bindings STATIC ${BINDINGS_SOURCES})

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
)

# Add PicoDVI include directories if DVI support enabled
if(MCUJS_HAS_DVI)
    target_include_directories(mcujs_bindings PUBLIC
        $ENV{PICODVI_PATH}/software/libdvi
        $ENV{PICODVI_PATH}/software/include
    )
endif()

pico_generate_pio_header(mcujs_bindings ${CMAKE_CURRENT_LIST_DIR}/neopixel.pio)

target_link_libraries(mcujs_bindings PUBLIC
    mcujs_jerry
    pico_stdlib
    pico_unique_id
    pico_multicore
    hardware_gpio
    hardware_pwm
    hardware_i2c
    hardware_spi
    hardware_adc
    hardware_timer
    hardware_pio
    hardware_dma
    hardware_clocks
    tinyusb_device
)

# Add CYW43 driver for WiFi boards (Pico W, Pico 2 W)
if(MCUJS_HAS_CYW43)
    target_link_libraries(mcujs_bindings PUBLIC
        pico_cyw43_arch_none  # Minimal CYW43 support (GPIO only, no network stack)
    )
endif()

# Add PicoDVI library for DVI output support
if(MCUJS_HAS_DVI)
    # PicoDVI libdvi sources (including assembly for TMDS encode)
    add_library(libdvi STATIC
        $ENV{PICODVI_PATH}/software/libdvi/dvi.c
        $ENV{PICODVI_PATH}/software/libdvi/dvi_serialiser.c
        $ENV{PICODVI_PATH}/software/libdvi/dvi_timing.c
        $ENV{PICODVI_PATH}/software/libdvi/tmds_encode.c
        $ENV{PICODVI_PATH}/software/libdvi/tmds_encode.S
    )
    
    target_include_directories(libdvi PUBLIC
        $ENV{PICODVI_PATH}/software/libdvi
        $ENV{PICODVI_PATH}/software/include
    )
    
    # Generate PIO headers for DVI serializer
    pico_generate_pio_header(libdvi $ENV{PICODVI_PATH}/software/libdvi/dvi_serialiser.pio)
    
    target_link_libraries(libdvi PUBLIC
        pico_stdlib
        pico_multicore
        hardware_pio
        hardware_dma
        hardware_irq
        hardware_vreg
        hardware_clocks
        hardware_interp
        hardware_pwm
    )
    
    # Add compile definitions for PicoDVI
    target_compile_definitions(libdvi PUBLIC
        DVI_DEFAULT_SERIAL_CONFIG=waveshare_rp2040_pizero
        DVI_VERTICAL_REPEAT=2
        DVI_N_TMDS_BUFFERS=3
    )
    
    target_link_libraries(mcujs_bindings PUBLIC libdvi)
endif()
