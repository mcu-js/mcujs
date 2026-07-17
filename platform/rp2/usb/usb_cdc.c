/*
 * mcujs - USB CDC Implementation
 * 
 * Serial communication over USB using TinyUSB directly
 */

#include "usb_cdc.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include <string.h>

void usb_cdc_init(void) {
    /* TinyUSB CDC is initialized via tusb_init() in main */
}

void usb_cdc_task(void) {
    /* Process TinyUSB device tasks */
    tud_task();
}

size_t usb_cdc_write(const char *data, size_t len) {
    if (data == NULL || len == 0) {
        return 0;
    }
    
    if (!tud_cdc_connected()) {
        return 0;
    }
    
    size_t written = 0;
    while (written < len) {
        size_t avail = tud_cdc_write_available();
        if (avail == 0) {
            tud_task();
            tud_cdc_write_flush();
            continue;
        }
        
        size_t to_write = len - written;
        if (to_write > avail) {
            to_write = avail;
        }
        
        size_t w = tud_cdc_write(data + written, to_write);
        written += w;
        
        if (w == 0) {
            break;  /* Avoid infinite loop */
        }
    }
    
    tud_cdc_write_flush();
    return written;
}

void usb_cdc_puts(const char *str) {
    if (str != NULL) {
        usb_cdc_write(str, strlen(str));
    }
}

void usb_cdc_putchar(char c) {
    if (tud_cdc_connected()) {
        tud_cdc_write_char(c);
        tud_cdc_write_flush();
    }
}

size_t usb_cdc_read(char *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return 0;
    }
    
    if (!tud_cdc_connected() || !tud_cdc_available()) {
        return 0;
    }
    
    return tud_cdc_read(buffer, len);
}

int usb_cdc_getchar(void) {
    if (!tud_cdc_connected() || !tud_cdc_available()) {
        return -1;
    }
    
    uint8_t c;
    if (tud_cdc_read(&c, 1) == 1) {
        return c;
    }
    return -1;
}

bool usb_cdc_available(void) {
    return tud_cdc_connected() && tud_cdc_available() > 0;
}

bool usb_cdc_connected(void) {
    return tud_cdc_connected();
}

void usb_cdc_flush(void) {
    tud_cdc_write_flush();
}

void usb_cdc_reset_usb(uint32_t delay_ms) {
    tud_disconnect();
    sleep_ms(delay_ms);
    watchdog_enable(1, false);
    while (1) {
        tight_loop_contents();
    }
}

/*--------------------------------------------------------------------
 * TinyUSB CDC Callbacks
 *--------------------------------------------------------------------*/

/* Invoked when CDC interface received data from host */
void tud_cdc_rx_cb(uint8_t itf) {
    (void)itf;
    /* Data available - will be read by usb_cdc_read/getchar */
}

/* Invoked when TX buffer is empty (transmit complete) */
void tud_cdc_tx_complete_cb(uint8_t itf) {
    (void)itf;
}

/* Invoked when line state changes (DTR/RTS) */
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void)itf;
    (void)rts;
    
    if (dtr) {
        /* Terminal connected */
    } else {
        /* Terminal disconnected */
    }
}

/* Invoked when line coding changes (baud rate, etc.) */
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p_line_coding) {
    (void)itf;
    (void)p_line_coding;
    /* Could handle baud rate changes here if needed */
}
