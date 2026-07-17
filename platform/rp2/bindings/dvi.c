/*
 * mcujs - Native DVI Output Module
 * 
 * Provides hardware-accelerated DVI/HDMI output using PicoDVI library.
 * 
 * Architecture (based on PicoDVI sprite_bounce example):
 * - Core 0: JavaScript runtime, renders to framebuffer, feeds scanlines to Core 1
 * - Core 1: TMDS encoding (dvi_scanbuf_main_16bpp)
 * - DMA IRQ: Outputs encoded TMDS data
 * 
 * The key insight is that Core 0 must render scanlines and feed them to Core 1
 * in sync with the display timing. Core 1 just does TMDS encoding.
 */

#include "mcujs_dvi.h"
#include "bindings.h"

#ifdef MCUJS_HAS_DVI
#if MCUJS_HAS_DVI

/* Pico SDK includes */
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/sync.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/timer.h"

/* PicoDVI library */
#include "dvi.h"
#include "dvi_serialiser.h"
#include "common_dvi_pin_configs.h"
#include "tmds_encode.h"

#include "screen.h"

#include <stdlib.h>
#include <string.h>

/* DVI timing for 640x480p 60Hz */
#define MCUJS_DVI_TIMING dvi_timing_640x480p_60hz
#define MCUJS_VREG_VSEL VREG_VOLTAGE_1_20

/* Frame dimensions - 160x120 framebuffer, 4x scaled to 640x480 */
#define FRAME_WIDTH 160
#define FRAME_HEIGHT 120

/* Scanline buffer width - PicoDVI needs 320 pixels for 640 output (2x internal scaling) */
#define SCANLINE_WIDTH 320

/* Number of scanline buffers for the colour queue */
#define N_SCANLINE_BUFFERS 4

/* DVI instance from PicoDVI */
static struct dvi_inst s_dvi_inst;

/* mcujs DVI state */
static mcujs_dvi_state_t s_mcujs_dvi_state = {
    .width = 0,
    .height = 0,
    .output_width = 640,
    .output_height = 480,
    .mode = MCUJS_DVI_MODE_640x480_60HZ,
    .initialized = false,
    .running = false
};

/* Scanline buffers - Core 0 renders into these, Core 1 encodes them */
static uint16_t s_scanline_buffers[N_SCANLINE_BUFFERS][SCANLINE_WIDTH];

/* Double buffered framebuffers */
static uint16_t s_framebuffer_a[FRAME_WIDTH * FRAME_HEIGHT];
static uint16_t s_framebuffer_b[FRAME_WIDTH * FRAME_HEIGHT];

/* Display buffer - what scanline renderer reads from (set atomically) */
static volatile const uint16_t *s_display_buffer = NULL;
/* Draw buffer - what Core 0 writes to */
static uint16_t *s_draw_buffer = NULL;
static uint32_t s_framebuffer_size = 0;

/* Flag for vsync - set when frame rendering completes */
static volatile bool s_frame_complete = false;

/*
 * Core 1 main - runs dvi_scanbuf_main_16bpp
 * This is the standard PicoDVI pattern: pull scanlines from q_colour_valid,
 * TMDS encode them, push to q_tmds_valid
 */
static void __not_in_flash("dvi") core1_main(void) {
    dvi_register_irqs_this_core(&s_dvi_inst, DMA_IRQ_0);
    
    /* Wait for first scanline to be queued before starting */
    while (queue_is_empty(&s_dvi_inst.q_colour_valid)) {
        __wfe();
    }
    
    dvi_start(&s_dvi_inst);
    
    /* This never returns - it continuously encodes scanlines */
    dvi_scanbuf_main_16bpp(&s_dvi_inst);
    
    __builtin_unreachable();
}

/*
 * Render one frame's worth of scanlines from framebuffer to scanline queue
 * This runs on Core 0 and must keep up with display timing
 */
static void __not_in_flash("dvi") render_scanlines(void) {
    const uint16_t *fb = (const uint16_t *)s_display_buffer;
    if (!fb) return;
    
    /* Render 240 scanlines (each framebuffer line is used 4 times for 4x vertical scaling) */
    for (uint y = 0; y < 240; y++) {
        /* Get a free scanline buffer */
        uint16_t *scanline;
        queue_remove_blocking(&s_dvi_inst.q_colour_free, &scanline);
        
        /* Calculate source line (4x vertical scaling: 240 -> 60, but we have 120 lines) */
        /* Actually for 120 lines -> 480 output, each line repeats 4 times */
        /* But we're outputting 240 lines to PicoDVI which doubles to 480 */
        /* So 240/2 = 120 source lines, each used twice */
        uint src_y = y / 2;
        if (src_y >= FRAME_HEIGHT) {
            src_y = FRAME_HEIGHT - 1;
        }
        
        const uint16_t *src_line = &fb[src_y * FRAME_WIDTH];
        
        /* Double pixels horizontally: 160 -> 320 */
        for (uint x = 0; x < FRAME_WIDTH; x++) {
            uint16_t pixel = src_line[x];
            scanline[x * 2] = pixel;
            scanline[x * 2 + 1] = pixel;
        }
        
        /* Queue for Core 1 to encode */
        queue_add_blocking(&s_dvi_inst.q_colour_valid, &scanline);
    }
    
    s_frame_complete = true;
}

/*
 * Initialize DVI output
 */
bool mcujs_dvi_init(uint16_t width, uint16_t height) {
    if (s_mcujs_dvi_state.initialized) {
        return true;
    }
    
    if (width == 0 || height == 0 || width > FRAME_WIDTH || height > FRAME_HEIGHT) {
        return false;
    }
    
    /* Configure DVI instance */
    s_dvi_inst.timing = &MCUJS_DVI_TIMING;
    s_dvi_inst.ser_cfg = waveshare_rp2040_pizero;
    
    /* Initialize DVI */
    dvi_init(&s_dvi_inst, next_striped_spin_lock_num(), next_striped_spin_lock_num());
    
    s_mcujs_dvi_state.width = width;
    s_mcujs_dvi_state.height = height;
    s_mcujs_dvi_state.output_width = 640;
    s_mcujs_dvi_state.output_height = 480;
    s_mcujs_dvi_state.initialized = true;
    
    return true;
}

/*
 * Start DVI output
 */
bool mcujs_dvi_start(void) {
    if (!s_mcujs_dvi_state.initialized) {
        return false;
    }
    
    if (s_mcujs_dvi_state.running) {
        return true;
    }
    
    /* Initialize framebuffers */
    s_framebuffer_size = (uint32_t)s_mcujs_dvi_state.width * s_mcujs_dvi_state.height;
    memset(s_framebuffer_a, 0, s_framebuffer_size * sizeof(uint16_t));
    memset(s_framebuffer_b, 0, s_framebuffer_size * sizeof(uint16_t));
    
    s_display_buffer = s_framebuffer_a;
    s_draw_buffer = s_framebuffer_b;
    
    /* Add scanline buffers to the free queue */
    for (int i = 0; i < N_SCANLINE_BUFFERS; i++) {
        void *bufptr = &s_scanline_buffers[i];
        queue_add_blocking(&s_dvi_inst.q_colour_free, &bufptr);
    }
    
    /* Give DMA priority to Core 1 */
    hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);
    
    /* Launch Core 1 for TMDS encoding */
    multicore_launch_core1(core1_main);
    
    s_mcujs_dvi_state.running = true;
    
    /* Start rendering the first frame to kick things off */
    render_scanlines();
    
    return true;
}

/*
 * Stop DVI output
 */
void mcujs_dvi_stop(void) {
    if (!s_mcujs_dvi_state.running) {
        return;
    }
    
    multicore_reset_core1();
    s_mcujs_dvi_state.running = false;
}

/*
 * Display a framebuffer - copies to draw buffer, then swaps and renders
 */
bool mcujs_dvi_show(const uint16_t *buffer, uint32_t length) {
    if (!s_mcujs_dvi_state.initialized || !s_display_buffer || !s_draw_buffer || !buffer) {
        return false;
    }
    
    uint32_t expected_size = s_framebuffer_size * sizeof(uint16_t);
    if (length < expected_size) {
        return false;
    }
    
    /* Copy to draw buffer */
    memcpy(s_draw_buffer, buffer, expected_size);
    
    /* Swap buffers atomically */
    uint16_t *old_display = (uint16_t *)s_display_buffer;
    s_display_buffer = s_draw_buffer;
    s_draw_buffer = old_display;
    __dmb();
    
    /* Render the new frame */
    s_frame_complete = false;
    render_scanlines();
    
    return true;
}

/*
 * Process DVI rendering - called from main loop
 * Continuously renders frames to keep display fed
 */
void mcujs_dvi_task(void) {
    if (!s_mcujs_dvi_state.running) {
        return;
    }
    
    /* If last frame is done, render another one (to keep display fed) */
    if (s_frame_complete) {
        s_frame_complete = false;
        render_scanlines();
    }
}

bool mcujs_dvi_is_running(void) {
    return s_mcujs_dvi_state.running;
}

const mcujs_dvi_state_t* mcujs_dvi_get_state(void) {
    return &s_mcujs_dvi_state;
}

uint16_t* mcujs_dvi_get_draw_buffer(void) {
    if (!s_mcujs_dvi_state.initialized || !s_mcujs_dvi_state.running) {
        return NULL;
    }
    return s_draw_buffer;
}

bool mcujs_dvi_swap_and_show(void) {
    if (!s_mcujs_dvi_state.initialized || !s_mcujs_dvi_state.running || !s_display_buffer || !s_draw_buffer) {
        return false;
    }
    
    /* Swap buffers atomically */
    uint16_t *old_display = (uint16_t *)s_display_buffer;
    s_display_buffer = s_draw_buffer;
    s_draw_buffer = old_display;
    __dmb();
    
    /* Render the new frame */
    s_frame_complete = false;
    render_scanlines();
    
    return true;
}

/*
 * =============================================================================
 * JerryScript Bindings
 * =============================================================================
 */

static jerry_value_t js_dvi_init(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    uint16_t width = FRAME_WIDTH;
    uint16_t height = FRAME_HEIGHT;
    
    if (argc >= 1 && jerry_value_is_number(args[0])) {
        width = (uint16_t)jerry_value_as_number(args[0]);
    }
    if (argc >= 2 && jerry_value_is_number(args[1])) {
        height = (uint16_t)jerry_value_as_number(args[1]);
    }
    
    if (!mcujs_dvi_init(width, height)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Failed to initialize DVI output");
    }
    
    return jerry_boolean(true);
}

static jerry_value_t js_dvi_start(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    if (!mcujs_dvi_start()) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Failed to start DVI output");
    }
    
    return jerry_boolean(true);
}

static jerry_value_t js_dvi_stop(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    mcujs_dvi_stop();
    
    return jerry_undefined();
}

/* DVI.fill(color) - Fill display buffer and render */
static jerry_value_t js_dvi_fill(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_number(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "DVI.fill requires color (RGB565)");
    }
    
    uint16_t color = (uint16_t)jerry_value_as_number(args[0]);
    
    if (!s_draw_buffer) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "DVI not started");
    }
    
    /* Fill draw buffer */
    for (uint32_t i = 0; i < s_framebuffer_size; i++) {
        s_draw_buffer[i] = color;
    }
    
    /* Swap and render */
    uint16_t *old_display = (uint16_t *)s_display_buffer;
    s_display_buffer = s_draw_buffer;
    s_draw_buffer = old_display;
    __dmb();
    
    s_frame_complete = false;
    render_scanlines();
    
    return jerry_undefined();
}

static jerry_value_t js_dvi_show(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "DVI.show requires bufferPtr and byteLength");
    }
    
    if (!jerry_value_is_number(args[0]) || !jerry_value_is_number(args[1])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "Invalid arguments to DVI.show");
    }
    
    uintptr_t buffer_ptr = (uintptr_t)jerry_value_as_number(args[0]);
    uint32_t byte_length = (uint32_t)jerry_value_as_number(args[1]);
    
    if (!mcujs_dvi_show((const uint16_t *)buffer_ptr, byte_length)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, 
            "Failed to display framebuffer");
    }
    
    return jerry_undefined();
}

static jerry_value_t js_dvi_is_running(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    return jerry_boolean(mcujs_dvi_is_running());
}

/* DVI.getDrawBuffer() - Returns pointer to draw buffer for direct rendering */
static jerry_value_t js_dvi_get_draw_buffer(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    uint16_t *buffer = mcujs_dvi_get_draw_buffer();
    if (!buffer) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "DVI not started");
    }
    
    return jerry_number((double)(uintptr_t)buffer);
}

/* DVI.getBufferSize() - Returns size in bytes of the draw buffer */
static jerry_value_t js_dvi_get_buffer_size(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    return jerry_number((double)(FRAME_WIDTH * FRAME_HEIGHT * sizeof(uint16_t)));
}

/* DVI.swapAndShow() - Swap buffers and display (for direct rendering) */
static jerry_value_t js_dvi_swap_and_show(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    if (!mcujs_dvi_swap_and_show()) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Failed to swap/show DVI buffer");
    }
    
    return jerry_undefined();
}

jerry_value_t js_create_dvi_module(void) {
    jerry_value_t module = jerry_object();
    
    js_set_function(module, "init", js_dvi_init);
    js_set_function(module, "start", js_dvi_start);
    js_set_function(module, "stop", js_dvi_stop);
    js_set_function(module, "show", js_dvi_show);
    js_set_function(module, "fill", js_dvi_fill);
    js_set_function(module, "isRunning", js_dvi_is_running);
    js_set_function(module, "getDrawBuffer", js_dvi_get_draw_buffer);
    js_set_function(module, "getBufferSize", js_dvi_get_buffer_size);
    js_set_function(module, "swapAndShow", js_dvi_swap_and_show);
    
    js_set_number(module, "width", FRAME_WIDTH);
    js_set_number(module, "height", FRAME_HEIGHT);
    js_set_string(module, "byteOrder", "native");
    
    return module;
}

void js_bind_dvi(void) {
    jerry_value_t module = js_create_dvi_module();
    js_register_global("DVI", module);
    jerry_value_free(module);
}

#endif /* MCUJS_HAS_DVI == 1 */
#endif /* MCUJS_HAS_DVI defined */

/* Stub implementations when DVI is not available */
#if !defined(MCUJS_HAS_DVI) || !MCUJS_HAS_DVI

#include "jerryscript.h"

void js_bind_dvi(void) {
    /* No-op: DVI not available on this board */
}

jerry_value_t js_create_dvi_module(void) {
    return jerry_undefined();
}

void mcujs_dvi_task(void) {
    /* No-op: DVI not available */
}

#endif /* !MCUJS_HAS_DVI */
