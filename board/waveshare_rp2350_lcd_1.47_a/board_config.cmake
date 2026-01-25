# mcujs - Waveshare RP2350-LCD-1.47-A Board CMake Configuration

# Set board-specific variables
set(MCUJS_BOARD_NAME "waveshare_rp2350_lcd_1.47_a")
set(MCUJS_CHIP "RP2350")
set(MCUJS_FLASH_SIZE 16777216)  # 16MB

# Set Pico SDK board
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Set platform (RP2350 with ARM cores)
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Platform")

# Compiler flags for RP2350
add_compile_definitions(
    MCUJS_BOARD_WAVESHARE_RP2350_LCD_1_47_A=1
    PICO_FLASH_SIZE_BYTES=${MCUJS_FLASH_SIZE}
)

message(STATUS "Configuring for Waveshare RP2350-LCD-1.47-A (RP2350)")
