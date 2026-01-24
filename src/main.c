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

#if MCUJS_HAS_NEOPIXEL
#include "neopixel.h"
#endif

#if MCUJS_HAS_CYW43
#include "pico/cyw43_arch.h"
#endif

/* Version info (from CMake) */
#ifndef MCUJS_VERSION
#define MCUJS_VERSION "0.1.0"
#endif

#ifndef MCUJS_LED_PIN
#define MCUJS_LED_PIN 255
#endif

/* Forward declarations */
static void print_banner(void);
static void print_banner_once(void);
static void main_loop(void);
static void boot_blink(int count, int on_ms, int off_ms);
static void boot_status_on(void);
static void boot_status_off(void);

int main(void) {
    /* Initialize basic Pico SDK (clocks, etc.) but NOT stdio */
    stdio_init_all();
    
#if MCUJS_HAS_CYW43
    /* Initialize CYW43 for Pico W / Pico 2 W boards */
    /* This must be done before any CYW43 GPIO access (including LED) */
    if (cyw43_arch_init()) {
        /* CYW43 init failed - can't indicate via LED since it's on CYW43 */
        /* Just continue without CYW43 support */
    }
#endif
    
    /* Setup LED for status indication */
#if MCUJS_HAS_CYW43
    /* LED is on CYW43, already initialized above */
#elif MCUJS_LED_PIN != 255
    gpio_init(MCUJS_LED_PIN);
    gpio_set_dir(MCUJS_LED_PIN, GPIO_OUT);
#endif

#if MCUJS_HAS_NEOPIXEL && MCUJS_LED_PIN == 255
    /* Initialize NeoPixel for status indication on boards without LED */
    neopixel_init(MCUJS_NEOPIXEL_PIN, MCUJS_NEOPIXEL_LENGTH);
#if MCUJS_NEOPIXEL_ORDER_GRB
    neopixel_set_order(true);
#else
    neopixel_set_order(false);
#endif
#endif

    /* Blink 3 times to show we reached main() */
    boot_blink(3, 200, 200);
    
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
            boot_blink(10, 50, 50);
            sleep_ms(500);
            tud_task();
        }
    }
    
    /* Initialize JavaScript engine */
    if (js_engine_init() != JS_OK) {
        /* Fatal error - 5 rapid blinks then pause */
        while (1) {
            boot_blink(5, 100, 100);
            sleep_ms(1000);
            tud_task();
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

/*--------------------------------------------------------------------
 * Boot Status Helpers (LED or NeoPixel)
 *--------------------------------------------------------------------*/

static void boot_status_on(void) {
#if MCUJS_HAS_CYW43
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
#elif MCUJS_LED_PIN != 255
    gpio_put(MCUJS_LED_PIN, 1);
#elif MCUJS_HAS_NEOPIXEL
    /* White at low brightness */
    neopixel_set_pixel(0, 32, 32, 32);
    neopixel_show();
#endif
}

static void boot_status_off(void) {
#if MCUJS_HAS_CYW43
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
#elif MCUJS_LED_PIN != 255
    gpio_put(MCUJS_LED_PIN, 0);
#elif MCUJS_HAS_NEOPIXEL
    neopixel_set_pixel(0, 0, 0, 0);
    neopixel_show();
#endif
}

static void boot_blink(int count, int on_ms, int off_ms) {
    for (int i = 0; i < count; i++) {
        boot_status_on();
        sleep_ms(on_ms);
        boot_status_off();
        sleep_ms(off_ms);
    }
}
