# mcujs core library
# Contains USB, filesystem, REPL, and boot loader

add_library(mcujs_core STATIC
    # Platform USB subsystem
    ${MCUJS_PLATFORM_USB_SOURCES}

    # Shared filesystem
    filesystem/fs.c
    ${MCUJS_PLATFORM_STORAGE_SOURCES}
    
    # FatFs library (ChaN's FAT filesystem) - from Docker environment
    $ENV{FATFS_PATH}/source/ff.c
    $ENV{FATFS_PATH}/source/ffunicode.c
    
    # Core functionality
    ${MCUJS_PLATFORM_BOARD_SOURCE}
    repl.c
    ${MCUJS_PLATFORM_BOOT_SOURCE}
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
    ${MCUJS_PLATFORM_INCLUDE_DIRS}
    # FatFs library headers - from Docker environment (AFTER our filesystem dir)
    $ENV{FATFS_PATH}/source
)

target_link_libraries(mcujs_core PUBLIC
    mcujs_host
)

mcujs_platform_configure_core(mcujs_core)
