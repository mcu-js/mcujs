/*
 * mcujs - USB CDC Interface
 * 
 * Serial communication over USB
 */

#ifndef MCUJS_USB_CDC_H
#define MCUJS_USB_CDC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Initialize USB CDC
 */
void usb_cdc_init(void);

/*
 * Process USB tasks - call regularly from main loop
 */
void usb_cdc_task(void);

/*
 * Write data to CDC
 * Non-blocking, returns number of bytes written
 */
size_t usb_cdc_write(const char *data, size_t len);

/*
 * Write a string to CDC (convenience function)
 */
void usb_cdc_puts(const char *str);

/*
 * Write a single character to CDC
 */
void usb_cdc_putchar(char c);

/*
 * Read data from CDC
 * Non-blocking, returns number of bytes read
 */
size_t usb_cdc_read(char *buffer, size_t len);

/*
 * Read a single character from CDC
 * Returns -1 if no data available
 */
int usb_cdc_getchar(void);

/*
 * Check if data is available for reading
 */
bool usb_cdc_available(void);

/*
 * Check if CDC is connected (DTR set)
 */
bool usb_cdc_connected(void);

/*
 * Flush TX buffer
 */
void usb_cdc_flush(void);

#endif /* MCUJS_USB_CDC_H */
