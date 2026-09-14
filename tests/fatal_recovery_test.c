/* Real port + boot gate + JerryScript; SDK reset/retained RAM/NVS/FS are fakes.
 * A reset exits the process; the next invocation restores only retained state.
 * Deliberate fatal injection, never heap flooding or live hardware access. */
#include "jerryscript.h"
#include "jerryscript-port.h"
#include "fatal_recovery.h"
#include "boot.h"
#include "engine.h"
#include "fs.h"
#include "test_reset.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef MCUJS_PLATFORM_ESP32
#include "nvs.h"
extern unsigned char __start_test_rtc[], __stop_test_rtc[];
#endif

watchdog_hw_t test_watchdog;
static uint8_t nvs_values[4] = {0, 0, 0, 1};
static int state_fd, startup_calls, callbacks;
static bool warm_reset, trigger_early, has_app = true;
static int64_t now;

static void save_state(void) {
    assert(lseek(state_fd, 0, SEEK_SET) == 0);
    assert(write(state_fd, &test_watchdog, sizeof(test_watchdog)) == sizeof(test_watchdog));
    assert(write(state_fd, nvs_values, sizeof(nvs_values)) == sizeof(nvs_values));
#ifdef MCUJS_PLATFORM_ESP32
    size_t size = (size_t)(__stop_test_rtc - __start_test_rtc);
    assert(write(state_fd, __start_test_rtc, size) == (ssize_t)size);
#endif
}
static void requested_reset(void) {
    assert(callbacks == 0);
    save_state();
    puts("SDK_RESET_REQUESTED"); fflush(stdout);
    _Exit(77); /* Never unwind or clean up the failed JS context. */
}
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms) {
    assert(pc == 0 && sp == 0 && delay_ms <= 100);
    requested_reset();
}
bool watchdog_caused_reboot(void) { return warm_reset; }
void tight_loop_contents(void) { puts("HALTED_WITHOUT_RESET"); exit(99); }
void esp_restart(void) { requested_reset(); }
int esp_reset_reason(void) { return warm_reset ? 4 : 0; }
void __wrap_abort(void) { puts("ABORTED_WITHOUT_RECOVERY_MARKER"); exit(99); }
uint64_t get_absolute_time(void) { return (uint64_t)now; }
uint64_t to_ms_since_boot(uint64_t t) { return t / 1000; }
int64_t esp_timer_get_time(void) { return now; }

/* Fake SDK registers for the real BOOTSEL sampler: button is never held. */
test_qspi_t test_qspi;
test_sio_t test_sio = { .gpio_hi_in = UINT32_MAX };
uint32_t save_and_disable_interrupts(void) { return 42; }
void restore_interrupts(uint32_t state) { assert(state == 42); }
void hw_write_masked(volatile uint32_t *reg, uint32_t value, uint32_t mask) {
    *reg = (*reg & ~mask) | (value & mask);
}
bool usb_cdc_connected(void) { return true; }
void usb_cdc_puts(const char *s) { fputs(s, stdout); }
fs_result_t fs_init(void) { return FS_OK; }
fs_result_t fs_format(void) { assert(!"recovery must not format storage"); return FS_ERROR; }
fs_result_t fs_exists(const char *path) {
    assert(strcmp(path, "/app/index.js") == 0);
    return has_app ? FS_OK : FS_ERROR_NOT_FOUND;
}
uint32_t fs_get_total_sectors(void) { return 100; }
uint32_t fs_get_free_space(void) { return 4096; }
bool mcujs_filesystem_partition_is_erased(void) { return false; }
size_t js_engine_get_error(char *buf, size_t size) {
    if (size) buf[0] = 0;
    return 0;
}

#ifdef MCUJS_PLATFORM_ESP32
static int key_index(const char *key) {
    const char *keys[] = {"safe", "pending", "failures", "fs_ready"};
    for (int i=0; i<4; i++) if (!strcmp(key, keys[i])) return i;
    assert(!"unexpected NVS key"); return -1;
}
esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle) {
    assert(!strcmp(name, "mcujs_boot")); (void)mode; *handle=1; return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *value) {
    (void)handle; *value=nvs_values[key_index(key)]; return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value) {
    (void)handle; nvs_values[key_index(key)]=value; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle) { (void)handle; return ESP_OK; }
#endif

static jerry_value_t fatal_now(const jerry_call_info_t *info, const jerry_value_t args[], const jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    jerry_port_fatal(JERRY_FATAL_OUT_OF_MEMORY);
    _Exit(98);
}
static jerry_value_t after_fatal(const jerry_call_info_t *info, const jerry_value_t args[], const jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    callbacks++; puts("UNEXPECTED_CALLBACK_AFTER_FATAL");
    return jerry_undefined();
}
static jerry_value_t evaluate(const char *text) {
    return jerry_eval((const jerry_char_t *)text, strlen(text), JERRY_PARSE_NO_OPTS);
}
static void run(const char *text) {
    jerry_value_t value=evaluate(text);
    assert(!jerry_value_is_exception(value)); jerry_value_free(value);
}
static void bind(const char *name, jerry_external_handler_t handler) {
    jerry_value_t global=jerry_current_realm(), key=jerry_string_sz(name), fn=jerry_function_external(handler);
    jerry_value_t result=jerry_object_set(global,key,fn);
    assert(jerry_value_is_true(result));
    jerry_value_free(result); jerry_value_free(fn); jerry_value_free(key); jerry_value_free(global);
}
js_result_t js_engine_exec_file(const char *path) {
    assert(!strcmp(path, "/app/index.js")); startup_calls++;
    run("var oldApp = true;");
    if (trigger_early) run("try { fatalNow(); } finally { afterFatal(); }");
    return JS_OK;
}

int main(int argc, char **argv) {
    assert(argc == 3);
    const char *mode=argv[1];
    state_fd=open(argv[2], O_RDWR|O_CREAT, 0600); assert(state_fd >= 0);
    ssize_t n=read(state_fd,&test_watchdog,sizeof(test_watchdog));
    if (n) {
        assert(n == sizeof(test_watchdog)); warm_reset=true;
        assert(read(state_fd,nvs_values,sizeof(nvs_values)) == sizeof(nvs_values));
#ifdef MCUJS_PLATFORM_ESP32
        size_t size=(size_t)(__stop_test_rtc-__start_test_rtc);
        assert(read(state_fd,__start_test_rtc,size) == (ssize_t)size);
#endif
    }
    if (!strcmp(mode,"cold")) warm_reset=false;
    trigger_early=!strcmp(mode,"fatal-early");
    mcujs_fatal_recovery_init();
    bool recovering=!strcmp(mode,"recover");
    assert(mcujs_fatal_recovery_code() == (recovering ? JERRY_FATAL_OUT_OF_MEMORY : -1));
#ifdef MCUJS_PLATFORM_ESP32
    assert(mcujs_boot_init());
#endif
    jerry_init(JERRY_INIT_EMPTY);
    bind("fatalNow",fatal_now); bind("afterFatal",after_fatal);
#ifdef MCUJS_PLATFORM_RP2
    boot_run_index_js();
#else
    mcujs_boot_execute_index();
#endif
    if (recovering) {
        assert(startup_calls == 0 && "failed startup app must be skipped");
        jerry_value_t value=evaluate("typeof oldApp === 'undefined'");
        assert(jerry_value_is_true(value)); jerry_value_free(value);
        run("var result; Promise.resolve(42).then(function(v){result=v;});");
        value=jerry_run_jobs(); assert(!jerry_value_is_exception(value)); jerry_value_free(value);
        value=evaluate("result === 42"); assert(jerry_value_is_true(value)); jerry_value_free(value);
        puts("STARTUP_SKIPPED_FRESH_VM_AND_PROMISE_PASS");
#ifdef MCUJS_PLATFORM_ESP32
        /* Explicit user re-enable clears the pre-existing startup failure latch. */
        assert(mcujs_boot_set_safe_mode(false) == MCUJS_BOOT_SAFE_MODE_OK);
#endif
    } else {
        assert(startup_calls == 1);
        now=6000000;
#ifdef MCUJS_PLATFORM_ESP32
        mcujs_boot_task();
#endif
        if (!strcmp(mode,"fatal-late")) {
            run("Promise.resolve().then(fatalNow); Promise.resolve().then(afterFatal);");
            jerry_value_t value=jerry_run_jobs(); jerry_value_free(value);
            assert(!"fatal Promise job must not return");
        }
        puts("NORMAL_STARTUP_PASS");
    }
    assert(callbacks == 0);
    jerry_cleanup(); save_state(); close(state_fd);
    return 0;
}
