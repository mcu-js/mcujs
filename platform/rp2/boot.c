/**
 * mcujs - JavaScript Runtime for Raspberry Pi Pico
 * Boot Loader Implementation
 */

#include "boot.h"
#include "fs.h"
#include "engine.h"
#include "usb_cdc.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Check if BOOTSEL button is pressed
 * 
 * The BOOTSEL button is connected to the QSPI CS pin, which is directly
 * tied to the flash chip. Reading it requires temporarily disabling flash
 * access and reading the GPIO state.
 * 
 * WARNING: This function must be called with interrupts disabled and
 * must not execute from flash (it's marked __no_inline_not_in_flash_func).
 */
static bool __no_inline_not_in_flash_func(get_bootsel_button)(void) {
    const uint CS_PIN_INDEX = 1;  /* QSPI CS is GPIO 1 in QSPI bank */
    
    /* Save current state of QSPI CS pin */
    uint32_t saved = ioqspi_hw->io[CS_PIN_INDEX].ctrl;
    
    /* Set CS pin to input with pull-up disabled */
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    
    /* Wait a bit for the pin to settle */
    for (volatile int i = 0; i < 1000; i++);
    
    /* Read the pin state - button pressed = LOW */
    bool button_pressed = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));
    
    /* Restore the original state */
    ioqspi_hw->io[CS_PIN_INDEX].ctrl = saved;
    
    return button_pressed;
}

/**
 * Check if safe mode should be enabled (BOOTSEL held during boot)
 */
static bool boot_check_safe_mode(void) {
    uint32_t ints = save_and_disable_interrupts();
    bool pressed = get_bootsel_button();
    restore_interrupts(ints);
    return pressed;
}

// Maximum file size for boot script (32KB)
#define BOOT_MAX_FILE_SIZE (32 * 1024)

/**
 * Print a boot message (if CDC is connected)
 */
static void boot_print(const char* msg) {
    if (usb_cdc_connected()) {
        usb_cdc_puts(msg);
    }
}

/**
 * Print a boot error message
 */
static void boot_error(const char* msg) {
    boot_print("Boot error: ");
    boot_print(msg);
    boot_print("\r\n");
}

/**
 * Check if boot file exists
 */
bool boot_file_exists(void) {
    return fs_exists(BOOT_INDEX_FILE) == FS_OK;
}

/**
 * Run a JavaScript file from the filesystem
 */
bool boot_run_file(const char* filename) {
    // Use engine's exec_file function which handles everything
    js_result_t result = js_engine_exec_file(filename);
    
    if (result != JS_OK) {
        char error_buf[512];  /* Larger buffer for stack traces */
        size_t len = js_engine_get_error(error_buf, sizeof(error_buf));
        if (len > 0) {
            boot_error(error_buf);
        } else {
            boot_error("Unknown error");
        }
        return false;
    }
    
    return true;
}

/**
 * Attempt to boot from index.js
 */
bool boot_run_index_js(void) {
    /* Check for safe mode (BOOTSEL held during boot) */
    if (boot_check_safe_mode()) {
        boot_print("*** SAFE MODE ***\r\n");
        boot_print("BOOTSEL held - skipping index.js\r\n");
        boot_print("Release BOOTSEL and reset to run normally.\r\n");
        return false;
    }
    
    /* Check if index.js exists */
    if (!boot_file_exists()) {
        boot_print("No index.js found. Starting REPL...\r\n");
        return false;
    }
    
    boot_print("Running ");
    boot_print(BOOT_INDEX_FILE);
    boot_print("...\r\n");
    
    /* Run the boot file */
    bool success = boot_run_file(BOOT_INDEX_FILE);
    
    if (success) {
        boot_print("Execution complete.\r\n");
    }
    
    return success;
}
