#ifndef MCUJS_CANVAS_RENDERER_H
#define MCUJS_CANVAS_RENDERER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool canvas_render(uint16_t *pixels, int width, int height,
                   const float *commands, size_t count, bool stroke,
                   const float rgba[4], float line_width);
#endif
