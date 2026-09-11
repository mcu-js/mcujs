/* Bounded baseline JPEG reader and shared legacy picojpeg lease boundary. */
#ifndef MCUJS_JPEG_H
#define MCUJS_JPEG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "picojpeg.h"
#include "jerryscript.h"
#define IMAGE_JPEG_READER_MAX_INPUT_BYTES 16384
#define IMAGE_JPEG_READER_MAX_DIMENSION 320
#define IMAGE_JPEG_READER_TARGET_BYTES 768
/* Shared memory stream: legacy decode and reader use the same callback. */
typedef struct {
    const uint8_t *data;
    size_t data_len, data_pos;
} jpeg_stream_t;
unsigned char jpeg_need_bytes_callback(unsigned char *, unsigned char, unsigned char *, void *);
bool jpeg_decoder_busy(void);
jerry_value_t js_create_jpeg_module(void);
#endif
