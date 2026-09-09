# Initial Waveshare RP2350-Touch-LCD-2.8 port (16MB flash).
set(MCUJS_BOARD_NAME "waveshare_rp2350_touch_lcd_2.8")
set(MCUJS_CHIP "RP2350")
set(MCUJS_FLASH_SIZE 16777216)
set(PICO_BOARD pico2 CACHE STRING "Board type")
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Platform")
add_compile_definitions(
    MCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_2_8=1
    PICO_FLASH_SIZE_BYTES=${MCUJS_FLASH_SIZE}
)
message(STATUS "Configuring for Waveshare RP2350-Touch-LCD-2.8 (RP2350)")
