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

static const char *TAG = "jerryscript";

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
