# mcujs - Raspberry Pi Pico Board CMake Configuration

# Set board-specific variables
set(MCUJS_BOARD_NAME "pico")
set(MCUJS_CHIP "RP2040")
set(MCUJS_FLASH_SIZE 2097152)  # 2MB

# Set Pico SDK board
set(PICO_BOARD pico CACHE STRING "Board type")

# Set platform (RP2040)
set(PICO_PLATFORM rp2040 CACHE STRING "Platform")

# Compiler flags for RP2040
add_compile_definitions(
    MCUJS_BOARD_PICO=1
    PICO_FLASH_SIZE_BYTES=${MCUJS_FLASH_SIZE}
)

message(STATUS "Configuring for Raspberry Pi Pico (RP2040)")
