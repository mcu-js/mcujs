# Raspberry Pi RP2040/RP2350 platform definition for mcujs.
#
# This module is the only build-system layer that imports and configures the
# Pico SDK. Shared runtime CMake files consume the source lists and target
# configuration functions declared here.

include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)

set(MCUJS_RP2_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR})

set(MCUJS_PLATFORM_MAIN_SOURCE
    ${CMAKE_CURRENT_LIST_DIR}/main.c
)

set(MCUJS_PLATFORM_JERRY_PORT_SOURCE
    ${CMAKE_CURRENT_LIST_DIR}/jerry_port.c
)

set(MCUJS_PLATFORM_USB_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/usb/usb_cdc.c
    ${CMAKE_CURRENT_LIST_DIR}/usb/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/usb/usb_msc.c
    ${CMAKE_CURRENT_LIST_DIR}/usb/usb_hid.c
)

set(MCUJS_PLATFORM_STORAGE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/filesystem/diskio.c
    ${CMAKE_CURRENT_LIST_DIR}/filesystem/flash_ops.c
)

set(MCUJS_PLATFORM_BOARD_SOURCE
    ${CMAKE_CURRENT_LIST_DIR}/board.c
)

set(MCUJS_PLATFORM_BOOT_SOURCE
    ${CMAKE_CURRENT_LIST_DIR}/boot.c
)

set(MCUJS_PLATFORM_HARDWARE_BINDING_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/bindings/pin_policy.c
    ${CMAKE_CURRENT_LIST_DIR}/bindings/gpio.c
    ${CMAKE_CURRENT_LIST_DIR}/bindings/timers.c
    ${CMAKE_CURRENT_LIST_DIR}/bindings/pwm.c
    ${CMAKE_CURRENT_LIST_DIR}/bindings/i2c.c
    ${CMAKE_CURRENT_LIST_DIR}/bindings/spi.c
    ${CMAKE_CURRENT_LIST_DIR}/bindings/adc.c
    ${CMAKE_CURRENT_LIST_DIR}/bindings/neopixel.c
    ${CMAKE_CURRENT_LIST_DIR}/bindings/onboard_led.c
    ${CMAKE_CURRENT_LIST_DIR}/bindings/board.c
)

set(MCUJS_PLATFORM_USB_BINDING_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/bindings/keyboard.c
    ${CMAKE_CURRENT_LIST_DIR}/bindings/mouse.c
)

set(MCUJS_PLATFORM_OPTIONAL_BINDING_SOURCES)
if(MCUJS_HAS_DVI)
    list(APPEND MCUJS_PLATFORM_OPTIONAL_BINDING_SOURCES
         ${CMAKE_CURRENT_LIST_DIR}/bindings/dvi.c)
endif()

set(MCUJS_PLATFORM_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/bindings
    ${CMAKE_CURRENT_LIST_DIR}/filesystem
    ${CMAKE_CURRENT_LIST_DIR}/usb
)

macro(mcujs_platform_sdk_init)
    pico_sdk_init()
    add_compile_definitions(
        MCUJS_PLATFORM_NAME="rp2"
        MCUJS_PLATFORM_RP2=1
        MCUJS_PLATFORM_SDK_NAME="pico-sdk"
        MCUJS_PLATFORM_SDK_VERSION="2.2.0"
        PICO_SDK_VERSION="2.2.0"
        TINYUSB_VERSION="0.18.0"
    )
endmacro()

function(mcujs_platform_configure_host target)
    target_link_libraries(${target} PUBLIC
        pico_stdlib
    )
endfunction()

function(mcujs_platform_configure_jerry target visibility)
    target_link_libraries(${target} ${visibility}
        pico_stdlib
        pico_time
    )
endfunction()

function(mcujs_platform_configure_core target)
    target_link_libraries(${target} PUBLIC
        pico_stdlib
        pico_multicore
        pico_unique_id
        hardware_flash
        hardware_sync
        tinyusb_device
        tinyusb_board
    )

    if(MCUJS_HAS_CYW43)
        target_link_libraries(${target} PUBLIC
            pico_cyw43_arch_none
        )
    endif()
endfunction()

function(mcujs_platform_configure_bindings target)
    pico_generate_pio_header(${target}
        ${MCUJS_RP2_PLATFORM_DIR}/bindings/neopixel.pio
    )

    target_link_libraries(${target} PUBLIC
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

    if(MCUJS_HAS_CYW43)
        target_link_libraries(${target} PUBLIC
            pico_cyw43_arch_none
        )
    endif()

    if(MCUJS_HAS_DVI)
        target_include_directories(${target} PUBLIC
            $ENV{PICODVI_PATH}/software/libdvi
            $ENV{PICODVI_PATH}/software/include
        )

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

        pico_generate_pio_header(libdvi
            $ENV{PICODVI_PATH}/software/libdvi/dvi_serialiser.pio
        )

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

        target_compile_definitions(libdvi PUBLIC
            DVI_DEFAULT_SERIAL_CONFIG=waveshare_rp2040_pizero
            DVI_VERTICAL_REPEAT=2
            DVI_N_TMDS_BUFFERS=3
        )

        target_link_libraries(${target} PUBLIC libdvi)
    endif()
endfunction()

function(mcujs_platform_configure_executable target)
    target_link_libraries(${target} PRIVATE
        pico_stdlib
        hardware_flash
        hardware_sync
        hardware_gpio
        hardware_pwm
        hardware_i2c
        hardware_spi
        tinyusb_device
        tinyusb_board
    )

    if(MCUJS_HAS_CYW43)
        target_link_libraries(${target} PRIVATE
            pico_cyw43_arch_none
        )
    endif()

    pico_enable_stdio_usb(${target} 0)
    pico_enable_stdio_uart(${target} 0)
    pico_add_extra_outputs(${target})

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/mcujs-${MCUJS_VERSION}-${BOARD}.uf2
            ${CMAKE_SOURCE_DIR}/build/mcujs-${MCUJS_VERSION}-${BOARD}.uf2
        COMMENT "Copying UF2 to build directory"
    )
endfunction()
