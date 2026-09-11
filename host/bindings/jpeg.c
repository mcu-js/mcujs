#include "jpeg.h"
#include "bindings.h"
#include "runtime_features.h"
#include <stdlib.h>
#include <string.h>
#if !MCUJS_FEATURE_IMAGE
#error "jpeg requires the native picojpeg build lane"
#endif
static void *s_jpeg_reader;
bool jpeg_decoder_busy(void) { return s_jpeg_reader != NULL; }

/* picojpeg byte callback */
unsigned char jpeg_need_bytes_callback(
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

/* Bounded pull reader. The snapshot is never the legacy shared input buffer. */
typedef struct {
    jpeg_stream_t stream;
    pjpeg_image_info_t info;
    uint8_t *snapshot;
    int mcu;
    bool closed;
} jpeg_reader_t;

/* Consume a fresh value and every property-operation result. Own data
 * properties cannot invoke application-defined inherited setters. */
static bool jpeg_define(jerry_value_t object, const char *name, jerry_value_t value) {
    jerry_value_t result = js_define_immutable_property(object, name, value);
    bool ok = jerry_value_is_true(result);
    jerry_value_free(result);
    jerry_value_free(value);
    return ok;
}

static jerry_value_t jpeg_error(jerry_error_t type, const char *code, const char *message) {
    jerry_value_t error = jerry_error_sz(type, message);
    if (!jpeg_define(error, "code", jerry_string_sz(code))) {
        jerry_value_free(error);
        return jerry_throw_sz(JERRY_ERROR_COMMON, "JPEG error construction failed");
    }
    return jerry_throw_value(error, true);
}

static void jpeg_reader_release(jpeg_reader_t *r) {
    if (s_jpeg_reader == r) s_jpeg_reader = NULL;
    free(r->snapshot);
    r->snapshot = NULL;
    r->stream.data = NULL;
}

static void jpeg_reader_free(void *ptr, jerry_object_native_info_t *info) {
    (void)info;
    jpeg_reader_release(ptr);
    free(ptr);
}
static const jerry_object_native_info_t jpeg_reader_type = { .free_cb = jpeg_reader_free };
static const jerry_object_native_info_t jpeg_module_type = { .free_cb = NULL };

/* No property access/coercion: a genuine, attached Uint8Array view only. */
static bool jpeg_byte_view(jerry_value_t value, uint8_t **data, size_t *length) {
    if (!jerry_value_is_typedarray(value) || jerry_typedarray_type(value) != JERRY_TYPEDARRAY_UINT8)
        return false;
    jerry_size_t offset, size;
    jerry_value_t buffer = jerry_typedarray_buffer(value, &offset, &size);
    if (jerry_value_is_exception(buffer)) { jerry_value_free(buffer); return false; }
    size_t total = jerry_arraybuffer_size(buffer);
    bool valid = jerry_arraybuffer_is_detachable(buffer) && offset <= total && size <= total - offset;
    uint8_t *base = valid ? jerry_arraybuffer_data(buffer) : NULL;
    valid = valid && base != NULL;
    if (valid) { *data = base + offset; *length = size; }
    jerry_value_free(buffer);
    return valid;
}

/* picojpeg permits a synthetic EOF and does not consume EOI on completion.
 * Require a complete single baseline scan envelope before leasing the decoder.
 * Segment lengths are walked, never searched inside APP/COM payloads. */
static bool jpeg_reader_envelope(const uint8_t *p, size_t n) {
    if (n < 4 || p[0] != 0xff || p[1] != 0xd8) return false;
    size_t pos = 2;
    bool frame = false;
    unsigned components = 0;
    uint8_t identifiers[3];
    while (pos < n) {
        if (p[pos++] != 0xff) return false;
        while (pos < n && p[pos] == 0xff) pos++;
        if (pos >= n) return false;
        unsigned marker = p[pos++];
        if (marker == 0 || marker == 0xd8 || marker == 0xd9 ||
            (marker >= 0xd0 && marker <= 0xd7)) return false;
        if (n - pos < 2) return false;
        size_t len = ((size_t)p[pos] << 8) | p[pos + 1];
        if (len < 2 || len > n - pos) return false;
        if (marker == 0xc0) {
            if (frame || len < 8 || p[pos + 2] != 8) return false;
            unsigned h = (p[pos + 3] << 8) | p[pos + 4];
            unsigned w = (p[pos + 5] << 8) | p[pos + 6];
            if (!w || !h || w > IMAGE_JPEG_READER_MAX_DIMENSION || h > IMAGE_JPEG_READER_MAX_DIMENSION)
                return false;
            components = p[pos + 7];
            if ((components != 1 && components != 3) || len != 8u + 3u * components)
                return false;
            for (unsigned j = 0; j < components; j++) {
                identifiers[j] = p[pos + 8 + 3 * j];
                for (unsigned k = 0; k < j; k++)
                    if (identifiers[j] == identifiers[k]) return false;
            }
            frame = true;
        } else if (marker >= 0xc1 && marker <= 0xcf && marker != 0xc4) {
            return false; /* progressive, extended, arithmetic, differential */
        }
        if (marker == 0xda) {
            /* Baseline scans use the full spectral band, with no refinement. */
            if (len < 8 || p[pos + 2] != components ||
                len != 6u + 2u * p[pos + 2] ||
                p[pos + len - 3] != 0 || p[pos + len - 2] != 63 ||
                p[pos + len - 1] != 0) return false;
            /* picojpeg decodes MCUs in frame order, not arbitrary SOS order. */
            for (unsigned j = 0; j < components; j++) {
                unsigned tables = p[pos + 4 + 2 * j];
                if (p[pos + 3 + 2 * j] != identifiers[j] ||
                    (tables >> 4) > 1 || (tables & 15) > 1) return false;
            }
        }
        pos += len;
        if (marker != 0xda) continue;
        if (!frame) return false;
        /* Entropy permits stuffed FF bytes and restart markers, then EOI only. */
        bool entropy = false;
        while (pos < n) {
            if (p[pos++] != 0xff) { entropy = true; continue; }
            while (pos < n && p[pos] == 0xff) pos++;
            if (pos >= n) return false;
            marker = p[pos++];
            if (marker == 0) { entropy = true; continue; }
            if (marker >= 0xd0 && marker <= 0xd7) continue;
            return entropy && marker == 0xd9 && pos == n;
        }
        return false;
    }
    return false;
}

static jerry_value_t jpeg_reader_close(const jerry_call_info_t *call, const jerry_value_t args[], jerry_length_t argc) {
    (void)args; (void)argc;
    jpeg_reader_t *r = jerry_object_get_native_ptr(call->this_value, &jpeg_reader_type);
    if (!r) return jpeg_error(JERRY_ERROR_TYPE, "ERR_INVALID_ARG_TYPE", "invalid JPEG reader receiver");
    jpeg_reader_release(r);
    r->closed = true;
    return jerry_undefined();
}

static jerry_value_t jpeg_reader_read(const jerry_call_info_t *call, const jerry_value_t args[], jerry_length_t argc) {
    jpeg_reader_t *r = jerry_object_get_native_ptr(call->this_value, &jpeg_reader_type);
    if (!r) return jpeg_error(JERRY_ERROR_TYPE, "ERR_INVALID_ARG_TYPE", "invalid JPEG reader receiver");
    if (r->closed) return jpeg_error(JERRY_ERROR_COMMON, "ENXIO", "JPEG reader is closed");
    if (!r->snapshot) return jerry_null();
    uint8_t *target; size_t length;
    if (argc != 1 || !jpeg_byte_view(args[0], &target, &length)) {
        jpeg_reader_release(r); r->closed = true;
        return jpeg_error(JERRY_ERROR_TYPE, "ERR_INVALID_ARG_TYPE", "target must be an attached Uint8Array");
    }
    if (length < IMAGE_JPEG_READER_TARGET_BYTES) {
        jpeg_reader_release(r); r->closed = true;
        return jpeg_error(JERRY_ERROR_RANGE, "ERR_OUT_OF_RANGE", "target must hold 768 bytes");
    }
    unsigned char status = pjpeg_decode_mcu(); /* exactly one MCU, no predecode */
    if (status) {
        jpeg_reader_release(r); r->closed = true;
        return jpeg_error(JERRY_ERROR_COMMON, "EIO", "malformed or truncated JPEG entropy");
    }
    const pjpeg_image_info_t *i = &r->info;
    int x = (r->mcu % i->m_MCUSPerRow) * i->m_MCUWidth;
    int y = (r->mcu / i->m_MCUSPerRow) * i->m_MCUHeight;
    int w = i->m_width - x, h = i->m_height - y;
    if (w > i->m_MCUWidth) w = i->m_MCUWidth;
    if (h > i->m_MCUHeight) h = i->m_MCUHeight;
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int src = (py / 8) * 128 + (px / 8) * 64 + (py % 8) * 8 + px % 8;
            *target++ = i->m_pMCUBufR[src];
            *target++ = i->m_comps == 1 ? i->m_pMCUBufR[src] : i->m_pMCUBufG[src];
            *target++ = i->m_comps == 1 ? i->m_pMCUBufR[src] : i->m_pMCUBufB[src];
        }
    }
    if (++r->mcu == i->m_MCUSPerRow * i->m_MCUSPerCol) jpeg_reader_release(r);
    jerry_value_t block = jerry_object();
    if (!jpeg_define(block, "x", jerry_number(x)) ||
        !jpeg_define(block, "y", jerry_number(y)) ||
        !jpeg_define(block, "width", jerry_number(w)) ||
        !jpeg_define(block, "height", jerry_number(h))) {
        jpeg_reader_release(r); r->closed = true;
        jerry_value_free(block);
        return jpeg_error(JERRY_ERROR_COMMON, "EIO", "JPEG block construction failed");
    }
    return block;
}

static jerry_value_t jpeg_open_handler(const jerry_call_info_t *call, const jerry_value_t args[], jerry_length_t argc) {
    if (!jerry_object_has_native_ptr(call->this_value, &jpeg_module_type))
        return jpeg_error(JERRY_ERROR_TYPE, "ERR_INVALID_ARG_TYPE", "invalid jpeg receiver");
    uint8_t *data; size_t length;
    if (argc != 1 || !jpeg_byte_view(args[0], &data, &length))
        return jpeg_error(JERRY_ERROR_TYPE, "ERR_INVALID_ARG_TYPE", "bytes must be an attached Uint8Array");
    if (!length || length > IMAGE_JPEG_READER_MAX_INPUT_BYTES)
        return jpeg_error(JERRY_ERROR_RANGE, "ERR_OUT_OF_RANGE", "JPEG input must contain 1..16384 bytes");
    if (s_jpeg_reader) return jpeg_error(JERRY_ERROR_COMMON, "EBUSY", "JPEG decoder is in use");
    if (!jpeg_reader_envelope(data, length))
        return jpeg_error(JERRY_ERROR_COMMON, "EIO", "expected complete single-scan baseline JPEG, dimensions 1..320");
    jpeg_reader_t *r = calloc(1, sizeof(*r));
    if (!r) return jpeg_error(JERRY_ERROR_COMMON, "ENOMEM", "JPEG reader allocation failed");
    r->snapshot = malloc(length);
    if (!r->snapshot) { free(r); return jpeg_error(JERRY_ERROR_COMMON, "ENOMEM", "JPEG snapshot allocation failed"); }
    memcpy(r->snapshot, data, length);
    r->stream = (jpeg_stream_t){ .data = r->snapshot, .data_len = length };
    s_jpeg_reader = r;
    if (pjpeg_decode_init(&r->info, jpeg_need_bytes_callback, &r->stream, 0) != 0) {
        jpeg_reader_release(r); free(r);
        return jpeg_error(JERRY_ERROR_COMMON, "EIO", "unsupported or malformed baseline JPEG");
    }
    jerry_value_t reader = jerry_object();
    jerry_object_set_native_ptr(reader, &jpeg_reader_type, r);
    if (!jpeg_define(reader, "width", jerry_number(r->info.m_width)) ||
        !jpeg_define(reader, "height", jerry_number(r->info.m_height)) ||
        !jpeg_define(reader, "read", jerry_function_external(jpeg_reader_read)) ||
        !jpeg_define(reader, "close", jerry_function_external(jpeg_reader_close))) {
        jpeg_reader_release(r); r->closed = true;
        jerry_value_free(reader); /* native finalizer owns the reader allocation */
        return jpeg_error(JERRY_ERROR_COMMON, "EIO", "JPEG reader construction failed");
    }
    return reader;
}

jerry_value_t js_create_jpeg_module(void) {
    jerry_value_t module = jerry_object();
    jerry_object_set_native_ptr(module, &jpeg_module_type, NULL);
    if (!jpeg_define(module, "open", jerry_function_external(jpeg_open_handler))) {
        jerry_value_free(module);
        return jpeg_error(JERRY_ERROR_COMMON, "EIO", "JPEG module construction failed");
    }
    return module;
}
