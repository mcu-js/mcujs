# mcujs - Waveshare RP2040 Touch LCD 1.28 Board CMake Configuration

# Set board-specific variables
set(MCUJS_BOARD_NAME "waveshare_rp2040_touch_lcd_1.28")
set(MCUJS_CHIP "RP2040")
set(MCUJS_FLASH_SIZE 4194304)  # 4MB

# Set Pico SDK board
set(PICO_BOARD pico CACHE STRING "Board type")

# Set platform (RP2040)
set(PICO_PLATFORM rp2040 CACHE STRING "Platform")

# Compiler flags for RP2040
add_compile_definitions(
    MCUJS_BOARD_WAVESHARE_RP2040_TOUCH_LCD_1_28=1
    PICO_FLASH_SIZE_BYTES=${MCUJS_FLASH_SIZE}
)

message(STATUS "Configuring for Waveshare RP2040 Touch LCD 1.28 (RP2040)")
