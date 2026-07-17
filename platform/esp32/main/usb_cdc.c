/*
 * MCU.js ESP32-S3 USB-OTG CDC adapter with UART0 recovery mirroring.
 *
 * TinyUSB owns the shared internal PHY in Milestone 4. UART0 on XIAO D6/D7
 * remains an independent 115200-baud input/output path for bring-up and
 * recovery when a 3.3 V USB-UART adapter is attached.
 */

#include "usb_cdc.h"
#include "board_config.h"
#include "usb_msc.h"

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb.h"
#include "tusb_cdc_acm.h"

#include <stdio.h>
#include <string.h>

#define MCUJS_UART UART_NUM_0
#define CDC_RX_STREAM_SIZE 512u
#define CDC_TX_BACKLOG_SIZE 2048u
#define CDC_TX_CHUNK_SIZE 64u

static StreamBufferHandle_t s_cdc_rx;
static StreamBufferHandle_t s_cdc_tx_backlog;
static bool s_initialized;
static bool s_uart_initialized;
static int s_pending_char = -1;
static uint8_t s_tx_pending[CDC_TX_CHUNK_SIZE];
static size_t s_tx_pending_length;
static size_t s_tx_pending_offset;


static void tinyusb_cdc_rx_callback(int interface, cdcacm_event_t *event) {
    (void)event;
    uint8_t buffer[64];
    size_t received = 0;

    do {
        received = 0;
        if (tinyusb_cdcacm_read(interface, buffer, sizeof(buffer), &received) != ESP_OK ||
            received == 0) {
            break;
        }
        (void)xStreamBufferSend(s_cdc_rx, buffer, received, 0);
    } while (received == sizeof(buffer));
}

static void uart_recovery_init(void) {
    const uart_config_t config = {
        .baud_rate = MCUJS_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(MCUJS_UART, &config));
    ESP_ERROR_CHECK(uart_set_pin(MCUJS_UART, MCUJS_UART_TX_PIN, MCUJS_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(MCUJS_UART, 512, 0, 0, NULL, 0));
    s_uart_initialized = true;
}

void usb_cdc_init(void) {
    s_cdc_rx = xStreamBufferCreate(CDC_RX_STREAM_SIZE, 1);
    s_cdc_tx_backlog = xStreamBufferCreate(CDC_TX_BACKLOG_SIZE, 1);
    ESP_ERROR_CHECK(s_cdc_rx == NULL || s_cdc_tx_backlog == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    uart_recovery_init();
    mcujs_usb_msc_init();

    const tinyusb_config_t tinyusb_config = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .string_descriptor_count = 0,
        .external_phy = false,
#if TUD_OPT_HIGH_SPEED
        .fs_configuration_descriptor = NULL,
        .hs_configuration_descriptor = NULL,
        .qualifier_descriptor = NULL,
#else
        .configuration_descriptor = NULL,
#endif
        .self_powered = false,
        .vbus_monitor_io = -1,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tinyusb_config));

    const tinyusb_config_cdcacm_t cdc_config = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = tinyusb_cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL,
    };
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&cdc_config));
    s_initialized = true;
}

void usb_cdc_task(void) {
    /* TinyUSB runs on the watched MCU.js main task so USB stalls participate
     * in the buttonless crash-loop recovery contract. */
    tud_task_ext(0, false);
    if (!s_initialized || !tud_cdc_connected()) {
        return;
    }

    bool queued = false;
    for (unsigned int attempt = 0; attempt < 8; attempt++) {
        if (s_tx_pending_offset == s_tx_pending_length) {
            s_tx_pending_length = xStreamBufferReceive(
                s_cdc_tx_backlog, s_tx_pending, sizeof(s_tx_pending), 0);
            s_tx_pending_offset = 0;
            if (s_tx_pending_length == 0) {
                break;
            }
        }

        size_t count = tinyusb_cdcacm_write_queue(
            TINYUSB_CDC_ACM_0,
            s_tx_pending + s_tx_pending_offset,
            s_tx_pending_length - s_tx_pending_offset);
        if (count == 0) {
            break;
        }
        s_tx_pending_offset += count;
        queued = true;
    }

    if (queued) {
        (void)tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
    }
}

bool usb_cdc_connected(void) {
    return s_initialized && tud_cdc_connected();
}

static bool refill_pending_char(void) {
    if (s_pending_char >= 0) {
        return true;
    }

    uint8_t c;
    if (s_cdc_rx != NULL && xStreamBufferReceive(s_cdc_rx, &c, 1, 0) == 1) {
        s_pending_char = c;
        return true;
    }
    if (s_uart_initialized && uart_read_bytes(MCUJS_UART, &c, 1, 0) == 1) {
        s_pending_char = c;
        return true;
    }
    return false;
}

bool usb_cdc_available(void) {
    return refill_pending_char();
}

int usb_cdc_getchar(void) {
    if (!refill_pending_char()) {
        return -1;
    }
    int result = s_pending_char;
    s_pending_char = -1;
    return result;
}

size_t usb_cdc_read(char *buffer, size_t length) {
    if (buffer == NULL || length == 0) {
        return 0;
    }

    size_t used = 0;
    if (refill_pending_char()) {
        buffer[used++] = (char)s_pending_char;
        s_pending_char = -1;
    }
    if (used < length && s_cdc_rx != NULL) {
        used += xStreamBufferReceive(s_cdc_rx, buffer + used, length - used, 0);
    }
    if (used < length && s_uart_initialized) {
        int count = uart_read_bytes(MCUJS_UART, buffer + used, length - used, 0);
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

    size_t uart_written = 0;
    if (s_uart_initialized) {
        int count = uart_write_bytes(MCUJS_UART, buffer, length);
        if (count > 0) {
            uart_written = (size_t)count;
        }
    }

    size_t usb_accepted = 0;
    if (s_initialized && tud_cdc_connected() &&
        s_tx_pending_offset == s_tx_pending_length &&
        xStreamBufferBytesAvailable(s_cdc_tx_backlog) == 0) {
        usb_accepted = tinyusb_cdcacm_write_queue(
            TINYUSB_CDC_ACM_0, (const uint8_t *)buffer, length);
        if (usb_accepted > 0) {
            (void)tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
        }
    }

    if (usb_accepted < length && s_cdc_tx_backlog != NULL) {
        usb_accepted += xStreamBufferSend(s_cdc_tx_backlog,
                                           buffer + usb_accepted,
                                           length - usb_accepted, 0);
    }

    return uart_written > usb_accepted ? uart_written : usb_accepted;
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
    usb_cdc_task();
    if (s_initialized && tud_cdc_connected()) {
        (void)tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(20));
    }
    if (s_uart_initialized) {
        (void)uart_wait_tx_done(MCUJS_UART, pdMS_TO_TICKS(20));
    }
}

void usb_cdc_reset_usb(uint32_t delay_ms) {
    usb_cdc_flush();
    if (s_initialized) {
        (void)tud_disconnect();
    }
    TickType_t ticks = pdMS_TO_TICKS(delay_ms);
    if (delay_ms > 0 && ticks == 0) {
        ticks = 1;
    }
    vTaskDelay(ticks);
    esp_restart();
}
