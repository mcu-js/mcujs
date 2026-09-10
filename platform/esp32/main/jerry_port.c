/* MCU.js JerryScript port for ESP-IDF. */

#include "jerryscript-port.h"
#include "jerryscript.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if MCUJS_JS_HEAP_EXTERNAL
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#if !defined(CONFIG_SPIRAM) || !defined(CONFIG_SPIRAM_MODE_OCT) || !defined(CONFIG_SPIRAM_BOOT_INIT) || !defined(CONFIG_SPIRAM_USE_MALLOC)
#error "External JS heap requires initialized octal PSRAM"
#endif
#if MCUJS_JS_HEAP_KIB < 1 || MCUJS_JS_HEAP_KIB > 512
#error "JS heap must fit JerryScript's 16-bit compressed-pointer range"
#endif
#endif

static const char *TAG = "jerryscript";

#if MCUJS_JS_HEAP_EXTERNAL
static struct jerry_context_t *s_context;

size_t jerry_port_context_alloc(size_t context_size) {
    /* Pinned JerryScript 3.0.0 aligns the context/heap boundary to 8 bytes.
     * The declared budget is heap bytes, excluding the aligned context. */
    const size_t heap_bytes = (size_t)MCUJS_JS_HEAP_KIB * 1024;
    if (s_context || context_size > SIZE_MAX - 7 - heap_bytes) {
        ESP_LOGE(TAG, "Invalid JS context allocation");
        jerry_port_fatal(JERRY_FATAL_OUT_OF_MEMORY);
    }
    const size_t total = ((context_size + 7) & ~(size_t)7) + heap_bytes;
    /* The arena base must also be 8-byte aligned: ESP-IDF malloc only
     * guarantees 4, which breaks Jerry's 16-bit compressed pointers. */
    s_context = heap_caps_aligned_alloc(8, total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_context) {
        ESP_LOGE(TAG, "Cannot allocate %u KiB JS heap in PSRAM", MCUJS_JS_HEAP_KIB);
        jerry_port_fatal(JERRY_FATAL_OUT_OF_MEMORY);
    }
    ESP_LOGI(TAG, "JS heap: %u KiB in PSRAM", MCUJS_JS_HEAP_KIB);
    return total;
}

struct jerry_context_t *jerry_port_context_get(void) {
    return s_context;
}

void jerry_port_context_free(void) {
    if (s_context) {
        heap_caps_free(s_context);
        s_context = NULL;
    }
}
#endif

void jerry_port_init(void) {
}

void jerry_port_fatal(jerry_fatal_code_t code) {
    ESP_LOGE(TAG, "fatal error: %d", (int)code);
    abort();
}

void jerry_port_log(const char *message_p) {
    if (message_p != NULL) {
        fputs(message_p, stderr);
    }
}

double jerry_port_current_time(void) {
    return (double)esp_timer_get_time() / 1000.0;
}

int32_t jerry_port_local_tza(double unix_ms) {
    (void)unix_ms;
    return 0;
}

void jerry_port_sleep(uint32_t sleep_time) {
    vTaskDelay(pdMS_TO_TICKS(sleep_time));
}

void jerry_port_print_char(char ch) {
    fputc(ch, stdout);
}

void jerry_port_print_buffer(const jerry_char_t *buffer_p, jerry_size_t buffer_size) {
    if (buffer_p != NULL && buffer_size > 0) {
        fwrite(buffer_p, 1, buffer_size, stdout);
    }
}

jerry_char_t *jerry_port_source_read(const char *file_name_p, jerry_size_t *out_size_p) {
    (void)file_name_p;
    *out_size_p = 0;
    return NULL;
}

void jerry_port_source_free(jerry_char_t *buffer_p) {
    free(buffer_p);
}

jerry_char_t *jerry_port_path_normalize(const jerry_char_t *path_p, jerry_size_t path_size) {
    jerry_char_t *result = malloc(path_size + 1);
    if (result != NULL) {
        memcpy(result, path_p, path_size);
        result[path_size] = '\0';
    }
    return result;
}

void jerry_port_path_free(jerry_char_t *path_p) {
    free(path_p);
}

jerry_size_t jerry_port_path_base(const jerry_char_t *path_p) {
    const jerry_char_t *base = path_p;
    for (const jerry_char_t *cursor = path_p; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            base = cursor + 1;
        }
    }
    return (jerry_size_t)(base - path_p);
}
