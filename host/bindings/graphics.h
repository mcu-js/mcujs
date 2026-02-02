/*
 * mcujs - Graphics Buffer Module
 * 
 * Native RGB565 framebuffer management for display drivers.
 * Single buffer strategy to minimize memory usage.
 */

#ifndef MCUJS_GRAPHICS_H
#define MCUJS_GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Buffer handle type - opaque to JavaScript */
typedef int32_t graphics_buffer_handle_t;

/* Invalid handle constant */
#define GRAPHICS_INVALID_HANDLE (-1)

/* Maximum buffer dimensions (safety limit) */
#define GRAPHICS_MAX_WIDTH  320
#define GRAPHICS_MAX_HEIGHT 320

/* Buffer info structure */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t byte_length;
} graphics_buffer_info_t;

/*
 * Create a new graphics buffer.
 * Returns handle >= 0 on success, GRAPHICS_INVALID_HANDLE on failure.
 * Only one buffer can exist at a time (single buffer strategy).
 */
graphics_buffer_handle_t graphics_create_buffer(uint16_t width, uint16_t height);

/*
 * Register an external buffer (e.g., screen's buffer) with the graphics system.
 * The graphics system will NOT free this buffer - caller retains ownership.
 * Returns a handle that can be used with image decoding functions.
 */
graphics_buffer_handle_t graphics_register_buffer(uint16_t *buffer, uint16_t width, uint16_t height);

/*
 * Free the graphics buffer.
 * Returns true on success, false if handle is invalid.
 * Note: Does not free external buffers registered via graphics_register_buffer.
 */
bool graphics_free_buffer(graphics_buffer_handle_t handle);

/*
 * Check if a buffer handle is valid.
 */
bool graphics_buffer_valid(graphics_buffer_handle_t handle);

/*
 * Get buffer info.
 * Returns true on success, false if handle is invalid.
 */
bool graphics_get_buffer_info(graphics_buffer_handle_t handle, graphics_buffer_info_t *info);

/*
 * Get raw pointer to buffer data.
 * Used for DMA transfers. Returns NULL if handle is invalid.
 */
uint16_t *graphics_get_buffer_data(graphics_buffer_handle_t handle);

/*
 * Replace the backing buffer for an existing handle.
 * Keeps the handle stable for callers that cache it.
 */
bool graphics_replace_buffer(graphics_buffer_handle_t handle, uint16_t *buffer,
                             uint16_t width, uint16_t height);

/*
 * Get buffer byte length.
 * Returns 0 if handle is invalid.
 */
uint32_t graphics_get_buffer_byte_length(graphics_buffer_handle_t handle);

/*
 * Fill entire buffer with a single color.
 */
void graphics_fill(graphics_buffer_handle_t handle, uint16_t color);

/*
 * Set a single pixel.
 */
void graphics_set_pixel(graphics_buffer_handle_t handle, uint16_t x, uint16_t y, uint16_t color);

/*
 * Fill a rectangle.
 */
void graphics_fill_rect(graphics_buffer_handle_t handle, 
                        uint16_t x, uint16_t y, 
                        uint16_t w, uint16_t h, 
                        uint16_t color);

/*
 * Convert RGB888 to RGB565.
 */
uint16_t graphics_color565(uint8_t r, uint8_t g, uint8_t b);

#endif /* MCUJS_GRAPHICS_H */
