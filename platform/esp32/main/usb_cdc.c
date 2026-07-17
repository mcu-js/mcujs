/*
 * MCU.js ESP32-S3 fixed USB Serial/JTAG console adapter.
 *
 * This uses ESP-IDF's interrupt-driven fixed USB Serial/JTAG driver. It does
 * not initialize USB-OTG or TinyUSB, preserving ROM download ownership.
 */

#include "usb_cdc.h"

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_check.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static int s_pending_char = -1;

void usb_cdc_init(void) {
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));

    /* Keep ESP-IDF logs on the same buffered driver as MCU.js output. MCU.js
     * reads through usb_serial_jtag_read_bytes() with a zero-tick timeout. */
    usb_serial_jtag_vfs_use_driver();
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
}

void usb_cdc_task(void) {
}

bool usb_cdc_connected(void) {
    return usb_serial_jtag_is_connected();
}

bool usb_cdc_available(void) {
    if (s_pending_char >= 0) {
        return 1;
    }

    unsigned char c;
    if (usb_serial_jtag_read_bytes(&c, 1, 0) == 1) {
        s_pending_char = c;
        return 1;
    }
    return 0;
}

int usb_cdc_getchar(void) {
    if (s_pending_char >= 0) {
        int c = s_pending_char;
        s_pending_char = -1;
        return c;
    }

    unsigned char c;
    return usb_serial_jtag_read_bytes(&c, 1, 0) == 1 ? c : -1;
}

size_t usb_cdc_read(char *buffer, size_t length) {
    if (buffer == NULL || length == 0) {
        return 0;
    }

    size_t used = 0;
    if (s_pending_char >= 0) {
        buffer[used++] = (char)s_pending_char;
        s_pending_char = -1;
    }

    if (used < length) {
        int count = usb_serial_jtag_read_bytes(buffer + used, length - used, 0);
        if (count > 0) {
            used += (size_t)count;
        }
    }
    return used;
}

size_t usb_cdc_write(const char *buffer, size_t length) {
    if (buffer == NULL || length == 0) {
        return 0;
    }

    size_t written = 0;
    unsigned int retries = 0;
    while (written < length) {
        int count = usb_serial_jtag_write_bytes(buffer + written, length - written, 1);
        if (count > 0) {
            written += (size_t)count;
            retries = 0;
            continue;
        }
        if (retries++ >= 100) {
            break;
        }
        vTaskDelay(1);
    }
    return written;
}

void usb_cdc_putchar(char c) {
    (void)usb_cdc_write(&c, 1);
}

void usb_cdc_puts(const char *str) {
    if (str != NULL) {
        (void)usb_cdc_write(str, strlen(str));
    }
}

void usb_cdc_flush(void) {
    fflush(stdout);
}

void usb_cdc_reset_usb(uint32_t delay_ms) {
    usb_cdc_flush();
    TickType_t ticks = pdMS_TO_TICKS(delay_ms);
    if (delay_ms > 0 && ticks == 0) {
        ticks = 1;
    }
    vTaskDelay(ticks);
    esp_restart();
}
