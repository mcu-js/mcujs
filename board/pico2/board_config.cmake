# mcujs - Raspberry Pi Pico 2 Board CMake Configuration

# Set board-specific variables
set(MCUJS_BOARD_NAME "pico2")
set(MCUJS_CHIP "RP2350")
set(MCUJS_FLASH_SIZE 4194304)  # 4MB

# Set Pico SDK board
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Set platform (RP2350 with ARM cores)
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Platform")

# Compiler flags for RP2350
add_compile_definitions(
    MCUJS_BOARD_PICO2=1
    PICO_FLASH_SIZE_BYTES=${MCUJS_FLASH_SIZE}
)

message(STATUS "Configuring for Raspberry Pi Pico 2 (RP2350)")
