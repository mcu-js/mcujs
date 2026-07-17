/*
 * mcujs - Native DVI Output Module
 * 
 * Provides hardware-accelerated DVI/HDMI output using PicoDVI library.
 * This module handles the low-level DVI signaling using PIO state machines
 * and integrates with the unified Screen API.
 * 
 * Usage from JavaScript:
 *   var DVI = require('DVI');
 *   DVI.init(320, 240);  // Initialize with resolution
 *   // ... draw to screen buffer via Screen API ...
 *   DVI.show(buffer);    // Display framebuffer
 */

#ifndef MCUJS_DVI_H
#define MCUJS_DVI_H

#include <stdint.h>
#include <stdbool.h>
#include "jerryscript.h"

#ifdef MCUJS_HAS_DVI
#if MCUJS_HAS_DVI

/*
 * Supported DVI video modes
 * All modes use pixel doubling (320x240 rendered, 640x480 output)
 */
typedef enum {
    MCUJS_DVI_MODE_640x480_60HZ,      /* 640x480p @ 60Hz (252 MHz bit clock) */
    MCUJS_DVI_MODE_COUNT
} mcujs_dvi_mode_t;

/*
 * DVI state
 */
typedef struct {
    uint16_t width;             /* Render width (e.g., 320) */
    uint16_t height;            /* Render height (e.g., 240) */
    uint16_t output_width;      /* Output width (e.g., 640) */
    uint16_t output_height;     /* Output height (e.g., 480) */
    mcujs_dvi_mode_t mode;
    bool initialized;
    bool running;
} mcujs_dvi_state_t;

/*
 * Initialize DVI output with specified resolution
 * Resolution is for the render buffer; actual output is doubled (640x480)
 * 
 * @param width  Render width (max 320)
 * @param height Render height (max 240)
 * @return true on success, false on failure
 */
bool mcujs_dvi_init(uint16_t width, uint16_t height);

/*
 * Start DVI output
 * Must be called after mcujs_dvi_init() and before mcujs_dvi_show()
 * 
 * @return true on success
 */
bool mcujs_dvi_start(void);

/*
 * Stop DVI output
 */
void mcujs_dvi_stop(void);

/*
 * Display a framebuffer on the DVI output
 * The buffer must be RGB565 format, width*height*2 bytes
 * 
 * @param buffer Pointer to RGB565 framebuffer
 * @param length Buffer length in bytes
 * @return true on success
 */
bool mcujs_dvi_show(const uint16_t *buffer, uint32_t length);

/*
 * Check if DVI is initialized and running
 */
bool mcujs_dvi_is_running(void);

/*
 * Get current DVI state
 */
const mcujs_dvi_state_t* mcujs_dvi_get_state(void);

/*
 * Get the draw buffer for direct rendering
 * Returns NULL if DVI is not initialized/started
 * The returned buffer is width*height RGB565 pixels
 */
uint16_t* mcujs_dvi_get_draw_buffer(void);

/*
 * Swap and display the draw buffer
 * Use this after rendering directly to the draw buffer
 */
bool mcujs_dvi_swap_and_show(void);

/*
 * Process DVI rendering - must be called regularly from main loop
 * Feeds scanlines from framebuffer to DVI encoder
 */
void mcujs_dvi_task(void);

/*
 * JerryScript binding
 */
void js_bind_dvi(void);
jerry_value_t js_create_dvi_module(void);

#endif /* MCUJS_HAS_DVI */
#endif /* MCUJS_HAS_DVI defined */

/* Declare task function even when DVI is disabled (will be a no-op) */
#if !defined(MCUJS_HAS_DVI) || !MCUJS_HAS_DVI
void mcujs_dvi_task(void);
#endif

#endif /* MCUJS_DVI_H */
