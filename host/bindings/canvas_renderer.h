#ifndef MCUJS_CANVAS_RENDERER_H
#define MCUJS_CANVAS_RENDERER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* The larger ctx scratch profile is exclusive to Sticky; RP stays unchanged. */
#ifdef MCUJS_CANVAS_STICKY
#define MCUJS_CANVAS_MAX_WIDTH 800
#define MCUJS_CANVAS_MAX_HEIGHT 480
#define MCUJS_CANVAS_COORD_LIMIT 1024
#else
#define MCUJS_CANVAS_MAX_WIDTH 320
#define MCUJS_CANVAS_MAX_HEIGHT 320
#define MCUJS_CANVAS_COORD_LIMIT 512
#endif
bool canvas_render(uint16_t *pixels, int width, int height,
                   const float *commands, size_t count, bool stroke,
                   const float rgba[4], float line_width);
#endif
