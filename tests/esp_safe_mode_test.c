/* Real ESP board binding + boot state machine + JerryScript; fake SDK/NVS/FS. */
#include "bindings.h"
#include "boot.h"
#include "engine.h"
#include "fs.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "nvs.h"
#include "driver/gpio.h"
esp_err_t gpio_config(const gpio_config_t *config) { (void)config; return ESP_OK; }
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int64_t now;
static int fail_write, fail_commit, writes;
static int failures;
int64_t esp_timer_get_time(void) { return now; }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return 65536; }
esp_err_t esp_efuse_mac_get_default(uint8_t *mac) { memset(mac, 0, 6); return ESP_OK; }
void esp_restart(void) { assert(0); }
esp_reset_reason_t esp_reset_reason(void) { return 0; }
void esp_reset_reason_set_hint(esp_reset_reason_t hint) { (void)hint; assert(0); }
void vTaskDelay(TickType_t ticks) { (void)ticks; }
esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle) {
    (void)name; (void)mode; *handle = 1; return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *value) {
    (void)handle; *value = strcmp(key, "fs_ready") == 0; return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value) {
    (void)handle; (void)key; (void)value; writes++; return fail_write ? ESP_FAIL : ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle) { (void)handle; return fail_commit ? ESP_FAIL : ESP_OK; }
bool fs_storage_ready(void) { return true; }
bool mcujs_filesystem_partition_is_erased(void) { return false; }
fs_result_t fs_init(void) { return FS_OK; }
fs_result_t fs_format(void) { assert(0); return FS_ERROR; }
fs_result_t fs_exists(const char *path) { (void)path; return FS_OK; }
uint32_t fs_get_total_sectors(void) { return 1; }
uint32_t fs_get_free_space(void) { return 512; }
void usb_cdc_puts(const char *text) { (void)text; }
js_result_t js_engine_exec_file(const char *path) { (void)path; return JS_OK; }
size_t js_engine_get_error(char *buf, size_t size) { (void)buf; (void)size; return 0; }

static void check(const char *label, const char *source) {
    jerry_value_t result = jerry_eval((const jerry_char_t *)source, strlen(source), JERRY_PARSE_NO_OPTS);
    bool ok = !jerry_value_is_exception(result) && jerry_value_is_true(result);
    printf("%s: %s\n", ok ? "PASS" : "FAIL", label);
    failures += !ok;
    jerry_value_free(result);
}
int main(void) {
    jerry_init(JERRY_INIT_EMPTY);
    js_bind_board();
    assert(mcujs_boot_init());
    mcujs_boot_execute_index();
    int before = writes;
    check("active qualification returns EBUSY", "(function(){try{board.safeMode(false)}catch(e){return e instanceof Error && e.name==='ResourceBusyError' && e.code==='EBUSY'}return false})()");
    assert(writes == before);
    check("invalid argument remains uncoded TypeError", "(function(){try{board.safeMode(0)}catch(e){return e instanceof TypeError && !('code' in e)}return false})()");
    now = 5000000;
    mcujs_boot_task();
    check("qualified clear succeeds", "board.safeMode(false) === undefined && board.safeMode() === false");
    check("enable succeeds", "board.safeMode(true) === undefined && board.safeMode() === true");
    fail_write = 1;
    check("NVS write failure returns EIO", "(function(){try{board.safeMode(false)}catch(e){return e instanceof Error && e.name==='Error' && e.code==='EIO' && board.safeMode()===true}return false})()");
    fail_write = 0; fail_commit = 1;
    check("NVS commit failure returns EIO", "(function(){try{board.safeMode(false)}catch(e){return e instanceof Error && e.name==='Error' && e.code==='EIO' && board.safeMode()===true}return false})()");
    fail_commit = 0;
    check("clear recovers after persistence failure", "board.safeMode(false) === undefined && board.safeMode() === false");
    jerry_cleanup();
    return failures ? 1 : 0;
}
