/*
 * mcujs - Image Decoding Module
 * 
 * Decode JPEG and BMP images directly into graphics buffers.
 * Uses picojpeg (public domain) for JPEG decoding.
 * 
 * Supported formats:
 *   - JPEG: Baseline DCT (via picojpeg)
 *   - BMP: 16-bit (RGB565), 24-bit (BGR), 32-bit (BGRA)
 */

#include "image.h"
#include "bindings.h"
#include "graphics.h"
#include "jerryscript.h"
#include "picojpeg.h"

#include <string.h>
#include <stdio.h>

/*
 * JPEG Decoding using picojpeg
 */

/* Context for picojpeg callback */
typedef struct {
    const uint8_t *data;
    size_t data_len;
    size_t data_pos;
} jpeg_stream_t;

/* picojpeg byte callback */
static unsigned char jpeg_need_bytes_callback(
    unsigned char *buf,
    unsigned char buf_size,
    unsigned char *bytes_read,
    void *callback_data)
{
    jpeg_stream_t *stream = (jpeg_stream_t *)callback_data;
    
    size_t remaining = stream->data_len - stream->data_pos;
    size_t to_read = (remaining < buf_size) ? remaining : buf_size;
    
    if (to_read > 0) {
        memcpy(buf, stream->data + stream->data_pos, to_read);
        stream->data_pos += to_read;
    }
    
    *bytes_read = (unsigned char)to_read;
    return 0;  /* Success */
}

/* Convert RGB888 to byte-swapped RGB565 for SPI displays */
static inline uint16_t rgb_to_565_swapped(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (color >> 8) | (color << 8);  /* Byte swap for big-endian SPI */
}

/* Convert RGB888 to native RGB565 for DVI displays */
static inline uint16_t rgb_to_565_native(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/* Byte order enum for image decoding */
typedef enum {
    IMAGE_BYTE_ORDER_SWAPPED = 0,  /* Default: SPI displays (big-endian) */
    IMAGE_BYTE_ORDER_NATIVE = 1    /* DVI displays (little-endian) */
} image_byte_order_t;

/* Extended JPEG decode with byte order option */
int image_decode_jpeg_ex(graphics_buffer_handle_t handle,
                         const uint8_t *data, size_t data_len,
                         int16_t dest_x, int16_t dest_y,
                         image_byte_order_t byte_order)
{
    if (!graphics_buffer_valid(handle) || data == NULL || data_len == 0) {
        return -1;
    }
    
    /* Get buffer info for clipping */
    graphics_buffer_info_t buf_info;
    if (!graphics_get_buffer_info(handle, &buf_info)) {
        return -1;
    }
    
    uint16_t *buffer = graphics_get_buffer_data(handle);
    if (buffer == NULL) {
        return -1;
    }
    
    /* Setup stream context */
    jpeg_stream_t stream = {
        .data = data,
        .data_len = data_len,
        .data_pos = 0
    };
    
    /* Initialize decoder */
    pjpeg_image_info_t img_info;
    unsigned char status = pjpeg_decode_init(&img_info, jpeg_need_bytes_callback, &stream, 0);
    if (status != 0) {
        return status;
    }
    
    /* Decode MCU by MCU */
    int mcu_x = 0;
    int mcu_y = 0;
    
    /* Block offsets for each scan type (from picojpeg docs) */
    /* Each 8x8 block is 64 bytes, stored contiguously */
    
    while ((status = pjpeg_decode_mcu()) == 0) {
        /* Calculate pixel position for this MCU in the image */
        int mcu_px = mcu_x * img_info.m_MCUWidth;
        int mcu_py = mcu_y * img_info.m_MCUHeight;
        
        /* Number of 8x8 blocks in this MCU */
        int blocks_x = img_info.m_MCUWidth / 8;
        int blocks_y = img_info.m_MCUHeight / 8;
        
        /* Process each 8x8 block in the MCU */
        for (int block_row = 0; block_row < blocks_y; block_row++) {
            for (int block_col = 0; block_col < blocks_x; block_col++) {
                /* Calculate block offset in the MCU buffer */
                /* Layout: YH1V1=0, YH2V1=0,64, YH1V2=0,128, YH2V2=0,64,128,192 */
                int block_offset;
                if (blocks_x == 1 && blocks_y == 1) {
                    /* Single block (grayscale or YH1V1) */
                    block_offset = 0;
                } else if (blocks_x == 2 && blocks_y == 1) {
                    /* YH2V1: horizontal blocks at 0, 64 */
                    block_offset = block_col * 64;
                } else if (blocks_x == 1 && blocks_y == 2) {
                    /* YH1V2: vertical blocks at 0, 128 */
                    block_offset = block_row * 128;
                } else {
                    /* YH2V2: 2x2 blocks at 0, 64, 128, 192 */
                    block_offset = (block_row * 2 + block_col) * 64;
                }
                
                /* Process each pixel in this 8x8 block */
                for (int py = 0; py < 8; py++) {
                    for (int px = 0; px < 8; px++) {
                        /* Image coordinates */
                        int img_x = mcu_px + block_col * 8 + px;
                        int img_y = mcu_py + block_row * 8 + py;
                        
                        /* Skip if outside image bounds */
                        if (img_x >= img_info.m_width || img_y >= img_info.m_height) {
                            continue;
                        }
                        
                        /* Destination coordinates */
                        int dx = dest_x + img_x;
                        int dy = dest_y + img_y;
                        
                        /* Clip to buffer bounds */
                        if (dx < 0 || dx >= buf_info.width || 
                            dy < 0 || dy >= buf_info.height) {
                            continue;
                        }
                        
                        /* Pixel offset within the 8x8 block */
                        int pixel_offset = block_offset + py * 8 + px;
                        
                        uint8_t r, g, b;
                        if (img_info.m_comps == 1) {
                            /* Grayscale */
                            r = g = b = img_info.m_pMCUBufR[pixel_offset];
                        } else {
                            /* RGB */
                            r = img_info.m_pMCUBufR[pixel_offset];
                            g = img_info.m_pMCUBufG[pixel_offset];
                            b = img_info.m_pMCUBufB[pixel_offset];
                        }
                        
                        /* Write to buffer with appropriate byte order */
                        if (byte_order == IMAGE_BYTE_ORDER_NATIVE) {
                            buffer[dy * buf_info.width + dx] = rgb_to_565_native(r, g, b);
                        } else {
                            buffer[dy * buf_info.width + dx] = rgb_to_565_swapped(r, g, b);
                        }
                    }
                }
            }
        }
        
        /* Move to next MCU */
        mcu_x++;
        if (mcu_x >= img_info.m_MCUSPerRow) {
            mcu_x = 0;
            mcu_y++;
        }
    }
    
    /* PJPG_NO_MORE_BLOCKS is expected at end of image */
    if (status == PJPG_NO_MORE_BLOCKS) {
        return 0;
    }
    
    return status;
}

/* Legacy wrapper for backward compatibility - uses swapped byte order for SPI displays */
int image_decode_jpeg(graphics_buffer_handle_t handle,
                      const uint8_t *data, size_t data_len,
                      int16_t dest_x, int16_t dest_y)
{
    return image_decode_jpeg_ex(handle, data, data_len, dest_x, dest_y, IMAGE_BYTE_ORDER_SWAPPED);
}

/*
 * BMP Decoding
 */

/* BMP file header (14 bytes) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  signature[2];    /* 'B', 'M' */
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t data_offset;
} bmp_file_header_t;

/* BMP info header (40 bytes for BITMAPINFOHEADER) */
typedef struct {
    uint32_t header_size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_ppm;
    int32_t  y_ppm;
    uint32_t colors_used;
    uint32_t colors_important;
} bmp_info_header_t;
#pragma pack(pop)

/* Read little-endian uint16 */
static inline uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* Read little-endian uint32 */
static inline uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Read little-endian int32 */
static inline int32_t read_le32_signed(const uint8_t *p) {
    return (int32_t)read_le32(p);
}

/* Extended BMP decode with byte order option */
int image_decode_bmp_ex(graphics_buffer_handle_t handle,
                        const uint8_t *data, size_t data_len,
                        int16_t dest_x, int16_t dest_y,
                        image_byte_order_t byte_order)
{
    if (!graphics_buffer_valid(handle) || data == NULL || data_len < 54) {
        return IMAGE_BMP_ERR_INVALID;
    }
    
    /* Get buffer info */
    graphics_buffer_info_t buf_info;
    if (!graphics_get_buffer_info(handle, &buf_info)) {
        return IMAGE_BMP_ERR_INVALID;
    }
    
    uint16_t *buffer = graphics_get_buffer_data(handle);
    if (buffer == NULL) {
        return IMAGE_BMP_ERR_INVALID;
    }
    
    /* Parse BMP file header */
    if (data[0] != 'B' || data[1] != 'M') {
        return IMAGE_BMP_ERR_INVALID;
    }
    
    uint32_t data_offset = read_le32(data + 10);
    
    /* Parse BMP info header */
    uint32_t header_size = read_le32(data + 14);
    if (header_size < 40) {
        return IMAGE_BMP_ERR_UNSUPPORTED;  /* Only BITMAPINFOHEADER supported */
    }
    
    int32_t width = read_le32_signed(data + 18);
    int32_t height = read_le32_signed(data + 22);
    uint16_t bpp = read_le16(data + 28);
    uint32_t compression = read_le32(data + 30);
    
    /* Validate */
    if (width <= 0 || width > 4096) {
        return IMAGE_BMP_ERR_INVALID;
    }
    
    /* Handle negative height (top-down BMP) */
    bool top_down = false;
    if (height < 0) {
        height = -height;
        top_down = true;
    }
    if (height <= 0 || height > 4096) {
        return IMAGE_BMP_ERR_INVALID;
    }
    
    /* Check supported formats */
    /* Support uncompressed (0) and BI_BITFIELDS (3) for 16-bit */
    if (compression != 0 && !(compression == 3 && bpp == 16)) {
        return IMAGE_BMP_ERR_UNSUPPORTED;
    }
    
    if (bpp != 16 && bpp != 24 && bpp != 32) {
        return IMAGE_BMP_ERR_UNSUPPORTED;
    }
    
    /* Calculate row stride (rows are 4-byte aligned) */
    uint32_t bytes_per_pixel = bpp / 8;
    uint32_t row_size = (width * bytes_per_pixel + 3) & ~3;
    
    /* Validate data length */
    if (data_offset + row_size * height > data_len) {
        return IMAGE_BMP_ERR_TRUNCATED;
    }
    
    /* Decode pixels */
    for (int32_t y = 0; y < height; y++) {
        /* BMP rows are stored bottom-up (unless top_down) */
        int32_t src_y = top_down ? y : (height - 1 - y);
        const uint8_t *row = data + data_offset + src_y * row_size;
        
        for (int32_t x = 0; x < width; x++) {
            /* Calculate destination */
            int dx = dest_x + x;
            int dy = dest_y + y;
            
            /* Clip */
            if (dx < 0 || dx >= buf_info.width || 
                dy < 0 || dy >= buf_info.height) {
                continue;
            }
            
            uint16_t color565;
            
            switch (bpp) {
                case 16: {
                    /* Assume RGB565 format */
                    uint16_t pixel = read_le16(row + x * 2);
                    if (byte_order == IMAGE_BYTE_ORDER_NATIVE) {
                        color565 = pixel;  /* Keep native */
                    } else {
                        color565 = (pixel >> 8) | (pixel << 8);  /* Byte-swap for SPI */
                    }
                    break;
                }
                case 24: {
                    /* BGR format */
                    uint8_t b = row[x * 3 + 0];
                    uint8_t g = row[x * 3 + 1];
                    uint8_t r = row[x * 3 + 2];
                    if (byte_order == IMAGE_BYTE_ORDER_NATIVE) {
                        color565 = rgb_to_565_native(r, g, b);
                    } else {
                        color565 = rgb_to_565_swapped(r, g, b);
                    }
                    break;
                }
                case 32: {
                    /* BGRA format (alpha ignored) */
                    uint8_t b = row[x * 4 + 0];
                    uint8_t g = row[x * 4 + 1];
                    uint8_t r = row[x * 4 + 2];
                    /* Alpha at row[x * 4 + 3] is ignored */
                    if (byte_order == IMAGE_BYTE_ORDER_NATIVE) {
                        color565 = rgb_to_565_native(r, g, b);
                    } else {
                        color565 = rgb_to_565_swapped(r, g, b);
                    }
                    break;
                }
                default:
                    color565 = 0;
                    break;
            }
            
            buffer[dy * buf_info.width + dx] = color565;
        }
    }
    
    return IMAGE_BMP_OK;
}

/* Legacy wrapper for backward compatibility - uses swapped byte order for SPI displays */
int image_decode_bmp(graphics_buffer_handle_t handle,
                     const uint8_t *data, size_t data_len,
                     int16_t dest_x, int16_t dest_y)
{
    return image_decode_bmp_ex(handle, data, data_len, dest_x, dest_y, IMAGE_BYTE_ORDER_SWAPPED);
}

/*
 * Image format detection and info
 */

bool image_get_info(const uint8_t *data, size_t data_len, image_info_t *info) {
    if (data == NULL || info == NULL || data_len < 4) {
        return false;
    }
    
    memset(info, 0, sizeof(image_info_t));
    
    /* Check for JPEG (FFD8FF) */
    if (data_len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        info->format = IMAGE_FORMAT_JPEG;
        
        /* Parse JPEG to find dimensions */
        /* This is simplified - just scan for SOF0 marker */
        size_t pos = 2;
        while (pos < data_len - 8) {
            if (data[pos] != 0xFF) {
                pos++;
                continue;
            }
            
            uint8_t marker = data[pos + 1];
            
            /* SOF0 (baseline DCT) or SOF2 (progressive) */
            if (marker == 0xC0 || marker == 0xC2) {
                info->height = (data[pos + 5] << 8) | data[pos + 6];
                info->width = (data[pos + 7] << 8) | data[pos + 8];
                info->bpp = 24;  /* JPEG is typically 24-bit */
                return true;
            }
            
            /* Skip to next marker */
            if (marker == 0xD8 || marker == 0xD9 || marker == 0x01 ||
                (marker >= 0xD0 && marker <= 0xD7)) {
                /* Markers without length */
                pos += 2;
            } else {
                /* Read length and skip */
                if (pos + 3 >= data_len) break;
                uint16_t len = (data[pos + 2] << 8) | data[pos + 3];
                pos += 2 + len;
            }
        }
        
        /* Couldn't find dimensions but it's JPEG */
        return true;
    }
    
    /* Check for BMP */
    if (data_len >= 54 && data[0] == 'B' && data[1] == 'M') {
        info->format = IMAGE_FORMAT_BMP;
        
        int32_t w = read_le32_signed(data + 18);
        int32_t h = read_le32_signed(data + 22);
        
        info->width = (w > 0) ? w : 0;
        info->height = (h > 0) ? h : -h;  /* Handle negative height */
        info->bpp = read_le16(data + 28);
        
        return true;
    }
    
    return false;
}

/*
 * JerryScript bindings
 */

#include "fs.h"

/* Static buffer for image data - read directly from filesystem */
/* RP2350 has 520KB RAM, RP2040 has 264KB. Adjust buffer size accordingly */
#if defined(PICO_RP2350)
#define IMAGE_BUFFER_SIZE (192 * 1024)  /* 192KB for RP2350 */
#else
#define IMAGE_BUFFER_SIZE (16 * 1024)   /* 16KB for RP2040 (limited RAM) */
#endif
static uint8_t s_image_buffer[IMAGE_BUFFER_SIZE];
static size_t s_image_buffer_len = 0;

/* Helper to load image data from a file path */
static bool load_image_from_file(const char *path, const uint8_t **data, size_t *len) {
    fs_file_t file;
    fs_result_t result = fs_open(&file, path, FS_MODE_READ);
    if (result != FS_OK) {
        return false;
    }
    
    size_t file_size;
    result = fs_size(&file, &file_size);
    if (result != FS_OK || file_size > sizeof(s_image_buffer)) {
        fs_close(&file);
        return false;
    }
    
    size_t bytes_read;
    result = fs_read(&file, s_image_buffer, file_size, &bytes_read);
    fs_close(&file);
    
    if (result != FS_OK) {
        return false;
    }
    
    s_image_buffer_len = bytes_read;
    *data = s_image_buffer;
    *len = bytes_read;
    return true;
}

/* Helper to get data from string argument (legacy, for compatibility) */
static bool get_image_data(jerry_value_t arg, const uint8_t **data, size_t *len) {
    if (!jerry_value_is_string(arg)) {
        return false;
    }
    
    jerry_size_t size = jerry_string_size(arg, JERRY_ENCODING_CESU8);
    if (size == 0) {
        return false;
    }
    
    if (size > sizeof(s_image_buffer)) {
        return false;
    }
    
    jerry_string_to_buffer(arg, JERRY_ENCODING_CESU8, s_image_buffer, size);
    
    *data = s_image_buffer;
    *len = size;
    return true;
}

/* image.info(data) - Get image format and dimensions */
static jerry_value_t image_info_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "image.info requires data argument");
    }
    
    const uint8_t *data;
    size_t data_len;
    if (!get_image_data(args[0], &data, &data_len)) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "data must be a string");
    }
    
    image_info_t info;
    if (!image_get_info(data, data_len, &info)) {
        return jerry_null();  /* Unknown format */
    }
    
    jerry_value_t result = jerry_object();
    js_set_number(result, "width", info.width);
    js_set_number(result, "height", info.height);
    
    const char *format_str = "unknown";
    switch (info.format) {
        case IMAGE_FORMAT_JPEG: format_str = "jpeg"; break;
        case IMAGE_FORMAT_BMP: format_str = "bmp"; break;
        default: break;
    }
    js_set_string(result, "format", format_str);
    
    if (info.format == IMAGE_FORMAT_BMP) {
        js_set_number(result, "bpp", info.bpp);
    }
    
    return result;
}

/* Helper to parse byteOrder from options object */
static bool get_byte_order_from_options(jerry_value_t options, image_byte_order_t *byte_order) {
    *byte_order = IMAGE_BYTE_ORDER_SWAPPED;  /* Default for SPI displays */
    
    jerry_value_t bo_key = jerry_string_sz("byteOrder");
    jerry_value_t bo_val = jerry_object_get(options, bo_key);
    bool ok = true;
    
    if (!jerry_value_is_undefined(bo_val) && !jerry_value_is_null(bo_val)) {
        if (!jerry_value_is_string(bo_val)) {
            ok = false;
        } else {
            jerry_size_t len = jerry_string_size(bo_val, JERRY_ENCODING_UTF8);
            if (len > 0 && len < 16) {
                char buf[16];
                jerry_string_to_buffer(bo_val, JERRY_ENCODING_UTF8, (jerry_char_t *)buf, sizeof(buf) - 1);
                buf[len] = '\0';
                if (strcmp(buf, "native") == 0) {
                    *byte_order = IMAGE_BYTE_ORDER_NATIVE;
                } else if (strcmp(buf, "swapped") == 0) {
                    *byte_order = IMAGE_BYTE_ORDER_SWAPPED;
                } else {
                    ok = false;
                }
            } else {
                ok = false;
            }
        }
    }
    
    jerry_value_free(bo_val);
    jerry_value_free(bo_key);
    
    return ok;
}

/* image.decodeJPEG(handle, data, options) */
static jerry_value_t image_decode_jpeg_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "image.decodeJPEG requires handle and data arguments");
    }
    
    /* Get buffer handle */
    if (!jerry_value_is_number(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "handle must be a number");
    }
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)jerry_value_as_number(args[0]);
    
    /* Get image data */
    const uint8_t *data;
    size_t data_len;
    if (!get_image_data(args[1], &data, &data_len)) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "data must be a string");
    }
    
    /* Get optional x, y, byteOrder from options object */
    int16_t x = 0, y = 0;
    image_byte_order_t byte_order = IMAGE_BYTE_ORDER_SWAPPED;
    if (argc >= 3 && jerry_value_is_object(args[2])) {
        jerry_value_t options = args[2];
        
        jerry_value_t x_key = jerry_string_sz("x");
        jerry_value_t x_val = jerry_object_get(options, x_key);
        if (jerry_value_is_number(x_val)) {
            x = (int16_t)jerry_value_as_number(x_val);
        }
        jerry_value_free(x_val);
        jerry_value_free(x_key);
        
        jerry_value_t y_key = jerry_string_sz("y");
        jerry_value_t y_val = jerry_object_get(options, y_key);
        if (jerry_value_is_number(y_val)) {
            y = (int16_t)jerry_value_as_number(y_val);
        }
        jerry_value_free(y_val);
        jerry_value_free(y_key);
        
        if (!get_byte_order_from_options(options, &byte_order)) {
            return jerry_throw_sz(JERRY_ERROR_TYPE, 
                "byteOrder must be 'native' or 'swapped'");
        }
    }
    
    /* Decode */
    int result = image_decode_jpeg_ex(handle, data, data_len, x, y, byte_order);
    if (result != 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "JPEG decode failed with error %d", result);
        return jerry_throw_sz(JERRY_ERROR_COMMON, msg);
    }
    
    return jerry_undefined();
}

/* image.decodeBMP(handle, data, options) */
static jerry_value_t image_decode_bmp_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "image.decodeBMP requires handle and data arguments");
    }
    
    /* Get buffer handle */
    if (!jerry_value_is_number(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "handle must be a number");
    }
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)jerry_value_as_number(args[0]);
    
    /* Get image data */
    const uint8_t *data;
    size_t data_len;
    if (!get_image_data(args[1], &data, &data_len)) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "data must be a string");
    }
    
    /* Get optional x, y, byteOrder from options object */
    int16_t x = 0, y = 0;
    image_byte_order_t byte_order = IMAGE_BYTE_ORDER_SWAPPED;
    if (argc >= 3 && jerry_value_is_object(args[2])) {
        jerry_value_t options = args[2];
        
        jerry_value_t x_key = jerry_string_sz("x");
        jerry_value_t x_val = jerry_object_get(options, x_key);
        if (jerry_value_is_number(x_val)) {
            x = (int16_t)jerry_value_as_number(x_val);
        }
        jerry_value_free(x_val);
        jerry_value_free(x_key);
        
        jerry_value_t y_key = jerry_string_sz("y");
        jerry_value_t y_val = jerry_object_get(options, y_key);
        if (jerry_value_is_number(y_val)) {
            y = (int16_t)jerry_value_as_number(y_val);
        }
        jerry_value_free(y_val);
        jerry_value_free(y_key);
        
        if (!get_byte_order_from_options(options, &byte_order)) {
            return jerry_throw_sz(JERRY_ERROR_TYPE, 
                "byteOrder must be 'native' or 'swapped'");
        }
    }
    
    /* Decode */
    int result = image_decode_bmp_ex(handle, data, data_len, x, y, byte_order);
    if (result != IMAGE_BMP_OK) {
        const char *msg;
        switch (result) {
            case IMAGE_BMP_ERR_INVALID:
                msg = "Invalid BMP file";
                break;
            case IMAGE_BMP_ERR_UNSUPPORTED:
                msg = "Unsupported BMP format (use 16/24/32-bit uncompressed)";
                break;
            case IMAGE_BMP_ERR_TRUNCATED:
                msg = "BMP file data truncated";
                break;
            default:
                msg = "BMP decode failed";
                break;
        }
        return jerry_throw_sz(JERRY_ERROR_COMMON, msg);
    }
    
    return jerry_undefined();
}

/* Helper to get string argument */
static size_t get_string_arg(const jerry_value_t args[], jerry_length_t argc,
                              jerry_length_t index, char *buffer, size_t buffer_size) {
    if (index >= argc || !jerry_value_is_string(args[index])) {
        return 0;
    }
    jerry_size_t len = jerry_string_to_buffer(args[index], JERRY_ENCODING_UTF8,
                                               (jerry_char_t *)buffer, buffer_size - 1);
    buffer[len] = '\0';
    return len;
}

/* image.drawJPEG(handle, path, options) - Load and decode JPEG from filesystem */
static jerry_value_t image_draw_jpeg_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "image.drawJPEG requires handle and path arguments");
    }
    
    /* Get buffer handle */
    if (!jerry_value_is_number(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "handle must be a number");
    }
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)jerry_value_as_number(args[0]);
    
    /* Get file path */
    char path[64];
    if (get_string_arg(args, argc, 1, path, sizeof(path)) == 0) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "path must be a string");
    }
    
    /* Get optional x, y, byteOrder from options object */
    int16_t x = 0, y = 0;
    image_byte_order_t byte_order = IMAGE_BYTE_ORDER_SWAPPED;
    if (argc >= 3 && jerry_value_is_object(args[2])) {
        jerry_value_t options = args[2];
        
        jerry_value_t x_key = jerry_string_sz("x");
        jerry_value_t x_val = jerry_object_get(options, x_key);
        if (jerry_value_is_number(x_val)) {
            x = (int16_t)jerry_value_as_number(x_val);
        }
        jerry_value_free(x_val);
        jerry_value_free(x_key);
        
        jerry_value_t y_key = jerry_string_sz("y");
        jerry_value_t y_val = jerry_object_get(options, y_key);
        if (jerry_value_is_number(y_val)) {
            y = (int16_t)jerry_value_as_number(y_val);
        }
        jerry_value_free(y_val);
        jerry_value_free(y_key);
        
        if (!get_byte_order_from_options(options, &byte_order)) {
            return jerry_throw_sz(JERRY_ERROR_TYPE, 
                "byteOrder must be 'native' or 'swapped'");
        }
    }
    
    /* Load image from file */
    const uint8_t *data;
    size_t data_len;
    if (!load_image_from_file(path, &data, &data_len)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Failed to load image file");
    }
    
    /* Decode */
    int result = image_decode_jpeg_ex(handle, data, data_len, x, y, byte_order);
    if (result != 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "JPEG decode failed with error %d", result);
        return jerry_throw_sz(JERRY_ERROR_COMMON, msg);
    }
    
    return jerry_undefined();
}

/* image.drawBMP(handle, path, options) - Load and decode BMP from filesystem */
static jerry_value_t image_draw_bmp_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "image.drawBMP requires handle and path arguments");
    }
    
    /* Get buffer handle */
    if (!jerry_value_is_number(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "handle must be a number");
    }
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)jerry_value_as_number(args[0]);
    
    /* Get file path */
    char path[64];
    if (get_string_arg(args, argc, 1, path, sizeof(path)) == 0) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "path must be a string");
    }
    
    /* Get optional x, y, byteOrder from options object */
    int16_t x = 0, y = 0;
    image_byte_order_t byte_order = IMAGE_BYTE_ORDER_SWAPPED;
    if (argc >= 3 && jerry_value_is_object(args[2])) {
        jerry_value_t options = args[2];
        
        jerry_value_t x_key = jerry_string_sz("x");
        jerry_value_t x_val = jerry_object_get(options, x_key);
        if (jerry_value_is_number(x_val)) {
            x = (int16_t)jerry_value_as_number(x_val);
        }
        jerry_value_free(x_val);
        jerry_value_free(x_key);
        
        jerry_value_t y_key = jerry_string_sz("y");
        jerry_value_t y_val = jerry_object_get(options, y_key);
        if (jerry_value_is_number(y_val)) {
            y = (int16_t)jerry_value_as_number(y_val);
        }
        jerry_value_free(y_val);
        jerry_value_free(y_key);
        
        if (!get_byte_order_from_options(options, &byte_order)) {
            return jerry_throw_sz(JERRY_ERROR_TYPE, 
                "byteOrder must be 'native' or 'swapped'");
        }
    }
    
    /* Load image from file */
    const uint8_t *data;
    size_t data_len;
    if (!load_image_from_file(path, &data, &data_len)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Failed to load image file");
    }
    
    /* Decode */
    int result = image_decode_bmp_ex(handle, data, data_len, x, y, byte_order);
    if (result != IMAGE_BMP_OK) {
        const char *msg;
        switch (result) {
            case IMAGE_BMP_ERR_INVALID:
                msg = "Invalid BMP file";
                break;
            case IMAGE_BMP_ERR_UNSUPPORTED:
                msg = "Unsupported BMP format (use 16/24/32-bit uncompressed)";
                break;
            case IMAGE_BMP_ERR_TRUNCATED:
                msg = "BMP file data truncated";
                break;
            default:
                msg = "BMP decode failed";
                break;
        }
        return jerry_throw_sz(JERRY_ERROR_COMMON, msg);
    }
    
    return jerry_undefined();
}

/*
 * Module creation
 */

jerry_value_t js_create_image_module(void) {
    jerry_value_t module = jerry_object();
    
    js_set_function(module, "info", image_info_handler);
    js_set_function(module, "decodeJPEG", image_decode_jpeg_handler);
    js_set_function(module, "decodeBMP", image_decode_bmp_handler);
    js_set_function(module, "drawJPEG", image_draw_jpeg_handler);
    js_set_function(module, "drawBMP", image_draw_bmp_handler);
    
    return module;
}
