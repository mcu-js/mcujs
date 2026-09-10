/* Native button integration, 64 KiB Jerry, no hardware or SDK installation.
 * Real production: devices/button/events factories, board registry, board.c and
 * timers.c, for Pico and XIAO. Only SDK clock/electrical input are simulated.
 * require below is a small cached factory router, NOT the production loader.
 * Failure tests additionally decorate the REAL board.buttonPressed function
 * with an EIO injection switch: neither hardware sampler exposes poll failures.
 * Successful reads still execute board.c and the fake SDK input boundary.
 * VM recreation uses real timer cleanup and Jerry teardown, not engine.c.
 * --board-smoke intentionally excludes button coverage; useful on a baseline
 * before the production button factory/registry has been integrated.
 */
#include "bindings.h"
#include "jerryscript.h"
#include "runtime_registry.h"
#include "runtime_validation_backend_stubs.h"
#include "validation.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool js_timers_process(void);
extern void js_timers_cleanup(void);
#ifndef MCUJS_BUTTONS_BOARD_SMOKE
extern jerry_value_t js_create_button_module(void);
#endif
extern jerry_value_t js_create_devices_module(void);
extern jerry_value_t js_create_events_module(void);

static uint64_t now_ms;
static bool physical_pressed;
static unsigned samples;
static const char *phase = "startup";
bool fs_storage_ready(void) { return true; }

#if defined(MCUJS_PLATFORM_RP2)
#include "pico/stdlib.h"
#define TARGET "Pico"
absolute_time_t get_absolute_time(void) { return now_ms; }
/* Pico's real SDK returns a wrapping uint32 value; its old test declaration
 * returns uint64, so deliberately reproduce the real low-32-bit behavior. */
uint64_t to_ms_since_boot(absolute_time_t time) { return (uint32_t)time; }
bool boot_button_pressed(void) { samples++; return physical_pressed; }
void usb_cdc_reset_usb(uint32_t delay_ms) {
    (void)delay_ms; assert(!"button tests must not reset USB or the board");
}
bool board_enter_uf2(void) {
    assert(!"button tests must not enter UF2"); return false;
}
#else
#include "board_config.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/task.h"
#define TARGET "XIAO ESP32-S3"
int64_t esp_timer_get_time(void) { return (int64_t)now_ms * 1000; }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return 65536; }
esp_err_t esp_efuse_mac_get_default(uint8_t *mac) { memset(mac, 0, 6); return ESP_OK; }
void esp_restart(void) { assert(!"button tests must not reset the board"); }
esp_reset_reason_t esp_reset_reason(void) { return 0; }
void esp_reset_reason_set_hint(esp_reset_reason_t hint) {
    (void)hint; assert(!"button tests must not request UF2");
}
void vTaskDelay(TickType_t ticks) { (void)ticks; assert(!"button sampling must not block"); }
bool mcujs_boot_safe_mode(void) { return false; }
bool mcujs_boot_set_safe_mode(bool enabled) { (void)enabled; return true; }
esp_err_t gpio_config(const gpio_config_t *config) {
    assert(MCUJS_BUTTON_PIN == 0); /* Only qualified BOOT, never RESET. */
    assert(config->pin_bit_mask == (UINT64_C(1) << MCUJS_BUTTON_PIN));
    assert(config->mode == GPIO_MODE_INPUT);
    assert(config->pull_up_en == GPIO_PULLUP_ENABLE);
    assert(config->pull_down_en == GPIO_PULLDOWN_DISABLE);
    assert(config->intr_type == GPIO_INTR_DISABLE);
    return ESP_OK;
}
extern int __real_gpio_get_level(gpio_num_t pin);
int __wrap_gpio_get_level(gpio_num_t pin) {
    if (pin != MCUJS_BUTTON_PIN) return __real_gpio_get_level(pin);
    samples++;
    return physical_pressed ? 0 : 1; /* Electrical level, board.c decodes polarity. */
}
#endif

static void check(jerry_value_t value) {
    if (!jerry_value_is_exception(value)) return;
    jerry_value_t error = jerry_exception_value(value, false);
    jerry_value_t text = jerry_value_to_string(error);
    char buffer[512] = {0};
    if (jerry_value_is_string(text))
        jerry_string_to_buffer(text, JERRY_ENCODING_UTF8,
                              (jerry_char_t *)buffer, sizeof(buffer) - 1);
    fprintf(stderr, "%s / %s: %s\n", TARGET, phase, buffer);
    jerry_value_free(text); jerry_value_free(error); jerry_value_free(value);
    exit(1);
}
static void evaluate(const char *source) {
    jerry_value_t value = jerry_eval((const jerry_char_t *)source,
                                    strlen(source), JERRY_PARSE_NO_OPTS);
    check(value); jerry_value_free(value);
}
static inline void publish(const char *name, jerry_value_t value) {
    check(value);
    jerry_value_t global = jerry_current_realm();
    jerry_value_t result = jerry_object_set_sz(global, name, value);
    check(result); assert(jerry_value_is_true(result));
    jerry_value_free(result); jerry_value_free(global); jerry_value_free(value);
}
static size_t retained_heap(void) {
    jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);
    jerry_heap_stats_t stats;
    assert(jerry_heap_stats(&stats) && "External Jerry build must enable memory stats");
    assert(stats.size >= 63u * 1024u && stats.size <= 64u * 1024u &&
           "External Jerry build must use --mem-heap=64");
    return stats.allocated_bytes;
}
static void advance(unsigned ms) { now_ms += ms; (void)js_timers_process(); }
static void ticks(unsigned count) {
    assert(count <= 1000);
    for (unsigned i = 0; i < count; i++) advance(10);
}

#ifndef MCUJS_BUTTONS_BOARD_SMOKE
static bool fail_sample;
static jerry_value_t native_sampler;
static jerry_value_t sample_with_fault(const jerry_call_info_t *info,
                                       const jerry_value_t args[], jerry_length_t argc) {
    if (fail_sample) {
        const mcujs_error_details_t details = {.resource = "button"};
        return mcujs_throw_operational_error(MCUJS_ERROR_IO,
                                             "Injected sampler EIO", &details);
    }
    return jerry_call(native_sampler, info->this_value, args, argc);
}
static struct {
    const char *name;
    jerry_value_t (*factory)(void);
    jerry_value_t value;
    bool loaded;
} modules[] = {
    {"devices", js_create_devices_module, 0, false},
    {"mcujs:button", js_create_button_module, 0, false},
    {"events", js_create_events_module, 0, false},
    {"board", js_create_board_module, 0, false},
};
static jerry_value_t require_module(const jerry_call_info_t *info,
                                    const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    assert(argc == 1 && jerry_value_is_string(args[0]));
    char name[32] = {0};
    jerry_size_t n = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    assert(n < sizeof(name));
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)name, n);
    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        if (strcmp(name, modules[i].name)) continue;
        if (!modules[i].loaded) {
            jerry_value_t value = modules[i].factory();
            if (jerry_value_is_exception(value)) return value;
            modules[i].value = value; modules[i].loaded = true;
        }
        return jerry_value_copy(modules[i].value);
    }
    fprintf(stderr, "Unexpected require: %s\n", name);
    return jerry_throw_sz(JERRY_ERROR_COMMON, "Unsupported native-test require");
}
#endif

static void setup_vm(void) {
    phase = "VM setup";
    physical_pressed = false;
    now_ms = 1000;
    samples = 0;
    assert(!js_timers_process()); /* No stale refs from a destroyed VM. */
    jerry_init(JERRY_INIT_EMPTY);
    (void)retained_heap();
    js_bind_board(); /* Real board.c for the selected target. */
    js_bind_timers();
    evaluate("function eq(a,b,m){if(a!==b)throw Error(m+': '+a+' !== '+b);}"
             "function code(f,w){var got='NO_ERROR';try{f();}catch(e){got=e.code;}eq(got,w,'error code');}"
             "function typeError(f){var ok=false;try{f();}catch(e){ok=e instanceof TypeError;}eq(ok,true,'TypeError');}");
#ifndef MCUJS_BUTTONS_BOARD_SMOKE
    fail_sample = false;
    jerry_value_t global = jerry_current_realm();
    jerry_value_t board = jerry_object_get_sz(global, "board");
    check(board);
    native_sampler = jerry_object_get_sz(board, "buttonPressed");
    check(native_sampler); assert(jerry_value_is_function(native_sampler));
    jerry_value_t wrapper = jerry_function_external(sample_with_fault);
    jerry_value_t result = jerry_object_set_sz(board, "buttonPressed", wrapper);
    check(result); assert(jerry_value_is_true(result));
    jerry_value_free(result); jerry_value_free(wrapper);
    jerry_value_free(board); jerry_value_free(global);
    publish("require", jerry_function_external(require_module));
#endif
}

static void board_timer_smoke(void) {
    phase = "real board / timer smoke (not button factory coverage)";
    evaluate("eq(board.buttonPressed(),false,'real released input');"
             "typeError(function(){board.buttonPressed(true);});"
             "typeError(function(){board.buttonPressed(undefined);});");
    assert(samples == 1);
    physical_pressed = true;
    evaluate("eq(board.buttonPressed(),true,'real pressed input');");
    physical_pressed = false;
    evaluate("eq(board.buttonPressed(),false,'real released input again');"
             "var fired=0,id=setInterval(function(){fired++;},10);");
    assert(samples == 3);
    advance(9); evaluate("eq(fired,0,'timer not early');");
    advance(1); evaluate("eq(fired,1,'timer due');clearInterval(id);");
    ticks(2); assert(!js_timers_process());
    evaluate("eq(fired,1,'timer cleared');");
}

#ifndef MCUJS_BUTTONS_BOARD_SMOKE
static void discovery(void) {
    phase = "descriptor discovery / lazy factory";
    unsigned before = samples;
    assert(mcujs_runtime_has_module("devices"));
    evaluate("var devices=require('devices'),desc=devices.button,h=null,old=null;"
             "eq(typeof desc,'object','qualified button descriptor');"
             "eq(require('devices'),devices,'cached devices');"
             "eq(Object.isFrozen(devices),true,'frozen devices');"
             "eq(desc,devices.button,'stable descriptor');"
             "eq(Object.isFrozen(desc),true,'frozen descriptor');"
             "eq(Object.isFrozen(desc.capabilities),true,'frozen capabilities');"
             "eq(desc.capabilities.interface,'button-events','interface');"
             "eq(desc.capabilities.readOnly,true,'read only');"
             "eq(desc.capabilities.maxOpenHandles,1,'exclusive');"
             "eq(desc.capabilities.pollIntervalMs,10,'poll period');"
             "eq(desc.capabilities.debounceMs,30,'debounce period');"
             "eq(desc.capabilities.interface,board.capability('devices').button.interface,'real registry');"
             "eq(desc.state,'idle','initial descriptor');"
             "typeError(function(){desc.open(undefined);});"
             "typeError(function(){desc.open({});});");
    assert(samples == before);
    assert(!js_timers_process()); /* Discovery and rejected opens allocate no timer. */
    evaluate("var E=require('events'),count=0,last=null,valid=true;"
             "function record(e){count++;last=e.type;valid=valid&&e.target===h&&e.currentTarget===h&&this===h&&e instanceof E.Event;}"
             "function listen(){h.addEventListener('press',record);h.addEventListener('release',record);}");
}
static void edges_and_lifecycle(void) {
    phase = "debounce / bounce / hold / close / reopen";
    unsigned before = samples;
    evaluate("h=desc.open();eq(h instanceof E.EventTarget,true,'real EventTarget');"
             "eq(h.state,'open','open handle');eq(desc.state,'busy','busy descriptor');"
             "eq(h.pressed,false,'initial released state');listen();"
             "code(function(){desc.open();},'EBUSY');");
    assert(samples == before + 1);
    physical_pressed = true;
    advance(9); assert(samples == before + 1);
    evaluate("eq(count,0,'no early polling');eq(h.pressed,false,'raw input not exposed');");
    advance(1); /* Candidate starts at this sample, not the electrical edge. */
    ticks(2); evaluate("eq(count,0,'candidate only 20ms old');");
    physical_pressed = false; ticks(1); /* Bounce cancels candidate. */
    physical_pressed = true; ticks(3);
    evaluate("eq(count,0,'bounce restarts stability window');eq(h.pressed,false,'still debouncing');");
    ticks(1);
    evaluate("eq(count,1,'one press after 30 stable ms');eq(last,'press','press type');"
             "eq(h.pressed,true,'debounced press');eq(valid,true,'event identity and receiver');");
    ticks(100);
    evaluate("eq(count,1,'held input does not repeat');");
    physical_pressed = false; ticks(3);
    evaluate("eq(count,1,'release also debounced');eq(h.pressed,true,'release candidate hidden');");
    ticks(1);
    evaluate("eq(count,2,'one release');eq(last,'release','release type');"
             "eq(h.pressed,false,'debounced release');eq(valid,true,'release identity');"
             "h.close();h.close();eq(h.state,'closed','idempotent close');eq(desc.state,'idle','lease released');"
             "code(function(){return h.pressed;},'ENXIO');"
             "E.EventTarget.prototype.dispatchEvent.call(h,new E.Event('press'));"
             "eq(count,2,'close removes existing listeners');");
    before = samples; physical_pressed = true; ticks(10);
    assert(samples == before && !js_timers_process());
    evaluate("eq(count,2,'closed timer and no fake release');"
             "old=h;h=desc.open();old.close();eq(h.state,'open','old close leaves new owner');"
             "eq(desc.state,'busy','new lease remains busy');code(function(){desc.open();},'EBUSY');"
             "eq(h.pressed,true,'initial held input sampled immediately');listen();");
    ticks(10);
    evaluate("eq(count,2,'no synthetic initial press');"
             "h.close();eq(count,2,'closing pressed handle emits no release');h=null;old=null;");
    assert(!js_timers_process());
}
static void cancellation_and_callback_close(void) {
    phase = "AbortSignal / remove / once / close inside dispatch";
    physical_pressed = false;
    evaluate("h=desc.open();var ac=new E.AbortController(),cancelled=0,once=0,removed=0;"
             "function onCancel(){cancelled++;}function onRemove(){removed++;}"
             "h.addEventListener('press',onCancel,{signal:ac.signal});ac.abort();"
             "h.addEventListener('press',function(){once++;},{once:true});"
             "h.addEventListener('press',onRemove);h.removeEventListener('press',onRemove);");
    physical_pressed = true; ticks(4);
    physical_pressed = false; ticks(4);
    physical_pressed = true; ticks(4);
    evaluate("eq(cancelled,0,'abort removes listener');eq(once,1,'once listener');"
             "eq(removed,0,'explicit removal');h.close();h=null;ac=null;");
    physical_pressed = false;
    evaluate("var closedInCallback=0,afterClose=0;h=desc.open();"
             "h.addEventListener('press',function(){closedInCallback++;h.close();});"
             "h.addEventListener('press',function(){afterClose++;});");
    physical_pressed = true; ticks(4);
    evaluate("eq(closedInCallback,1,'callback ran');eq(afterClose,0,'close cancels dispatch snapshot');"
             "eq(h.state,'closed','closed within callback');eq(desc.state,'idle','callback releases owner');h=null;");
    unsigned before = samples; ticks(3);
    assert(samples == before && !js_timers_process());
}
static void failure_and_recovery(void) {
    phase = "injected sampler EIO / unavailable / recovery";
    fail_sample = true;
    evaluate("code(function(){desc.open();},'EIO');eq(desc.state,'unavailable','initial read failure');");
    assert(!js_timers_process());
    fail_sample = false;
    evaluate("h=desc.open();eq(desc.state,'busy','retry recovers');"
             "var errors=0,errorCode=null,errorIdentity=false,ac=new E.AbortController();"
             "h.addEventListener('error',function(e){errors++;errorCode=e.error&&e.error.code;"
             "errorIdentity=e.target===h&&e.currentTarget===h&&h.state==='error'&&desc.state==='unavailable';});"
             "h.addEventListener('press',function(){}, {signal:ac.signal});");
    fail_sample = true; ticks(1);
    evaluate("eq(errors,1,'one error event');eq(errorCode,'EIO','event.error');"
             "eq(errorIdentity,true,'error state before dispatch');eq(h.state,'error','failed handle');"
             "eq(desc.state,'unavailable','poll failed');code(function(){return h.pressed;},'ENXIO');"
             "E.EventTarget.prototype.dispatchEvent.call(h,new E.Event('error'));"
             "eq(errors,1,'error cleanup removes listeners');"
             "var capacity=new E.EventTarget();for(var i=0;i<E.limits.subscriptions;i++)"
             "capacity.addEventListener('slot'+i,function(){},{signal:ac.signal});"
             "ac.abort();capacity=null;ac=null;old=h;h.close();h.close();");
    unsigned before = samples; ticks(3);
    assert(samples == before && !js_timers_process());
    fail_sample = false;
    evaluate("h=desc.open();old.close();eq(h.state,'open','failed old close cannot release new owner');"
             "eq(desc.state,'busy','recovered new owner');code(function(){desc.open();},'EBUSY');"
             "h.close();h=null;old=null;");
    phase = "real timer exhaustion / recovery";
    evaluate("var ids=[];for(var i=0;i<16;i++)ids.push(setInterval(function(){},1000));"
             "code(function(){desc.open();},'ERR_RESOURCE_EXHAUSTED');"
             "ids.forEach(clearInterval);ids=null;h=desc.open();eq(h.state,'open','timer exhaustion retry');"
             "h.close();h=null;");
    assert(!js_timers_process());
}
static void clock_wrap(void) {
    phase = "32-bit millisecond wrap";
    assert(!js_timers_process());
    now_ms = UINT32_MAX - UINT64_C(25);
    physical_pressed = false;
    evaluate("count=0;valid=true;h=desc.open();listen();");
    physical_pressed = true; ticks(3);
    evaluate("eq(count,0,'wrapped candidate only 20ms old');eq(h.pressed,false,'wrapped candidate hidden');");
    ticks(1);
    evaluate("eq(count,1,'press crosses clock wrap');eq(h.pressed,true,'wrapped state');eq(valid,true,'wrapped event');");
    physical_pressed = false; ticks(4);
    evaluate("eq(count,2,'release after wrap');eq(last,'release','postwrap edge');h.close();h=null;");
    assert(!js_timers_process());
}
static void bounded_cycles(unsigned vm) {
    phase = "bounded close/reopen GC / external signal cleanup";
    physical_pressed = false;
    evaluate("var retainedController=new E.AbortController();");
    /* Reuse one external signal; leaked subscriptions hit its bound on the
     * second cycle. No reflection into event/button private symbols is needed. */
    const char *cycle =
        "(function(){var b=desc.open();for(var k=0;k<E.limits.subscriptions;k++)"
        "b.addEventListener('cycle'+k,function(){},{signal:retainedController.signal});"
        "b.close();b.close();eq(b.state,'closed','cycle close');eq(desc.state,'idle','cycle lease');})();";
    for (unsigned i = 0; i < 4; i++) { evaluate(cycle); (void)retained_heap(); }
    size_t baseline = retained_heap(), retained = baseline;
    for (unsigned i = 0; i < 64; i++) {
        unsigned before = samples;
        evaluate(cycle);
        assert(samples == before + 1);
        ticks(1); assert(samples == before + 1 && !js_timers_process());
        retained = retained_heap();
        assert(retained <= baseline + 1024u && "closed handles/listeners must be collectable");
    }
    printf("%s VM %u: 64 close/reopen cycles, retained heap %zu -> %zu bytes (<=1024 slack)\n",
           TARGET, vm, baseline, retained);
    evaluate("retainedController.abort();retainedController=null;"
             "h=desc.open();h.addEventListener('press',function(){throw Error('stale VM callback');});"
             "eq(desc.state,'busy','leave live owner at teardown');");
}
#endif

static void cleanup_vm(void) {
    phase = "VM teardown with live timer / factory roots";
    js_timers_cleanup(); /* Real engine teardown hook, before Jerry cleanup. */
#ifndef MCUJS_BUTTONS_BOARD_SMOKE
    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        if (modules[i].loaded) jerry_value_free(modules[i].value);
        modules[i].loaded = false; modules[i].value = 0;
    }
    jerry_value_free(native_sampler);
#endif
    jerry_cleanup();
    now_ms += 100;
    assert(!js_timers_process());
}
int main(void) {
    mcujs_test_reset_backend();
    for (unsigned vm = 1; vm <= 2; vm++) {
        setup_vm();
        board_timer_smoke();
#ifndef MCUJS_BUTTONS_BOARD_SMOKE
        discovery();
        edges_and_lifecycle();
        cancellation_and_callback_close();
        failure_and_recovery();
        clock_wrap();
        bounded_cycles(vm);
#endif
        cleanup_vm();
    }
#ifdef MCUJS_BUTTONS_BOARD_SMOKE
    printf("PASS %s: real board/timer fixture, 64 KiB Jerry, two VMs; NO button-factory coverage\n", TARGET);
#else
    printf("PASS %s: real devices/button/events factories + real board/timers; "
           "debounce, lifecycle, cancellation, wrap, injected EIO, exhaustion, GC, two VMs\n", TARGET);
#endif
    return 0;
}
