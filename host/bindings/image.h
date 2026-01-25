/*
 * mcujs - Image Decoding Module
 * 
 * Decode JPEG and BMP images directly into graphics buffers.
 * Uses picojpeg (public domain) for JPEG decoding.
 */

#ifndef MCUJS_IMAGE_H
#define MCUJS_IMAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "graphics.h"

/* Image format detection */
typedef enum {
    IMAGE_FORMAT_UNKNOWN = 0,
    IMAGE_FORMAT_JPEG,
    IMAGE_FORMAT_BMP
} image_format_t;

/* Image info structure */
typedef struct {
    uint16_t width;
    uint16_t height;
    image_format_t format;
    uint8_t bpp;          /* Bits per pixel (for BMP) */
} image_info_t;

/*
 * Detect image format and get dimensions without full decode.
 * Returns true on success, false if format not recognized.
 */
bool image_get_info(const uint8_t *data, size_t data_len, image_info_t *info);

/*
 * Decode JPEG image into a graphics buffer.
 * 
 * handle: Target graphics buffer handle
 * data: JPEG file data
 * data_len: Length of data
 * x, y: Destination position in buffer
 * 
 * Returns 0 on success, or picojpeg error code on failure.
 */
int image_decode_jpeg(graphics_buffer_handle_t handle,
                      const uint8_t *data, size_t data_len,
                      int16_t x, int16_t y);

/*
 * Decode BMP image into a graphics buffer.
 * Supports 16-bit (RGB565), 24-bit (BGR), and 32-bit (BGRA) BMPs.
 * 
 * handle: Target graphics buffer handle
 * data: BMP file data
 * data_len: Length of data
 * x, y: Destination position in buffer
 * 
 * Returns 0 on success, negative error code on failure.
 */
int image_decode_bmp(graphics_buffer_handle_t handle,
                     const uint8_t *data, size_t data_len,
                     int16_t x, int16_t y);

/* BMP decode error codes */
#define IMAGE_BMP_OK              0
#define IMAGE_BMP_ERR_INVALID    -1  /* Not a valid BMP file */
#define IMAGE_BMP_ERR_UNSUPPORTED -2  /* Unsupported BMP format */
#define IMAGE_BMP_ERR_TRUNCATED  -3  /* File data truncated */

#endif /* MCUJS_IMAGE_H */
