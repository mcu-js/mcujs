/*
 * mcujs - Unified Screen API
 * 
 * Native display abstraction for DVI, LCD, OLED, E-Ink displays.
 */

#include "screen.h"
#include "bindings.h"
#include "graphics.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 5x7 bitmap font for basic text rendering */
static const uint8_t font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, /* Space */
    0x00, 0x00, 0x5F, 0x00, 0x00, /* ! */
    0x00, 0x07, 0x00, 0x07, 0x00, /* " */
    0x14, 0x7F, 0x14, 0x7F, 0x14, /* # */
    0x24, 0x2A, 0x7F, 0x2A, 0x12, /* $ */
    0x23, 0x13, 0x08, 0x64, 0x62, /* % */
    0x36, 0x49, 0x55, 0x22, 0x50, /* & */
    0x00, 0x05, 0x03, 0x00, 0x00, /* ' */
    0x00, 0x1C, 0x22, 0x41, 0x00, /* ( */
    0x00, 0x41, 0x22, 0x1C, 0x00, /* ) */
    0x08, 0x2A, 0x1C, 0x2A, 0x08, /* * */
    0x08, 0x08, 0x3E, 0x08, 0x08, /* + */
    0x00, 0x50, 0x30, 0x00, 0x00, /* , */
    0x08, 0x08, 0x08, 0x08, 0x08, /* - */
    0x00, 0x60, 0x60, 0x00, 0x00, /* . */
    0x20, 0x10, 0x08, 0x04, 0x02, /* / */
    0x3E, 0x51, 0x49, 0x45, 0x3E, /* 0 */
    0x00, 0x42, 0x7F, 0x40, 0x00, /* 1 */
    0x42, 0x61, 0x51, 0x49, 0x46, /* 2 */
    0x21, 0x41, 0x45, 0x4B, 0x31, /* 3 */
    0x18, 0x14, 0x12, 0x7F, 0x10, /* 4 */
    0x27, 0x45, 0x45, 0x45, 0x39, /* 5 */
    0x3C, 0x4A, 0x49, 0x49, 0x30, /* 6 */
    0x01, 0x71, 0x09, 0x05, 0x03, /* 7 */
    0x36, 0x49, 0x49, 0x49, 0x36, /* 8 */
    0x06, 0x49, 0x49, 0x29, 0x1E, /* 9 */
    0x00, 0x36, 0x36, 0x00, 0x00, /* : */
    0x00, 0x56, 0x36, 0x00, 0x00, /* ; */
    0x00, 0x08, 0x14, 0x22, 0x41, /* < */
    0x14, 0x14, 0x14, 0x14, 0x14, /* = */
    0x41, 0x22, 0x14, 0x08, 0x00, /* > */
    0x02, 0x01, 0x51, 0x09, 0x06, /* ? */
    0x32, 0x49, 0x79, 0x41, 0x3E, /* @ */
    0x7E, 0x11, 0x11, 0x11, 0x7E, /* A */
    0x7F, 0x49, 0x49, 0x49, 0x36, /* B */
    0x3E, 0x41, 0x41, 0x41, 0x22, /* C */
    0x7F, 0x41, 0x41, 0x22, 0x1C, /* D */
    0x7F, 0x49, 0x49, 0x49, 0x41, /* E */
    0x7F, 0x09, 0x09, 0x01, 0x01, /* F */
    0x3E, 0x41, 0x41, 0x51, 0x32, /* G */
    0x7F, 0x08, 0x08, 0x08, 0x7F, /* H */
    0x00, 0x41, 0x7F, 0x41, 0x00, /* I */
    0x20, 0x40, 0x41, 0x3F, 0x01, /* J */
    0x7F, 0x08, 0x14, 0x22, 0x41, /* K */
    0x7F, 0x40, 0x40, 0x40, 0x40, /* L */
    0x7F, 0x02, 0x04, 0x02, 0x7F, /* M */
    0x7F, 0x04, 0x08, 0x10, 0x7F, /* N */
    0x3E, 0x41, 0x41, 0x41, 0x3E, /* O */
    0x7F, 0x09, 0x09, 0x09, 0x06, /* P */
    0x3E, 0x41, 0x51, 0x21, 0x5E, /* Q */
    0x7F, 0x09, 0x19, 0x29, 0x46, /* R */
    0x46, 0x49, 0x49, 0x49, 0x31, /* S */
    0x01, 0x01, 0x7F, 0x01, 0x01, /* T */
    0x3F, 0x40, 0x40, 0x40, 0x3F, /* U */
    0x1F, 0x20, 0x40, 0x20, 0x1F, /* V */
    0x7F, 0x20, 0x18, 0x20, 0x7F, /* W */
    0x63, 0x14, 0x08, 0x14, 0x63, /* X */
    0x03, 0x04, 0x78, 0x04, 0x03, /* Y */
    0x61, 0x51, 0x49, 0x45, 0x43, /* Z */
    0x00, 0x00, 0x7F, 0x41, 0x41, /* [ */
    0x02, 0x04, 0x08, 0x10, 0x20, /* \ */
    0x41, 0x41, 0x7F, 0x00, 0x00, /* ] */
    0x04, 0x02, 0x01, 0x02, 0x04, /* ^ */
    0x40, 0x40, 0x40, 0x40, 0x40, /* _ */
    0x00, 0x01, 0x02, 0x04, 0x00, /* ` */
    0x20, 0x54, 0x54, 0x54, 0x78, /* a */
    0x7F, 0x48, 0x44, 0x44, 0x38, /* b */
    0x38, 0x44, 0x44, 0x44, 0x20, /* c */
    0x38, 0x44, 0x44, 0x48, 0x7F, /* d */
    0x38, 0x54, 0x54, 0x54, 0x18, /* e */
    0x08, 0x7E, 0x09, 0x01, 0x02, /* f */
    0x08, 0x14, 0x54, 0x54, 0x3C, /* g */
    0x7F, 0x08, 0x04, 0x04, 0x78, /* h */
    0x00, 0x44, 0x7D, 0x40, 0x00, /* i */
    0x20, 0x40, 0x44, 0x3D, 0x00, /* j */
    0x00, 0x7F, 0x10, 0x28, 0x44, /* k */
    0x00, 0x41, 0x7F, 0x40, 0x00, /* l */
    0x7C, 0x04, 0x18, 0x04, 0x78, /* m */
    0x7C, 0x08, 0x04, 0x04, 0x78, /* n */
    0x38, 0x44, 0x44, 0x44, 0x38, /* o */
    0x7C, 0x14, 0x14, 0x14, 0x08, /* p */
    0x08, 0x14, 0x14, 0x18, 0x7C, /* q */
    0x7C, 0x08, 0x04, 0x04, 0x08, /* r */
    0x48, 0x54, 0x54, 0x54, 0x20, /* s */
    0x04, 0x3F, 0x44, 0x40, 0x20, /* t */
    0x3C, 0x40, 0x40, 0x20, 0x7C, /* u */
    0x1C, 0x20, 0x40, 0x20, 0x1C, /* v */
    0x3C, 0x40, 0x30, 0x40, 0x3C, /* w */
    0x44, 0x28, 0x10, 0x28, 0x44, /* x */
    0x0C, 0x50, 0x50, 0x50, 0x3C, /* y */
    0x44, 0x64, 0x54, 0x4C, 0x44, /* z */
    0x00, 0x08, 0x36, 0x41, 0x00, /* { */
    0x00, 0x00, 0x7F, 0x00, 0x00, /* | */
    0x00, 0x41, 0x36, 0x08, 0x00, /* } */
    0x08, 0x08, 0x2A, 0x1C, 0x08, /* -> */
    0x08, 0x1C, 0x2A, 0x08, 0x08, /* <- */
};

/* Screen state (singleton) */
static screen_state_t s_screen = {
    .buffer = NULL,
    .width = 0,
    .height = 0,
    .byte_length = 0,
    .byte_order = SCREEN_BYTE_ORDER_NATIVE,
    .initialized = false,
    .owns_buffer = false,
    .graphics_handle = GRAPHICS_INVALID_HANDLE,
    .driver = 0,
    .show_callback = 0
};

/* Preset RGB565 colors (native byte order) */
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_YELLOW  0xFFE0
#define COLOR_ORANGE  0xFD20
#define COLOR_GRAY    0x8410

/*
 * Internal helper: swap bytes for SPI displays
 */
static inline uint16_t swap_bytes(uint16_t color) {
    return (color >> 8) | (color << 8);
}

/*
 * Internal helper: apply byte order to color
 */
static inline uint16_t apply_byte_order(uint16_t color) {
    if (s_screen.byte_order == SCREEN_BYTE_ORDER_SWAPPED) {
        return swap_bytes(color);
    }
    return color;
}

/*
 * Color conversion - RGB888 to RGB565
 */
uint16_t screen_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return apply_byte_order(color);
}

/*
 * Color conversion from CSS hex string
 */
uint16_t screen_color_from_hex(const char *hex) {
    if (!hex || hex[0] != '#') {
        return 0;
    }
    
    uint8_t r, g, b;
    size_t len = strlen(hex);
    
    if (len == 4) {
        /* #RGB format */
        r = (hex[1] >= 'A' ? (hex[1] - 'A' + 10) : (hex[1] - '0')) * 17;
        g = (hex[2] >= 'A' ? (hex[2] - 'A' + 10) : (hex[2] - '0')) * 17;
        b = (hex[3] >= 'A' ? (hex[3] - 'A' + 10) : (hex[3] - '0')) * 17;
    } else if (len == 7) {
        /* #RRGGBB format */
        char rs[3] = { hex[1], hex[2], 0 };
        char gs[3] = { hex[3], hex[4], 0 };
        char bs[3] = { hex[5], hex[6], 0 };
        r = (uint8_t)strtol(rs, NULL, 16);
        g = (uint8_t)strtol(gs, NULL, 16);
        b = (uint8_t)strtol(bs, NULL, 16);
    } else {
        return 0;
    }
    
    return screen_rgb(r, g, b);
}

/*
 * Get raw buffer pointer
 */
uint16_t *screen_get_buffer(void) {
    return s_screen.buffer;
}

uint32_t screen_get_buffer_byte_length(void) {
    return s_screen.byte_length;
}

uint16_t screen_get_width(void) {
    return s_screen.width;
}

uint16_t screen_get_height(void) {
    return s_screen.height;
}

bool screen_is_initialized(void) {
    return s_screen.initialized;
}

screen_byte_order_t screen_get_byte_order(void) {
    return s_screen.byte_order;
}

/*
 * Drawing primitives
 */

void screen_fill(uint16_t color) {
    if (!s_screen.initialized || !s_screen.buffer) return;
    
    uint32_t pixel_count = (uint32_t)s_screen.width * (uint32_t)s_screen.height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        s_screen.buffer[i] = color;
    }
}

void screen_set_pixel(int16_t x, int16_t y, uint16_t color) {
    if (!s_screen.initialized || !s_screen.buffer) return;
    if (x < 0 || y < 0 || x >= s_screen.width || y >= s_screen.height) return;
    
    s_screen.buffer[y * s_screen.width + x] = color;
}

void screen_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!s_screen.initialized || !s_screen.buffer) return;
    
    /* Clip to screen bounds */
    int16_t x0 = x < 0 ? 0 : x;
    int16_t y0 = y < 0 ? 0 : y;
    int16_t x1 = x + w > s_screen.width ? s_screen.width : x + w;
    int16_t y1 = y + h > s_screen.height ? s_screen.height : y + h;
    
    for (int16_t py = y0; py < y1; py++) {
        for (int16_t px = x0; px < x1; px++) {
            s_screen.buffer[py * s_screen.width + px] = color;
        }
    }
}

void screen_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (!s_screen.initialized || !s_screen.buffer) return;
    
    /* Bresenham's line algorithm */
    int16_t dx = abs(x1 - x0);
    int16_t dy = -abs(y1 - y0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;
    
    while (1) {
        screen_set_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void screen_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    if (!s_screen.initialized || !s_screen.buffer) return;
    
    /* Midpoint circle algorithm */
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    
    while (x >= y) {
        screen_set_pixel(cx + x, cy + y, color);
        screen_set_pixel(cx + y, cy + x, color);
        screen_set_pixel(cx - y, cy + x, color);
        screen_set_pixel(cx - x, cy + y, color);
        screen_set_pixel(cx - x, cy - y, color);
        screen_set_pixel(cx - y, cy - x, color);
        screen_set_pixel(cx + y, cy - x, color);
        screen_set_pixel(cx + x, cy - y, color);
        
        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }
}

void screen_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    if (!s_screen.initialized || !s_screen.buffer) return;
    
    for (int16_t y = -r; y <= r; y++) {
        for (int16_t x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                screen_set_pixel(cx + x, cy + y, color);
            }
        }
    }
}

void screen_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint8_t size) {
    if (!s_screen.initialized || !s_screen.buffer) return;
    if (c < 32 || c > 127) return;
    
    const uint8_t *glyph = &font5x7[(c - 32) * 5];
    
    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = glyph[i];
        for (int8_t j = 0; j < 7; j++) {
            if (line & (1 << j)) {
                if (size == 1) {
                    screen_set_pixel(x + i, y + j, color);
                } else {
                    screen_fill_rect(x + i * size, y + j * size, size, size, color);
                }
            }
        }
    }
}

void screen_draw_text(int16_t x, int16_t y, const char *text, uint16_t color, uint8_t size) {
    if (!s_screen.initialized || !s_screen.buffer || !text) return;
    
    int16_t cursor_x = x;
    while (*text) {
        if (*text == '\n') {
            cursor_x = x;
            y += 8 * size;
        } else {
            screen_draw_char(cursor_x, y, *text, color, size);
            cursor_x += 6 * size;
        }
        text++;
    }
}

/*
 * JerryScript bindings
 */

/* screen.init(driver, [overrides]) */
static jerry_value_t js_screen_init(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_object(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "screen.init requires a driver object");
    }
    
    jerry_value_t driver = args[0];
    
    /* Get width from driver (or overrides) */
    uint16_t width = 320;
    uint16_t height = 240;
    screen_byte_order_t byte_order = SCREEN_BYTE_ORDER_NATIVE;
    
    /* Check for overrides first */
    if (argc >= 2 && jerry_value_is_object(args[1])) {
        jerry_value_t overrides = args[1];
        
        jerry_value_t w_key = jerry_string_sz("width");
        jerry_value_t w_val = jerry_object_get(overrides, w_key);
        if (jerry_value_is_number(w_val)) {
            width = (uint16_t)jerry_value_as_number(w_val);
        }
        jerry_value_free(w_val);
        jerry_value_free(w_key);
        
        jerry_value_t h_key = jerry_string_sz("height");
        jerry_value_t h_val = jerry_object_get(overrides, h_key);
        if (jerry_value_is_number(h_val)) {
            height = (uint16_t)jerry_value_as_number(h_val);
        }
        jerry_value_free(h_val);
        jerry_value_free(h_key);
    }
    
    /* Fall back to driver defaults */
    jerry_value_t w_key = jerry_string_sz("width");
    jerry_value_t w_val = jerry_object_get(driver, w_key);
    if (jerry_value_is_number(w_val) && argc < 2) {
        width = (uint16_t)jerry_value_as_number(w_val);
    }
    jerry_value_free(w_val);
    jerry_value_free(w_key);
    
    jerry_value_t h_key = jerry_string_sz("height");
    jerry_value_t h_val = jerry_object_get(driver, h_key);
    if (jerry_value_is_number(h_val) && argc < 2) {
        height = (uint16_t)jerry_value_as_number(h_val);
    }
    jerry_value_free(h_val);
    jerry_value_free(h_key);
    
    /* Get byte order from driver */
    jerry_value_t bo_key = jerry_string_sz("byteOrder");
    jerry_value_t bo_val = jerry_object_get(driver, bo_key);
    if (jerry_value_is_string(bo_val)) {
        jerry_size_t str_size = jerry_string_size(bo_val, JERRY_ENCODING_UTF8);
        char *str = malloc(str_size + 1);
        if (str) {
            jerry_string_to_buffer(bo_val, JERRY_ENCODING_UTF8, (jerry_char_t *)str, str_size);
            str[str_size] = '\0';
            if (strcmp(str, "swapped") == 0) {
                byte_order = SCREEN_BYTE_ORDER_SWAPPED;
            }
            free(str);
        }
    }
    jerry_value_free(bo_val);
    jerry_value_free(bo_key);
    
    /* Validate dimensions */
    if (width == 0 || height == 0 || width > SCREEN_MAX_WIDTH || height > SCREEN_MAX_HEIGHT) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, 
            "Invalid screen dimensions");
    }
    
    /* Free existing buffer if we own it */
    if (s_screen.buffer && s_screen.owns_buffer) {
        free(s_screen.buffer);
    }
    s_screen.buffer = NULL;
    s_screen.owns_buffer = false;
    s_screen.graphics_handle = GRAPHICS_INVALID_HANDLE;
    if (s_screen.driver != 0) {
        jerry_value_free(s_screen.driver);
        s_screen.driver = 0;
    }
    if (s_screen.show_callback != 0) {
        jerry_value_free(s_screen.show_callback);
        s_screen.show_callback = 0;
    }
    
    /* Calculate buffer size */
    uint32_t byte_length = (uint32_t)width * (uint32_t)height * sizeof(uint16_t);
    
    /* Check if driver provides an external buffer (e.g., DVI's draw buffer) */
    jerry_value_t buf_key = jerry_string_sz("buffer");
    jerry_value_t buf_val = jerry_object_get(driver, buf_key);
    jerry_value_free(buf_key);
    
    if (jerry_value_is_number(buf_val)) {
        /* Use external buffer - driver provides raw pointer */
        uintptr_t buf_ptr = (uintptr_t)jerry_value_as_number(buf_val);
        s_screen.buffer = (uint16_t *)buf_ptr;
        s_screen.owns_buffer = false;
        jerry_value_free(buf_val);
        
        if (!s_screen.buffer) {
            return jerry_throw_sz(JERRY_ERROR_COMMON, 
                "Driver provided NULL buffer pointer");
        }
    } else {
        jerry_value_free(buf_val);
        
        /* Allocate our own buffer */
        s_screen.buffer = (uint16_t *)malloc(byte_length);
        if (!s_screen.buffer) {
            return jerry_throw_sz(JERRY_ERROR_COMMON, 
                "Failed to allocate screen buffer (out of memory)");
        }
        s_screen.owns_buffer = true;
    }
    
    /* Initialize state */
    memset(s_screen.buffer, 0, byte_length);
    s_screen.width = width;
    s_screen.height = height;
    s_screen.byte_length = byte_length;
    s_screen.byte_order = byte_order;
    s_screen.driver = jerry_value_copy(driver);
    
    /* Cache the show callback */
    jerry_value_t show_key = jerry_string_sz("show");
    s_screen.show_callback = jerry_object_get(driver, show_key);
    jerry_value_free(show_key);
    
    if (!jerry_value_is_function(s_screen.show_callback)) {
        if (s_screen.owns_buffer) free(s_screen.buffer);
        s_screen.buffer = NULL;
        s_screen.owns_buffer = false;
        s_screen.graphics_handle = GRAPHICS_INVALID_HANDLE;
        jerry_value_free(s_screen.driver);
        s_screen.driver = 0;
        jerry_value_free(s_screen.show_callback);
        s_screen.show_callback = 0;
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "Driver must have a show() function");
    }
    
    /* Call driver's init function if present */
    jerry_value_t init_key = jerry_string_sz("init");
    jerry_value_t init_func = jerry_object_get(driver, init_key);
    jerry_value_free(init_key);
    
    if (jerry_value_is_function(init_func)) {
        /* Build config object to pass to init (backward compatible) */
        jerry_value_t config = jerry_object();
        js_set_number(config, "width", width);
        js_set_number(config, "height", height);
        
        jerry_value_t init_args[1] = { config };
        jerry_value_t result = jerry_call(init_func, driver, init_args, 1);
        
        jerry_value_free(config);
        
        if (jerry_value_is_exception(result)) {
            jerry_value_free(init_func);
            if (s_screen.owns_buffer) free(s_screen.buffer);
            s_screen.buffer = NULL;
            s_screen.owns_buffer = false;
            s_screen.graphics_handle = GRAPHICS_INVALID_HANDLE;
            jerry_value_free(s_screen.driver);
            s_screen.driver = 0;
            jerry_value_free(s_screen.show_callback);
            s_screen.show_callback = 0;
            return result;
        }
        jerry_value_free(result);
    }
    jerry_value_free(init_func);
    
    s_screen.initialized = true;
    
    return jerry_undefined();
}

/* screen.fill(color) */
static jerry_value_t js_screen_fill(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (!s_screen.initialized) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Screen not initialized. Call screen.init(driver) first.");
    }
    
    if (argc < 1 || !jerry_value_is_number(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "screen.fill requires a color");
    }
    
    uint16_t color = (uint16_t)jerry_value_as_number(args[0]);
    screen_fill(color);
    
    return jerry_undefined();
}

/* screen.setPixel(x, y, color) */
static jerry_value_t js_screen_set_pixel(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (!s_screen.initialized) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Screen not initialized. Call screen.init(driver) first.");
    }
    
    if (argc < 3) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "screen.setPixel requires x, y, color");
    }
    
    int16_t x = (int16_t)jerry_value_as_number(args[0]);
    int16_t y = (int16_t)jerry_value_as_number(args[1]);
    uint16_t color = (uint16_t)jerry_value_as_number(args[2]);
    
    screen_set_pixel(x, y, color);
    
    return jerry_undefined();
}

/* screen.fillRect(x, y, w, h, color) */
static jerry_value_t js_screen_fill_rect(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (!s_screen.initialized) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Screen not initialized. Call screen.init(driver) first.");
    }
    
    if (argc < 5) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "screen.fillRect requires x, y, w, h, color");
    }
    
    int16_t x = (int16_t)jerry_value_as_number(args[0]);
    int16_t y = (int16_t)jerry_value_as_number(args[1]);
    int16_t w = (int16_t)jerry_value_as_number(args[2]);
    int16_t h = (int16_t)jerry_value_as_number(args[3]);
    uint16_t color = (uint16_t)jerry_value_as_number(args[4]);
    
    screen_fill_rect(x, y, w, h, color);
    
    return jerry_undefined();
}

/* screen.drawLine(x0, y0, x1, y1, color) */
static jerry_value_t js_screen_draw_line(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (!s_screen.initialized) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Screen not initialized. Call screen.init(driver) first.");
    }
    
    if (argc < 5) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "screen.drawLine requires x0, y0, x1, y1, color");
    }
    
    int16_t x0 = (int16_t)jerry_value_as_number(args[0]);
    int16_t y0 = (int16_t)jerry_value_as_number(args[1]);
    int16_t x1 = (int16_t)jerry_value_as_number(args[2]);
    int16_t y1 = (int16_t)jerry_value_as_number(args[3]);
    uint16_t color = (uint16_t)jerry_value_as_number(args[4]);
    
    screen_draw_line(x0, y0, x1, y1, color);
    
    return jerry_undefined();
}

/* screen.drawCircle(cx, cy, r, color) */
static jerry_value_t js_screen_draw_circle(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (!s_screen.initialized) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Screen not initialized. Call screen.init(driver) first.");
    }
    
    if (argc < 4) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "screen.drawCircle requires cx, cy, r, color");
    }
    
    int16_t cx = (int16_t)jerry_value_as_number(args[0]);
    int16_t cy = (int16_t)jerry_value_as_number(args[1]);
    int16_t r = (int16_t)jerry_value_as_number(args[2]);
    uint16_t color = (uint16_t)jerry_value_as_number(args[3]);
    
    screen_draw_circle(cx, cy, r, color);
    
    return jerry_undefined();
}

/* screen.fillCircle(cx, cy, r, color) */
static jerry_value_t js_screen_fill_circle(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (!s_screen.initialized) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Screen not initialized. Call screen.init(driver) first.");
    }
    
    if (argc < 4) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "screen.fillCircle requires cx, cy, r, color");
    }
    
    int16_t cx = (int16_t)jerry_value_as_number(args[0]);
    int16_t cy = (int16_t)jerry_value_as_number(args[1]);
    int16_t r = (int16_t)jerry_value_as_number(args[2]);
    uint16_t color = (uint16_t)jerry_value_as_number(args[3]);
    
    screen_fill_circle(cx, cy, r, color);
    
    return jerry_undefined();
}

/* screen.drawText(x, y, text, color, [size]) */
static jerry_value_t js_screen_draw_text(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (!s_screen.initialized) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Screen not initialized. Call screen.init(driver) first.");
    }
    
    if (argc < 4) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "screen.drawText requires x, y, text, color");
    }
    
    int16_t x = (int16_t)jerry_value_as_number(args[0]);
    int16_t y = (int16_t)jerry_value_as_number(args[1]);
    uint16_t color = (uint16_t)jerry_value_as_number(args[3]);
    uint8_t size = argc >= 5 ? (uint8_t)jerry_value_as_number(args[4]) : 1;
    
    if (size == 0) size = 1;
    
    /* Get text string */
    if (!jerry_value_is_string(args[2])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "text must be a string");
    }
    
    jerry_size_t str_size = jerry_string_size(args[2], JERRY_ENCODING_UTF8);
    char *text = malloc(str_size + 1);
    if (!text) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Out of memory");
    }
    
    jerry_string_to_buffer(args[2], JERRY_ENCODING_UTF8, (jerry_char_t *)text, str_size);
    text[str_size] = '\0';
    
    screen_draw_text(x, y, text, color, size);
    
    free(text);
    
    return jerry_undefined();
}

/* screen.rgb(r, g, b) */
static jerry_value_t js_screen_rgb(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 3) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "screen.rgb requires r, g, b values");
    }
    
    uint8_t r = (uint8_t)jerry_value_as_number(args[0]);
    uint8_t g = (uint8_t)jerry_value_as_number(args[1]);
    uint8_t b = (uint8_t)jerry_value_as_number(args[2]);
    
    return jerry_number((double)screen_rgb(r, g, b));
}

/* screen.color(hex) */
static jerry_value_t js_screen_color(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "screen.color requires a hex string like '#FF0000'");
    }
    
    jerry_size_t str_size = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    char *hex = malloc(str_size + 1);
    if (!hex) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Out of memory");
    }
    
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)hex, str_size);
    hex[str_size] = '\0';
    
    uint16_t color = screen_color_from_hex(hex);
    free(hex);
    
    return jerry_number((double)color);
}

/* screen.show() */
static jerry_value_t js_screen_show(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    if (!s_screen.initialized) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Screen not initialized. Call screen.init(driver) first.");
    }
    
    if (!jerry_value_is_function(s_screen.show_callback)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Driver show() function not available");
    }
    
    /* Call driver's show(buffer, byteLength) */
    jerry_value_t buffer_ptr = jerry_number((double)(uintptr_t)s_screen.buffer);
    jerry_value_t byte_length = jerry_number((double)s_screen.byte_length);
    jerry_value_t show_args[2] = { buffer_ptr, byte_length };
    
    jerry_value_t result = jerry_call(s_screen.show_callback, s_screen.driver, show_args, 2);
    
    jerry_value_free(buffer_ptr);
    jerry_value_free(byte_length);
    
    if (jerry_value_is_exception(result)) {
        return result;
    }
    jerry_value_free(result);

    /* Refresh buffer pointer if driver updates it (e.g., DVI double buffering) */
    jerry_value_t buf_key = jerry_string_sz("buffer");
    jerry_value_t buf_val = jerry_object_get(s_screen.driver, buf_key);
    jerry_value_free(buf_key);
    if (jerry_value_is_number(buf_val)) {
        uintptr_t buf_ptr = (uintptr_t)jerry_value_as_number(buf_val);
        uint16_t *new_buffer = (uint16_t *)buf_ptr;
        if (new_buffer && new_buffer != s_screen.buffer) {
            s_screen.buffer = new_buffer;
            s_screen.owns_buffer = false;
            if (s_screen.graphics_handle != GRAPHICS_INVALID_HANDLE) {
                graphics_replace_buffer(s_screen.graphics_handle, s_screen.buffer,
                                        s_screen.width, s_screen.height);
            }
        }
    }
    jerry_value_free(buf_val);
    
    return jerry_undefined();
}

/* Getter for screen.width */
static jerry_value_t js_screen_width_getter(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    return jerry_number((double)s_screen.width);
}

/* Getter for screen.height */
static jerry_value_t js_screen_height_getter(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    return jerry_number((double)s_screen.height);
}

/* screen.getBufferHandle() - Get a graphics handle for the screen's buffer */
static jerry_value_t js_screen_get_buffer_handle(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    if (!s_screen.initialized || !s_screen.buffer) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Screen not initialized");
    }
    
    if (s_screen.graphics_handle != GRAPHICS_INVALID_HANDLE) {
        uint16_t *data = graphics_get_buffer_data(s_screen.graphics_handle);
        if (data == s_screen.buffer) {
            return jerry_number((double)s_screen.graphics_handle);
        }
        s_screen.graphics_handle = GRAPHICS_INVALID_HANDLE;
    }
    
    /* Register screen's buffer with the graphics system */
    s_screen.graphics_handle = graphics_register_buffer(
        s_screen.buffer, s_screen.width, s_screen.height);
    
    if (s_screen.graphics_handle == GRAPHICS_INVALID_HANDLE) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Failed to register screen buffer");
    }
    
    return jerry_number((double)s_screen.graphics_handle);
}

/* screen.getByteOrder() - Get the byte order string ('native' or 'swapped') */
static jerry_value_t js_screen_get_byte_order(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    if (s_screen.byte_order == SCREEN_BYTE_ORDER_NATIVE) {
        return jerry_string_sz("native");
    } else {
        return jerry_string_sz("swapped");
    }
}

/*
 * Module creation and registration
 */

jerry_value_t js_create_screen_module(void) {
    jerry_value_t module = jerry_object();
    
    /* Functions */
    js_set_function(module, "init", js_screen_init);
    js_set_function(module, "fill", js_screen_fill);
    js_set_function(module, "setPixel", js_screen_set_pixel);
    js_set_function(module, "fillRect", js_screen_fill_rect);
    js_set_function(module, "drawLine", js_screen_draw_line);
    js_set_function(module, "drawCircle", js_screen_draw_circle);
    js_set_function(module, "fillCircle", js_screen_fill_circle);
    js_set_function(module, "drawText", js_screen_draw_text);
    js_set_function(module, "rgb", js_screen_rgb);
    js_set_function(module, "color", js_screen_color);
    js_set_function(module, "show", js_screen_show);
    
    /* Width/height as getter functions (simpler than property descriptors) */
    js_set_function(module, "getWidth", js_screen_width_getter);
    js_set_function(module, "getHeight", js_screen_height_getter);
    
    /* Buffer access for image module integration */
    js_set_function(module, "getBufferHandle", js_screen_get_buffer_handle);
    js_set_function(module, "getByteOrder", js_screen_get_byte_order);
    
    /* Preset colors (native byte order - fixed values) */
    js_set_number(module, "BLACK", COLOR_BLACK);
    js_set_number(module, "WHITE", COLOR_WHITE);
    js_set_number(module, "RED", COLOR_RED);
    js_set_number(module, "GREEN", COLOR_GREEN);
    js_set_number(module, "BLUE", COLOR_BLUE);
    js_set_number(module, "CYAN", COLOR_CYAN);
    js_set_number(module, "MAGENTA", COLOR_MAGENTA);
    js_set_number(module, "YELLOW", COLOR_YELLOW);
    js_set_number(module, "ORANGE", COLOR_ORANGE);
    js_set_number(module, "GRAY", COLOR_GRAY);
    
    return module;
}

void js_bind_screen(void) {
    jerry_value_t module = js_create_screen_module();
    js_register_global("screen", module);
    jerry_value_free(module);
}
