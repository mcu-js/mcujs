# mcujs - Raspberry Pi Pico 2 W Board CMake Configuration

# Set board-specific variables
set(MCUJS_BOARD_NAME "pico2_w")
set(MCUJS_CHIP "RP2350")
set(MCUJS_FLASH_SIZE 4194304)  # 4MB
set(MCUJS_HAS_CYW43 ON)

# Set Pico SDK board
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Set platform (RP2350 with ARM cores)
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Platform")

# Compiler flags for RP2350 with CYW43
add_compile_definitions(
    MCUJS_BOARD_PICO2_W=1
    MCUJS_HAS_CYW43=1
    PICO_FLASH_SIZE_BYTES=${MCUJS_FLASH_SIZE}
    # CYW43 uses GPIO for LED control
    CYW43_WL_GPIO_LED_PIN=0
)

message(STATUS "Configuring for Raspberry Pi Pico 2 W (RP2350 + CYW43)")
