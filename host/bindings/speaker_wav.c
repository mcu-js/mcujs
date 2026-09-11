#include "speaker_wav.h"
#include <limits.h>
#include <string.h>
static uint16_t le16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return (uint32_t)le16(p) | (uint32_t)le16(p+2) << 16; }
static speaker_wav_result_t exact(speaker_wav_t *w, void *p, size_t n) {
    size_t got = 0;
    w->fs_error = fs_read(&w->file, p, n, &got);
    return w->fs_error == FS_OK && got == n ? SPEAKER_WAV_OK : SPEAKER_WAV_IO;
}
void speaker_wav_close(speaker_wav_t *w) {
    if (w->file.is_open) (void)fs_close(&w->file);
    w->remaining = 0;
}
speaker_wav_result_t speaker_wav_open(speaker_wav_t *w, const char *path) {
    uint8_t h[16]; size_t size = 0; uint32_t data = 0, data_size = 0;
    bool fmt = false, found_data = false;
    speaker_wav_result_t result = SPEAKER_WAV_INVALID;
    w->fs_error = fs_open(&w->file, path, FS_MODE_READ);
    if (w->fs_error != FS_OK) return SPEAKER_WAV_IO;
    w->fs_error = fs_size(&w->file, &size);
    if (w->fs_error != FS_OK) { result = SPEAKER_WAV_IO; goto fail; }
    if (size < 12 || size > INT32_MAX) goto fail;
    if (exact(w,h,12)) { result = SPEAKER_WAV_IO; goto fail; }
    if (memcmp(h,"RIFF",4) || memcmp(h+8,"WAVE",4) || le32(h+4) != size-8) goto fail;
    uint32_t pos = 12; unsigned chunks = 0;
    while (pos < size) {
        if (++chunks > SPEAKER_WAV_MAX_CHUNKS || size-pos < 8) goto fail;
        w->fs_error = fs_seek(&w->file,pos);
        if (w->fs_error != FS_OK || exact(w,h,8)) { result = SPEAKER_WAV_IO; goto fail; }
        uint32_t n = le32(h+4); pos += 8;
        if (n > size-pos || (n & 1u) > size-pos-n) goto fail;
        if (!memcmp(h,"fmt ",4)) {
            if (fmt || n < 16) goto fail;
            if (exact(w,h,16)) { result = SPEAKER_WAV_IO; goto fail; }
            if (le16(h)!=1 || le16(h+2)!=1 || le32(h+4)!=16000 ||
                le32(h+8)!=32000 || le16(h+12)!=2 || le16(h+14)!=16 || n!=16) {
                result = SPEAKER_WAV_UNSUPPORTED; goto fail;
            }
            fmt = true;
        } else if (!memcmp(h,"data",4)) {
            if (found_data || (n & 1u)) goto fail;
            found_data = true; data = pos; data_size = n;
        }
        pos += n + (n & 1u);
    }
    if (!fmt || !found_data) goto fail;
    w->fs_error = fs_seek(&w->file,data);
    if (w->fs_error != FS_OK) { result = SPEAKER_WAV_IO; goto fail; }
    w->remaining = data_size;
    return SPEAKER_WAV_OK;
fail:
    speaker_wav_close(w);
    return result;
}
speaker_wav_result_t speaker_wav_read(speaker_wav_t *w, uint32_t *frames, size_t capacity, double volume, size_t *count) {
    uint8_t bytes[SPEAKER_FRAMES * 2];
    *count = 0;
    if (capacity > SPEAKER_FRAMES) return SPEAKER_WAV_INVALID;
    size_t n = w->remaining / 2;
    if (n > capacity) n = capacity;
    if (!n) return SPEAKER_WAV_OK;
    if (exact(w,bytes,n*2)) return SPEAKER_WAV_IO;
    for (size_t i=0;i<n;i++) {
        uint16_t raw = le16(bytes+i*2);
        int32_t signed_sample = raw < 32768 ? raw : (int32_t)raw - 65536;
        uint16_t sample = (uint16_t)(int32_t)(signed_sample * volume); // truncate toward zero
        frames[i] = (uint32_t)sample << 16 | sample;
    }
    w->remaining -= n*2; *count = n;
    return SPEAKER_WAV_OK;
}
