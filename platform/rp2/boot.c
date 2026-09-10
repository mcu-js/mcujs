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
#include "hardware/regs/sio.h"

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
 * Sampling follows raspberrypi/pico-examples picoboard/button/button.c:
 * run from SRAM, mask interrupts, float CS, and allow 1000 settling iterations.
 * Restore the entire original control register before unmasking interrupts.
 * Runtime use is restricted to Pico RP2040: core 1 is idle (only DVI launches
 * it, on a different board), and SPI DMA completes synchronously. Do not call
 * while another core, DMA, or an XIP streamer can access flash.
 */
bool __no_inline_not_in_flash_func(boot_button_pressed)(void) {
    const uint CS_PIN_INDEX = 1;  /* QSPI CS is GPIO 1 in QSPI bank */
    uint32_t ints = save_and_disable_interrupts();
    
    /* Save current state of QSPI CS pin */
    uint32_t saved = ioqspi_hw->io[CS_PIN_INDEX].ctrl;
    
    /* Float flash CS (Hi-Z); BOOTSEL pulls it low when pressed. */
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    
    /* Wait a bit for the pin to settle */
    for (volatile int i = 0; i < 1000; i++);
    
    /* The QSPI bank index is 1 on both chips, but the SIO input bit is not:
     * RP2350 places QSPI CSN at bit 27 (not RP2040's bit 1). */
#if PICO_RP2350
    const uint32_t cs_input_mask = SIO_GPIO_HI_IN_QSPI_CSN_BITS;
#else
    const uint32_t cs_input_mask = 1u << CS_PIN_INDEX;
#endif
    bool button_pressed = !(sio_hw->gpio_hi_in & cs_input_mask);
    
    /* Restore the original state */
    ioqspi_hw->io[CS_PIN_INDEX].ctrl = saved;
    restore_interrupts(ints);
    
    return button_pressed;
}

/**
 * Check if safe mode should be enabled (BOOTSEL held during boot)
 */
static bool boot_check_safe_mode(void) {
    return boot_button_pressed();
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
        boot_print("BOOTSEL held - skipping /app/index.js\r\n");
        boot_print("Release BOOTSEL and reset to run normally.\r\n");
        return false;
    }
    
    /* Check if index.js exists */
    if (!boot_file_exists()) {
        boot_print("No /app/index.js found. Starting REPL...\r\n");
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
