#include "repl.h"
#include "usb_cdc.h"
#include "engine.h"
#include "fs.h"
#include "board.h"
#include "runtime_features.h"
#include "runtime_registry.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static char s_input[512];
static size_t s_input_len;
static size_t s_input_pos;
static char s_output[8192];
static size_t s_output_len;
static char s_executed[256];
static int s_exec_count;
static int s_format_count;
static int s_reset_count;
static bool s_cdc_connected;
static bool s_fs_host_owned;

static void reset_io(void) {
    memset(s_input, 0, sizeof(s_input));
    s_input_len = 0;
    s_input_pos = 0;
    memset(s_output, 0, sizeof(s_output));
    s_output_len = 0;
    memset(s_executed, 0, sizeof(s_executed));
    s_exec_count = 0;
    s_format_count = 0;
    s_reset_count = 0;
    s_cdc_connected = true;
    s_fs_host_owned = false;
}

static void feed_bytes(const char *bytes) {
    size_t len = strlen(bytes);
    assert(len <= sizeof(s_input));
    memcpy(s_input, bytes, len);
    s_input_len = len;
    s_input_pos = 0;
    repl_task();
}

void usb_cdc_task(void) {}
bool usb_cdc_connected(void) { return s_cdc_connected; }
bool usb_cdc_available(void) { return s_input_pos < s_input_len; }
int usb_cdc_getchar(void) {
    return s_input_pos < s_input_len ? (unsigned char)s_input[s_input_pos++] : -1;
}
void usb_cdc_puts(const char *str) {
    size_t len = strlen(str);
    assert(s_output_len + len < sizeof(s_output));
    memcpy(s_output + s_output_len, str, len);
    s_output_len += len;
    s_output[s_output_len] = '\0';
}
void usb_cdc_flush(void) {}
void usb_cdc_reset_usb(uint32_t delay_ms) {
    (void)delay_ms;
    s_reset_count++;
}

js_result_t js_engine_exec(const char *code, size_t code_len,
                           char *result_buf, size_t result_buf_len) {
    assert(code_len < sizeof(s_executed));
    memcpy(s_executed, code, code_len);
    s_executed[code_len] = '\0';
    s_exec_count++;
    snprintf(result_buf, result_buf_len, "'ok'");
    return JS_OK;
}
js_result_t js_engine_exec_file(const char *filename) {
    (void)filename;
    return JS_OK;
}
size_t js_engine_get_error(char *buf, size_t buf_len) {
    if (buf_len > 0) buf[0] = '\0';
    return 0;
}
int js_engine_get_completions(const char *partial,
                              js_completion_callback_t callback,
                              void *user_data) {
    if (strcmp(partial, "boa") == 0) {
        callback("board", user_data);
        return 1;
    }
    return 0;
}
void js_engine_register_global_identifier(const char *name) { (void)name; }
bool js_engine_suggest_method(const char *source, char *suggestion,
                              size_t suggestion_len) {
    (void)source; (void)suggestion; (void)suggestion_len;
    return false;
}

fs_result_t fs_format(void) { s_format_count++; return FS_OK; }
fs_result_t fs_sync(void) { return FS_OK; }
fs_result_t fs_invalidate(void) { return FS_OK; }
void fs_notify_host(void) {}
uint32_t fs_get_free_space(void) { return 4096; }
bool fs_host_owned(void) { return s_fs_host_owned; }
fs_result_t fs_open(fs_file_t *file, const char *path, fs_mode_t mode) {
    (void)file; (void)path; (void)mode;
    return FS_ERROR_NOT_FOUND;
}
fs_result_t fs_close(fs_file_t *file) { (void)file; return FS_OK; }
fs_result_t fs_read(fs_file_t *file, void *buffer, size_t size, size_t *bytes_read) {
    (void)file; (void)buffer; (void)size;
    *bytes_read = 0;
    return FS_OK;
}
fs_result_t fs_write(fs_file_t *file, const void *buffer, size_t size,
                     size_t *bytes_written) {
    (void)file; (void)buffer;
    *bytes_written = size;
    return FS_OK;
}
fs_result_t fs_remove(const char *path) { (void)path; return FS_OK; }
fs_result_t fs_list_dir(const char *path, fs_dir_callback_t callback,
                        void *user_data) {
    (void)path;
    fs_entry_t hidden = {.name = ".Trash-1000", .size = 0, .is_dir = true};
    fs_entry_t visible = {.name = "index.js", .size = 123, .is_dir = false};
    callback(&hidden, user_data);
    callback(&visible, user_data);
    return FS_OK;
}

static const board_info_t s_board = {
    .name = "test_board",
    .chip = "test_chip",
    .flash_size = 1024,
    .ram_size = 512,
    .cpu_freq = 1,
    .led_pin = 1,
    .neopixel_pin = 255,
};
const board_info_t *board_get_info(void) { return &s_board; }
void board_delay_ms(uint32_t ms) { (void)ms; }
bool board_enter_uf2(void) { return true; }

static void test_crlf_is_one_enter(void) {
    reset_io();
    repl_init();
    feed_bytes("\r\n");
    assert(strcmp(s_output, "> \r\n> ") == 0);
    assert(s_exec_count == 0);
}

static void test_tab_completion_with_crlf(void) {
    reset_io();
    repl_init();
    feed_bytes("boa\t.name\r\n");
    assert(s_exec_count == 1);
    assert(strcmp(s_executed, "board.name") == 0);
    assert(strcmp(s_output, "> board.name\r\n'ok'\r\n> ") == 0);
}

static void test_lf_only_client(void) {
    reset_io();
    repl_init();
    feed_bytes("2+2\n");
    assert(s_exec_count == 1);
    assert(strcmp(s_executed, "2+2") == 0);
}

static void test_prompted_command_ignores_paired_lf(void) {
    reset_io();
    repl_init();
    feed_bytes(".format\r\n");
    assert(s_format_count == 1);
    assert(strstr(s_output, "Filesystem formatted successfully.") != NULL);
}

static void test_reset_alias(void) {
    reset_io();
    repl_init();
    feed_bytes(".reset\r\n");
    assert(s_reset_count == 1);
    assert(strstr(s_output, "Resetting...\r\n") != NULL);
}

static void test_uart_input_without_usb(void) {
    reset_io();
    s_cdc_connected = false;
    repl_init();
    feed_bytes("2+2\r");
    assert(s_exec_count == 1);
    assert(strcmp(s_executed, "2+2") == 0);
}

static void test_usb_reconnect_redraws_prompt(void) {
    reset_io();
    s_cdc_connected = false;
    repl_init();
    feed_bytes("");
    assert(strcmp(s_output, "> ") == 0);
    s_output[0] = '\0';
    s_output_len = 0;
    s_cdc_connected = true;
    feed_bytes("");
    assert(strcmp(s_output, "> ") == 0);
}

static void test_info_reports_host_ownership(void) {
    reset_io();
    s_fs_host_owned = true;
    repl_init();
    feed_bytes(".info\r");
    assert(strstr(s_output, "FS Free: host-owned (eject MCUJS)\r\n") != NULL);
    assert(strstr(s_output, "FS Free: 0 B") == NULL);
}

static void test_help_matches_build_features(void) {
    reset_io();
    repl_init();
    feed_bytes(".help\r");
    assert(strstr(s_output, "uniqueId()") != NULL);
    assert(strstr(s_output, " led, ids") == NULL);
    const mcujs_runtime_registry_t *registry = mcujs_runtime_registry();
    for (size_t i = 0; i < registry->builtin_module_count; i++) {
        char expected[192];
        int written = snprintf(expected, sizeof(expected),
                               "  require('%s')\r\n",
                               registry->builtin_modules[i]);
        assert(written > 0 && (size_t)written < sizeof(expected));
        assert(strstr(s_output, expected) != NULL);
    }
    static const char *const known_modules[] = {
        "board", "fs", "process", "gpio", "pwm", "i2c", "spi", "adc",
        "neopixel", "image", "keyboard", "mouse", "mcujs:module",
        "node:module",
    };
    for (size_t i = 0; i < sizeof(known_modules) / sizeof(known_modules[0]); i++) {
        char marker[192];
        int written = snprintf(marker, sizeof(marker), "  require('%s')\r\n",
                               known_modules[i]);
        assert(written > 0 && (size_t)written < sizeof(marker));
        assert((strstr(s_output, marker) != NULL) ==
               mcujs_runtime_has_module(known_modules[i]));
    }
#if MCUJS_REGISTRY_SAFE_MODE && MCUJS_REGISTRY_STORAGE_READY
    assert(strstr(s_output, "safeMode(), storageReady()") != NULL);
#elif MCUJS_REGISTRY_SAFE_MODE
    assert(strstr(s_output, "board recovery method: safeMode()") != NULL);
    assert(strstr(s_output, "storageReady()") == NULL);
#elif MCUJS_REGISTRY_STORAGE_READY
    assert(strstr(s_output, "safeMode()") == NULL);
    assert(strstr(s_output, "board storage method: storageReady()") != NULL);
#else
    assert(strstr(s_output, "safeMode(), storageReady()") == NULL);
#endif
}

static void test_capability_discovery_uses_registry(void) {
    reset_io();
    repl_init();
    feed_bytes(".capabilities\r");
    assert(strstr(s_output, "Board: ") != NULL);
    const mcujs_runtime_registry_t *registry = mcujs_runtime_registry();
    char expected_modules[1024] = "Modules:";
    size_t expected_length = strlen(expected_modules);
    for (size_t i = 0; i < registry->builtin_module_count; i++) {
        int written = snprintf(expected_modules + expected_length,
                               sizeof(expected_modules) - expected_length,
                               " %s", registry->builtin_modules[i]);
        assert(written > 0 && (size_t)written <
               sizeof(expected_modules) - expected_length);
        expected_length += (size_t)written;
    }
    assert(strstr(s_output, expected_modules) != NULL);

    reset_io();
    repl_init();
    assert(registry->capability_count > 0);
    const mcujs_runtime_capability_t *capability = &registry->capabilities[0];
    char command[96];
    int command_length = snprintf(command, sizeof(command),
                                  ".capabilities %s\r", capability->name);
    assert(command_length > 0 && (size_t)command_length < sizeof(command));
    feed_bytes(command);
    assert(s_exec_count == 0);
    assert(strstr(s_output, capability->json) != NULL);

    reset_io();
    repl_init();
    feed_bytes(".capabilities spi');board.reset();//\r");
    assert(s_exec_count == 0);
    assert(s_executed[0] == '\0');
    assert(strstr(s_output, "Invalid capability name.\r\n") != NULL);

    reset_io();
    repl_init();
    feed_bytes(".capabilities aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\r");
    assert(s_exec_count == 0);
    assert(s_executed[0] == '\0');
    assert(strstr(s_output, "Invalid capability name.\r\n") != NULL);
}

static void test_largest_capability_is_complete_and_crlf_framed(void) {
#if defined(MCUJS_BOARD_SEEED_XIAO_ESP32S3)
    const mcujs_runtime_capability_t *adc = mcujs_runtime_find_capability("adc");
    assert(adc != NULL);
    assert(strlen(adc->json) == 552);

    reset_io();
    repl_init();
    feed_bytes(".capabilities adc\r");

    char expected[8192];
    int written = snprintf(expected, sizeof(expected),
                           "> .capabilities adc\r\n%s\r\n> ", adc->json);
    assert(written > 0 && (size_t)written < sizeof(expected));
    assert(strcmp(s_output, expected) == 0);
    assert(s_exec_count == 0);
#endif
}

static void test_ls_hides_dot_entries(void) {
    reset_io();
    repl_init();
    feed_bytes(".ls\r");
    assert(strstr(s_output, "index.js") != NULL);
    assert(strstr(s_output, ".Trash-1000") == NULL);
}

int main(void) {
    test_crlf_is_one_enter();
    test_tab_completion_with_crlf();
    test_lf_only_client();
    test_prompted_command_ignores_paired_lf();
    test_reset_alias();
    test_uart_input_without_usb();
    test_usb_reconnect_redraws_prompt();
    test_info_reports_host_ownership();
    test_help_matches_build_features();
    test_capability_discovery_uses_registry();
    test_largest_capability_is_complete_and_crlf_framed();
    test_ls_hides_dot_entries();
    puts("REPL input tests passed");
    return 0;
}
