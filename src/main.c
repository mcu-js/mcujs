/**
 * mcujs - JavaScript Runtime for Raspberry Pi Pico
 * Main entry point
 */

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"

#include "board.h"
#include "board_config.h"
#include "engine.h"
#include "bindings.h"
#include "fs.h"
#include "usb_cdc.h"
#include "repl.h"
#include "boot.h"

/* Version info (from CMake) */
#ifndef MCUJS_VERSION
#define MCUJS_VERSION "0.1.0"
#endif

#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

/* Forward declarations */
static void print_banner(void);
static void print_banner_once(void);
static void main_loop(void);

int main(void) {
    /* Initialize basic Pico SDK (clocks, etc.) but NOT stdio */
    stdio_init_all();
    
    /* Setup LED for status indication */
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    
    /* Blink LED 3 times to show we reached main() */
    for (int i = 0; i < 3; i++) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(200);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(200);
    }
    
    /* Initialize TinyUSB */
    tusb_init();
    
    /* Run USB task for a bit to allow enumeration */
    for (int i = 0; i < 100; i++) {
        tud_task();
        sleep_ms(10);
    }
    
    /* Initialize filesystem */
    fs_result_t fs_result = fs_init();
    if (fs_result != FS_OK) {
        /* Filesystem init failed - indicate with rapid blinks */
        while (1) {
            for (int i = 0; i < 10; i++) {
                gpio_put(PICO_DEFAULT_LED_PIN, 1);
                sleep_ms(50);
                gpio_put(PICO_DEFAULT_LED_PIN, 0);
                sleep_ms(50);
                tud_task();
            }
            sleep_ms(500);
        }
    }
    
    /* Initialize JavaScript engine */
    if (js_engine_init() != JS_OK) {
        /* Fatal error - 5 rapid blinks then pause */
        while (1) {
            for (int i = 0; i < 5; i++) {
                gpio_put(PICO_DEFAULT_LED_PIN, 1);
                sleep_ms(100);
                gpio_put(PICO_DEFAULT_LED_PIN, 0);
                sleep_ms(100);
                tud_task();
            }
            sleep_ms(1000);
        }
    }
    
    /* Initialize REPL */
    repl_init();
    
    /* Attempt to boot from index.js */
    boot_run_index_js();

    /* Print startup banner on first CDC connect */
    print_banner_once();
    
    /* Enter main loop */
    main_loop();
    
    /* Should never reach here */
    js_engine_cleanup();
    return 0;
}

/**
 * Print startup banner to serial
 */
static void print_banner(void) {
    const board_info_t *info = board_get_info();

    usb_cdc_puts("\r\n");
    usb_cdc_puts("  __  __  ___ _   _  _ ___ \r\n");
    usb_cdc_puts(" |  \\/  |/ __| | | || / __|\r\n");
    usb_cdc_puts(" | |\\/| | (__| |_| || \\__ \\\r\n");
    usb_cdc_puts(" |_|  |_|\\___|\\___// |___/\r\n");
    usb_cdc_puts("                 |__/     \r\n");
    usb_cdc_puts("\r\n");
    usb_cdc_puts("mcujs v");
    usb_cdc_puts(MCUJS_VERSION);
    usb_cdc_puts(" - JavaScript for ");
    usb_cdc_puts(info->name);
    usb_cdc_puts("\r\n");
    usb_cdc_puts("Type .help for available commands\r\n");
    usb_cdc_puts("\r\n> ");
}

static void print_banner_once(void) {
    static bool printed = false;

    if (!printed && usb_cdc_connected()) {
        sleep_ms(100);
        tud_task();
        print_banner();
        printed = true;
    }
}

/**
 * Main application loop
 */
static void main_loop(void) {
    while (1) {
        /* Process USB tasks */
        tud_task();
        
        /* Process REPL input/output */
        repl_task();

        /* Print banner once CDC connects */
        print_banner_once();
        
        /* Process JS timers */
        js_engine_process_timers();
        
        /* Yield to allow other processing */
        tight_loop_contents();
    }
}

/*--------------------------------------------------------------------
 * TinyUSB Device Callbacks
 *--------------------------------------------------------------------*/

/* Invoked when device is mounted */
void tud_mount_cb(void) {
    /* Device connected to host */
}

/* Invoked when device is unmounted */
void tud_umount_cb(void) {
    /* Device disconnected from host */
}

/* Invoked when USB bus is suspended */
void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
}

/* Invoked when USB bus is resumed */
void tud_resume_cb(void) {
}
