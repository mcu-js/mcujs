#ifndef MCUJS_FS_PATH_H
#define MCUJS_FS_PATH_H

#include <string.h>

/* Canonical logical paths only. Never pass private FatFs/VFS paths here.
 * Relative paths use a logical directory base (NULL defaults to /app).
 * Escaping the app mount is rejected at the offending segment, not after
 * cancellation. Output is unchanged on error and may alias either input. */
static inline fs_result_t fs_normalize_path(const char *path, const char *base,
                                           char *out, size_t out_size) {
    if (!path || !out || !out_size || !*path) return FS_ERROR_INVALID;
    size_t raw_len = 0;
    while (path[raw_len]) {
        if (raw_len >= FS_PATH_MAX - 1 || path[raw_len] == ':' ||
            path[raw_len] == '\\') return FS_ERROR_INVALID;
        raw_len++;
    }
    char normalized[FS_PATH_MAX] = "/";
    size_t length = 1;
    if (*path != '/') {
        const char *directory = base ? base : FS_APP_ROOT;
        if (*directory != '/') return FS_ERROR_INVALID;
        fs_result_t result = fs_normalize_path(directory, NULL, normalized,
                                               sizeof(normalized));
        if (result != FS_OK) return result;
        length = strlen(normalized);
    }
    const char *cursor = path;
    while (*cursor) {
        while (*cursor == '/') cursor++;
        const char *segment = cursor;
        while (*cursor && *cursor != '/') cursor++;
        size_t count = (size_t)(cursor - segment);
        if (!count || (count == 1 && segment[0] == '.')) continue;
        if (count == 2 && segment[0] == '.' && segment[1] == '.') {
            /* Never leave the first mount component, even transiently. */
            if (!strchr(normalized + 1, '/')) return FS_ERROR_INVALID;
            while (normalized[--length] != '/') {}
            normalized[length] = '\0';
            continue;
        }
        if (length == 1 && (count != 3 || memcmp(segment, "app", 3))) {
#if MCUJS_HAS_SD
            if (count != 2 || memcmp(segment, "sd", 2))
#endif
                return FS_ERROR_NOT_FOUND;
        }
        size_t slash = length > 1 ? 1 : 0;
        if (length + slash + count >= sizeof(normalized)) return FS_ERROR_INVALID;
        if (slash) normalized[length++] = '/';
        memcpy(normalized + length, segment, count);
        length += count;
        normalized[length] = '\0';
    }
    if (length >= out_size) return FS_ERROR_INVALID;
    memcpy(out, normalized, length + 1);
    return FS_OK;
}
#endif
