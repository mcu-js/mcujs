/*
 * mcujs - Graphics Buffer Module
 * 
 * Native RGB565 framebuffer management for display drivers.
 * Single buffer strategy to minimize memory usage.
 */

#include "graphics.h"
#include "bindings.h"
#include "jerryscript.h"

#include <stdlib.h>
#include <string.h>

/* Single buffer state (single buffer strategy) */
static uint16_t *s_buffer = NULL;
static uint16_t s_width = 0;
static uint16_t s_height = 0;
static uint32_t s_byte_length = 0;
static graphics_buffer_handle_t s_current_handle = GRAPHICS_INVALID_HANDLE;
static graphics_buffer_handle_t s_next_handle = 0;

/*
 * Core graphics functions
 */

graphics_buffer_handle_t graphics_create_buffer(uint16_t width, uint16_t height) {
    /* Validate dimensions */
    if (width == 0 || height == 0 || 
        width > GRAPHICS_MAX_WIDTH || height > GRAPHICS_MAX_HEIGHT) {
        return GRAPHICS_INVALID_HANDLE;
    }
    
    /* Free existing buffer if any (single buffer strategy) */
    if (s_buffer != NULL) {
        free(s_buffer);
        s_buffer = NULL;
        s_current_handle = GRAPHICS_INVALID_HANDLE;
    }
    
    /* Calculate buffer size */
    uint32_t pixel_count = (uint32_t)width * (uint32_t)height;
    uint32_t byte_length = pixel_count * sizeof(uint16_t);
    
    /* Allocate buffer */
    s_buffer = (uint16_t *)malloc(byte_length);
    if (s_buffer == NULL) {
        return GRAPHICS_INVALID_HANDLE;
    }
    
    /* Initialize to black */
    memset(s_buffer, 0, byte_length);
    
    /* Store state */
    s_width = width;
    s_height = height;
    s_byte_length = byte_length;
    s_current_handle = s_next_handle++;
    
    return s_current_handle;
}

bool graphics_free_buffer(graphics_buffer_handle_t handle) {
    if (handle != s_current_handle || s_buffer == NULL) {
        return false;
    }
    
    free(s_buffer);
    s_buffer = NULL;
    s_width = 0;
    s_height = 0;
    s_byte_length = 0;
    s_current_handle = GRAPHICS_INVALID_HANDLE;
    
    return true;
}

bool graphics_buffer_valid(graphics_buffer_handle_t handle) {
    return (handle == s_current_handle && s_buffer != NULL);
}

bool graphics_get_buffer_info(graphics_buffer_handle_t handle, graphics_buffer_info_t *info) {
    if (!graphics_buffer_valid(handle) || info == NULL) {
        return false;
    }
    
    info->width = s_width;
    info->height = s_height;
    info->byte_length = s_byte_length;
    return true;
}

uint16_t *graphics_get_buffer_data(graphics_buffer_handle_t handle) {
    if (!graphics_buffer_valid(handle)) {
        return NULL;
    }
    return s_buffer;
}

uint32_t graphics_get_buffer_byte_length(graphics_buffer_handle_t handle) {
    if (!graphics_buffer_valid(handle)) {
        return 0;
    }
    return s_byte_length;
}

void graphics_fill(graphics_buffer_handle_t handle, uint16_t color) {
    if (!graphics_buffer_valid(handle)) {
        return;
    }
    
    uint32_t pixel_count = (uint32_t)s_width * (uint32_t)s_height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        s_buffer[i] = color;
    }
}

void graphics_set_pixel(graphics_buffer_handle_t handle, uint16_t x, uint16_t y, uint16_t color) {
    if (!graphics_buffer_valid(handle)) {
        return;
    }
    
    if (x >= s_width || y >= s_height) {
        return;
    }
    
    s_buffer[y * s_width + x] = color;
}

void graphics_fill_rect(graphics_buffer_handle_t handle, 
                        uint16_t x, uint16_t y, 
                        uint16_t w, uint16_t h, 
                        uint16_t color) {
    if (!graphics_buffer_valid(handle)) {
        return;
    }
    
    /* Clip to buffer bounds */
    if (x >= s_width || y >= s_height) {
        return;
    }
    
    uint16_t x_end = x + w;
    uint16_t y_end = y + h;
    
    if (x_end > s_width) x_end = s_width;
    if (y_end > s_height) y_end = s_height;
    
    /* Fill rectangle */
    for (uint16_t py = y; py < y_end; py++) {
        for (uint16_t px = x; px < x_end; px++) {
            s_buffer[py * s_width + px] = color;
        }
    }
}

uint16_t graphics_color565(uint8_t r, uint8_t g, uint8_t b) {
    /* RGB565: RRRRR GGGGGG BBBBB */
    uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    /* Byte-swap for big-endian SPI transmission (display expects MSB first) */
    return (color >> 8) | (color << 8);
}

/*
 * JerryScript bindings
 */

/* graphics.createBuffer({ width, height }) */
static jerry_value_t graphics_create_buffer_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_object(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "graphics.createBuffer requires { width, height }");
    }
    
    jerry_value_t options = args[0];
    
    /* Get width */
    jerry_value_t width_key = jerry_string_sz("width");
    jerry_value_t width_val = jerry_object_get(options, width_key);
    jerry_value_free(width_key);
    
    if (!jerry_value_is_number(width_val)) {
        jerry_value_free(width_val);
        return jerry_throw_sz(JERRY_ERROR_TYPE, "width must be a number");
    }
    uint16_t width = (uint16_t)jerry_value_as_number(width_val);
    jerry_value_free(width_val);
    
    /* Get height */
    jerry_value_t height_key = jerry_string_sz("height");
    jerry_value_t height_val = jerry_object_get(options, height_key);
    jerry_value_free(height_key);
    
    if (!jerry_value_is_number(height_val)) {
        jerry_value_free(height_val);
        return jerry_throw_sz(JERRY_ERROR_TYPE, "height must be a number");
    }
    uint16_t height = (uint16_t)jerry_value_as_number(height_val);
    jerry_value_free(height_val);
    
    /* Create buffer */
    graphics_buffer_handle_t handle = graphics_create_buffer(width, height);
    
    if (handle == GRAPHICS_INVALID_HANDLE) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Failed to create graphics buffer (out of memory or invalid size)");
    }
    
    return jerry_number((double)handle);
}

/* graphics.freeBuffer(handle) */
static jerry_value_t graphics_free_buffer_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_number(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "graphics.freeBuffer requires a buffer handle");
    }
    
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)jerry_value_as_number(args[0]);
    
    if (!graphics_free_buffer(handle)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Invalid buffer handle");
    }
    
    return jerry_undefined();
}

/* graphics.getBufferInfo(handle) */
static jerry_value_t graphics_get_buffer_info_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_number(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "graphics.getBufferInfo requires a buffer handle");
    }
    
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)jerry_value_as_number(args[0]);
    graphics_buffer_info_t info;
    
    if (!graphics_get_buffer_info(handle, &info)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Invalid buffer handle");
    }
    
    jerry_value_t result = jerry_object();
    js_set_number(result, "width", info.width);
    js_set_number(result, "height", info.height);
    js_set_number(result, "byteLength", info.byte_length);
    
    return result;
}

/* graphics.fill(handle, color) */
static jerry_value_t graphics_fill_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "graphics.fill requires handle and color");
    }
    
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)jerry_value_as_number(args[0]);
    uint16_t color = (uint16_t)jerry_value_as_number(args[1]);
    
    if (!graphics_buffer_valid(handle)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Invalid buffer handle");
    }
    
    graphics_fill(handle, color);
    return jerry_undefined();
}

/* graphics.setPixel(handle, x, y, color) */
static jerry_value_t graphics_set_pixel_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 4) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "graphics.setPixel requires handle, x, y, color");
    }
    
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)jerry_value_as_number(args[0]);
    uint16_t x = (uint16_t)jerry_value_as_number(args[1]);
    uint16_t y = (uint16_t)jerry_value_as_number(args[2]);
    uint16_t color = (uint16_t)jerry_value_as_number(args[3]);
    
    if (!graphics_buffer_valid(handle)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Invalid buffer handle");
    }
    
    graphics_set_pixel(handle, x, y, color);
    return jerry_undefined();
}

/* graphics.fillRect(handle, x, y, w, h, color) */
static jerry_value_t graphics_fill_rect_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 6) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "graphics.fillRect requires handle, x, y, w, h, color");
    }
    
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)jerry_value_as_number(args[0]);
    uint16_t x = (uint16_t)jerry_value_as_number(args[1]);
    uint16_t y = (uint16_t)jerry_value_as_number(args[2]);
    uint16_t w = (uint16_t)jerry_value_as_number(args[3]);
    uint16_t h = (uint16_t)jerry_value_as_number(args[4]);
    uint16_t color = (uint16_t)jerry_value_as_number(args[5]);
    
    if (!graphics_buffer_valid(handle)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Invalid buffer handle");
    }
    
    graphics_fill_rect(handle, x, y, w, h, color);
    return jerry_undefined();
}

/* graphics.color565(r, g, b) */
static jerry_value_t graphics_color565_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 3) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "graphics.color565 requires r, g, b values");
    }
    
    uint8_t r = (uint8_t)jerry_value_as_number(args[0]);
    uint8_t g = (uint8_t)jerry_value_as_number(args[1]);
    uint8_t b = (uint8_t)jerry_value_as_number(args[2]);
    
    return jerry_number((double)graphics_color565(r, g, b));
}

/*
 * Module creation and registration
 */

jerry_value_t js_create_graphics_module(void) {
    jerry_value_t module = jerry_object();
    
    js_set_function(module, "createBuffer", graphics_create_buffer_handler);
    js_set_function(module, "freeBuffer", graphics_free_buffer_handler);
    js_set_function(module, "getBufferInfo", graphics_get_buffer_info_handler);
    js_set_function(module, "fill", graphics_fill_handler);
    js_set_function(module, "setPixel", graphics_set_pixel_handler);
    js_set_function(module, "fillRect", graphics_fill_rect_handler);
    js_set_function(module, "color565", graphics_color565_handler);
    
    return module;
}

void js_bind_graphics(void) {
    jerry_value_t module = js_create_graphics_module();
    js_register_global("graphics", module);
    jerry_value_free(module);
}
