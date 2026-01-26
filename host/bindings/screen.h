/*
 * mcujs - Unified Screen API
 * 
 * Native display abstraction for DVI, LCD, OLED, E-Ink displays.
 * Provides a simple, web-developer-friendly API:
 * 
 *   var screen = require('screen');
 *   var dvi = require('drivers/dvi');
 *   screen.init(dvi);
 *   screen.fill(screen.BLACK);
 *   screen.fillRect(10, 10, 50, 50, screen.RED);
 *   screen.show();
 */

#ifndef MCUJS_SCREEN_H
#define MCUJS_SCREEN_H

#include <stdint.h>
#include <stdbool.h>
#include "jerryscript.h"

/* Maximum screen dimensions (memory safety limit for RP2040 with 264KB RAM) 
 * 320x240 RGB565 = 153KB, leaving ~100KB for JS heap and stack
 * For larger resolutions, use scanline rendering (future feature)
 */
#define SCREEN_MAX_WIDTH  320
#define SCREEN_MAX_HEIGHT 240

/* Byte order for color values */
typedef enum {
    SCREEN_BYTE_ORDER_NATIVE,    /* For DVI, most displays */
    SCREEN_BYTE_ORDER_SWAPPED    /* For SPI displays (big-endian) */
} screen_byte_order_t;

/* Screen state */
typedef struct {
    uint16_t *buffer;
    uint16_t width;
    uint16_t height;
    uint32_t byte_length;
    screen_byte_order_t byte_order;
    bool initialized;
    jerry_value_t driver;           /* JS driver object reference */
    jerry_value_t show_callback;    /* Cached driver.show function */
} screen_state_t;

/*
 * Color conversion - RGB888 to RGB565
 * Handles byte order based on current driver config
 */
uint16_t screen_rgb(uint8_t r, uint8_t g, uint8_t b);

/*
 * Color conversion from CSS hex string
 * Supports: "#RGB", "#RRGGBB"
 */
uint16_t screen_color_from_hex(const char *hex);

/*
 * Drawing primitives
 */
void screen_fill(uint16_t color);
void screen_set_pixel(int16_t x, int16_t y, uint16_t color);
void screen_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void screen_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void screen_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void screen_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void screen_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint8_t size);
void screen_draw_text(int16_t x, int16_t y, const char *text, uint16_t color, uint8_t size);

/*
 * Get raw buffer pointer (for drivers to access)
 */
uint16_t *screen_get_buffer(void);
uint32_t screen_get_buffer_byte_length(void);

/*
 * Get current screen dimensions
 */
uint16_t screen_get_width(void);
uint16_t screen_get_height(void);

/*
 * Check if screen is initialized
 */
bool screen_is_initialized(void);

#endif /* MCUJS_SCREEN_H */
