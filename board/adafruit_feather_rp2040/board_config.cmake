# mcujs - Adafruit Feather RP2040 Board CMake Configuration

# Set board-specific variables
set(MCUJS_BOARD_NAME "adafruit_feather_rp2040")
set(MCUJS_CHIP "RP2040")
set(MCUJS_FLASH_SIZE 8388608)  # 8MB

# Set Pico SDK board (use Adafruit's official board definition)
set(PICO_BOARD adafruit_feather_rp2040 CACHE STRING "Board type")

# Set platform (RP2040)
set(PICO_PLATFORM rp2040 CACHE STRING "Platform")

# Compiler flags for RP2040
add_compile_definitions(
    MCUJS_BOARD_ADAFRUIT_FEATHER_RP2040=1
    PICO_FLASH_SIZE_BYTES=${MCUJS_FLASH_SIZE}
)

message(STATUS "Configuring for Adafruit Feather RP2040 (RP2040)")
