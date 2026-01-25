# mcujs core library
# Contains USB, filesystem, REPL, and boot loader

add_library(mcujs_core STATIC
    # USB subsystem (using TinyUSB directly)
    usb/usb_cdc.c
    usb/usb_descriptors.c
    usb/usb_msc.c
    
    # Filesystem (FatFs + flash driver)
    filesystem/fs.c
    filesystem/diskio.c
    filesystem/flash_ops.c
    
    # FatFs library (ChaN's FAT filesystem) - from Docker environment
    $ENV{FATFS_PATH}/source/ff.c
    $ENV{FATFS_PATH}/source/ffunicode.c
    
    # Core functionality
    board.c
    repl.c
    boot.c
)

target_include_directories(mcujs_core PUBLIC
    # Our ffconf.h MUST come before FatFs source to override default config
    ${CMAKE_CURRENT_SOURCE_DIR}/filesystem
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/usb
    ${CMAKE_SOURCE_DIR}/host
    ${CMAKE_SOURCE_DIR}/host/bindings
    ${CMAKE_SOURCE_DIR}/javascript
    ${CMAKE_SOURCE_DIR}/javascript/jerryscript-core
    ${CMAKE_SOURCE_DIR}/board
    ${CMAKE_SOURCE_DIR}/board/${BOARD}
    # FatFs library headers - from Docker environment (AFTER our filesystem dir)
    $ENV{FATFS_PATH}/source
)

target_link_libraries(mcujs_core PUBLIC
    mcujs_host
    pico_stdlib
    pico_multicore
    pico_unique_id
    hardware_flash
    hardware_sync
    tinyusb_device
    tinyusb_board
)

# Add CYW43 driver for WiFi boards (Pico W, Pico 2 W)
if(MCUJS_HAS_CYW43)
    target_link_libraries(mcujs_core PUBLIC
        pico_cyw43_arch_none  # Minimal CYW43 support (GPIO only, no network stack)
    )
endif()
