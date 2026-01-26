# mcujs - Waveshare RP2350-Touch-LCD-1.69 Board CMake Configuration

# Set board-specific variables
set(MCUJS_BOARD_NAME "waveshare_rp2350_touch_lcd_1.69")
set(MCUJS_CHIP "RP2350")
set(MCUJS_FLASH_SIZE 16777216)  # 16MB

# Set Pico SDK board
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Set platform (RP2350 with ARM cores)
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Platform")

# Compiler flags for RP2350
add_compile_definitions(
    MCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_1_69=1
    PICO_FLASH_SIZE_BYTES=${MCUJS_FLASH_SIZE}
)

message(STATUS "Configuring for Waveshare RP2350-Touch-LCD-1.69 (RP2350)")
