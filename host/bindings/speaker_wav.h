#ifndef MCUJS_SPEAKER_WAV_H
#define MCUJS_SPEAKER_WAV_H
#include "fs.h"
#define SPEAKER_FRAMES 512
#define SPEAKER_WAV_MAX_CHUNKS 128
/* No borrowed JS memory. One backend filesystem handle, never a whole-file read. */
typedef struct { fs_file_t file; uint32_t remaining; fs_result_t fs_error; } speaker_wav_t;
typedef enum { SPEAKER_WAV_OK, SPEAKER_WAV_INVALID, SPEAKER_WAV_UNSUPPORTED, SPEAKER_WAV_IO } speaker_wav_result_t;
speaker_wav_result_t speaker_wav_open(speaker_wav_t *wav, const char *path);
speaker_wav_result_t speaker_wav_read(speaker_wav_t *wav, uint32_t *frames, size_t capacity, double volume, size_t *count);
void speaker_wav_close(speaker_wav_t *wav);
#endif
