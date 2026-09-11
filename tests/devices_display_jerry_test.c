/* Configured display integration: real factories, Canvas, registry and drivers.
 * Boundaries: IDF fakes from the existing adapter fixtures; safeMode/storageReady
 * callbacks; a minimal cached require (NOT the production module loader).
 * Only canvas_native.c's libc calloc/free are redirected, for deterministic
 * allocation failure and native-handle accounting. Jerry's allocator is intact.
 * Run against an externally built 64 KiB Jerry heap with memory stats enabled.
 */
#include "jerryscript.h"
#include "bindings.h"
#include "runtime_registry.h"
#include "portable_draw_source.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MCUJS_CANVAS_FIXTURE_ONLY 1
#if defined(MCUJS_CANVAS_STICKY) && !defined(MCUJS_CANVAS_EPAPER154)
#include "canvas_sticky_test.c"
#define ADAPTER_NAME "Sticky SSD1677"
#define EXPECTED_WIDTH 800
#define EXPECTED_HEIGHT 480
#define BLACK_BYTE 47900
#elif defined(MCUJS_CANVAS_EPAPER154) && !defined(MCUJS_CANVAS_STICKY)
#include "canvas_epaper_test.c"
#define ADAPTER_NAME "ePaper154"
#define EXPECTED_WIDTH 200
#define EXPECTED_HEIGHT 200
#define BLACK_BYTE 0
#else
#error "Select exactly one configured display adapter"
#endif

extern jerry_value_t js_create_devices_module(void);
extern jerry_value_t js_create_canvas_module(void);
extern jerry_value_t js_create_canvas_native_module(void);
extern jerry_value_t js_create_events_module(void);
extern void js_canvas_present(void);
extern void js_canvas_reset(void);

static unsigned native_live, native_alloc_calls;
static bool fail_native_alloc;
void *mcujs_test_canvas_calloc(size_t count, size_t size) {
    native_alloc_calls++;
    if (fail_native_alloc) { fail_native_alloc = false; return NULL; }
    void *p = calloc(count, size);
    if (p) native_live++;
    return p;
}
void mcujs_test_canvas_free(void *p) {
    if (p) { assert(native_live > 0); native_live--; }
    free(p);
}

static struct {
    const char *name;
    jerry_value_t (*factory)(void);
    jerry_value_t value;
    bool loaded;
} modules[] = {
    {"devices", js_create_devices_module, 0, false},
    {"canvas", js_create_canvas_module, 0, false},
    {"mcujs:canvas-native", js_create_canvas_native_module, 0, false},
    {"events", js_create_events_module, 0, false},
    {"board", js_create_board_module, 0, false},
};
static const char *phase;
static void check_result(jerry_value_t value) {
    if (!jerry_value_is_exception(value)) return;
    jerry_value_t error = jerry_exception_value(value, false);
    jerry_value_t text = jerry_value_to_string(error);
    char message[512] = {0};
    if (jerry_value_is_string(text))
        jerry_string_to_buffer(text, JERRY_ENCODING_UTF8,
                              (jerry_char_t *)message, sizeof(message) - 1);
    fprintf(stderr, "%s / %s: %s\n", ADAPTER_NAME, phase, message);
    jerry_value_free(text);
    jerry_value_free(error);
    jerry_value_free(value);
    exit(1);
}
static void evaluate(const char *source) {
    jerry_value_t result = jerry_eval((const jerry_char_t *)source,
                                     strlen(source), JERRY_PARSE_NO_OPTS);
    check_result(result);
    jerry_value_free(result);
}
static void publish(const char *name, jerry_value_t value) {
    jerry_value_t global = jerry_current_realm();
    jerry_value_t result = jerry_object_set_sz(global, name, value);
    check_result(result);
    assert(jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_free(global);
    jerry_value_free(value);
}
static jerry_value_t require_module(const jerry_call_info_t *info,
                                   const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    assert(argc == 1 && jerry_value_is_string(args[0]));
    char name[32] = {0};
    jerry_size_t n = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    assert(n < sizeof(name));
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)name, n);
    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        if (strcmp(name, modules[i].name) != 0) continue;
        if (!modules[i].loaded) {
            jerry_value_t value = modules[i].factory();
            if (jerry_value_is_exception(value)) return value;
            modules[i].value = value;
            modules[i].loaded = true;
        }
        return jerry_value_copy(modules[i].value);
    }
    fprintf(stderr, "Unexpected require: %s\n", name);
    return jerry_throw_sz(JERRY_ERROR_COMMON, "Test require: unsupported module");
}
static jerry_value_t false_callback(const jerry_call_info_t *info,
                                    const jerry_value_t args[], jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    return jerry_boolean(false);
}
static unsigned io_count(void) {
#ifdef MCUJS_CANVAS_STICKY
    return transfers;
#else
    return command_count;
#endif
}
static void assert_released(void) {
    assert(live == 0 && bus_owned == 0);
#ifdef MCUJS_CANVAS_STICKY
    assert(pins[47] == 0);
#else
    assert(pins[6] == 1);
#endif
}
static void assert_frame(void) {
    /* Exact portable draw.js: black frame, white 8x8 origin square.
     * Check real packed SPI bytes, including Sticky's inverted row ordering. */
    assert(ram_len == sizeof(ram));
    for (size_t i = 0; i < ram_len; i++) {
        size_t row=i/(EXPECTED_WIDTH/8), column=i%(EXPECTED_WIDTH/8);
#ifdef MCUJS_CANVAS_STICKY
        bool white=column==0 && row>=EXPECTED_HEIGHT-8;
#else
        bool white=column==0 && row<8;
#endif
        assert(ram[i] == (white ? 0xff : 0x00));
    }
#ifdef MCUJS_CANVAS_STICKY
    assert(old_len == ram_len && memcmp(old_ram, ram, ram_len) == 0);
#endif
}
static size_t retained_heap(void) {
    jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);
    jerry_heap_stats_t stats;
    assert(jerry_heap_stats(&stats) && "Jerry must enable JERRY_MEM_STATS");
    /* Jerry may reserve a small header from the configured heap. */
    assert(stats.size >= 63u * 1024u && stats.size <= 64u * 1024u &&
           "Use the externally built 64 KiB heap");
    return stats.allocated_bytes;
}
static void setup_vm(void) {
    phase = "VM setup";
    jerry_init(JERRY_INIT_EMPTY);
    (void)retained_heap();
    const mcujs_runtime_registry_t *registry = mcujs_runtime_registry();
    assert(mcujs_runtime_has_module("devices"));
    assert(mcujs_runtime_find_capability("devices") != NULL);
    jerry_value_t board = jerry_object();
    assert(js_board_apply_registry(board,
        registry->safe_mode ? false_callback : NULL,
        registry->storage_ready ? false_callback : NULL));
    publish("board", board);
    publish("require", jerry_function_external(require_module));
}
static void discovery_and_lifecycle(void) {
    phase = "discovery / explicit lifecycle";
    unsigned io = io_count(), refreshes = updates, allocs = native_alloc_calls;
    int saved_pins[49]; memcpy(saved_pins, pins, sizeof(pins));
    evaluate("var module={exports:{}};");
    evaluate(portable_draw_source);
    evaluate("var paint=module.exports;module=null;");
    evaluate(
        "function eq(a,b,m){if(a!==b)throw Error(m+': '+a+' !== '+b);}"
        "function code(f,w){var got='NO_ERROR';try{f();}catch(e){got=e.code;}eq(got,w,'error code');}"
        "var devices=require('devices'),desc=devices.display,Canvas=require('canvas');"
        "var h=null,old=null,direct=null,c=null;"
        "eq(Object.isFrozen(devices),true,'frozen exports');"
        "eq(require('devices'),devices,'cached exports');"
        "eq(desc,devices.display,'stable descriptor');"
        "eq(Object.isFrozen(desc),true,'frozen descriptor');"
        "eq(Object.isFrozen(desc.capabilities),true,'frozen capabilities');"
        "eq(desc.capabilities,desc.capabilities,'stable capabilities');"
        "eq(desc.capabilities.interface,'canvas-2d-subset','interface');"
        "eq(desc.capabilities.maxOpenHandles,1,'exclusive lease');"
        "eq(desc.capabilities.interface,board.capability('devices').display.interface,'real registry');"
        "eq(desc.state,'idle','discovery is not physical readiness');");
    assert(live == 0 && bus_owned == 0 && native_alloc_calls == allocs);
    assert(io_count() == io && updates == refreshes);
    assert(memcmp(saved_pins, pins, sizeof(pins)) == 0);
    evaluate(
        "h=desc.open();c=h.canvas;eq(h.state,'open','open handle');"
        "eq(desc.state,'busy','leased descriptor');eq(c,h.canvas,'stable Canvas');"
        "eq(c.getContext('2d'),c.getContext('2d'),'stable context');"
        "eq(c.getContext('2d').canvas,c,'context canvas');"
        "code(function(){desc.open();},'EBUSY');"
        "code(function(){Canvas.connect('default');},'EBUSY');"
        "paint(h);");
    assert(live == 1 && bus_owned == 1);
    assert(io_count() == io && updates == refreshes);
    char dimensions[128];
    snprintf(dimensions, sizeof(dimensions), "eq(c.width,%d,'width');eq(c.height,%d,'height');",
             EXPECTED_WIDTH, EXPECTED_HEIGHT);
    evaluate(dimensions);
    evaluate("h.present();eq(h.state,'open','present stays open');");
    assert(updates > refreshes); assert_frame();
    io = io_count(); refreshes = updates;
    evaluate("h.present();");
    js_canvas_present();
    assert(io_count() == io && updates == refreshes); /* clean is a no-op */
    evaluate(
        "h.close();h.close();eq(h.state,'closed','repeated close');"
        "eq(desc.state,'idle','owner released');"
        "code(function(){h.present();},'ENXIO');"
        "code(function(){c.getContext('2d').fillRect(0,0,1,1);},'ENXIO');"
        "old=h;h=desc.open();eq(h.canvas===c,false,'reopen fresh Canvas');"
        "old.close();eq(h.state,'open','old close cannot close new owner');"
        "paint(h);h.close();h.close();");
    js_canvas_present();
    assert(updates == refreshes && io_count() == io); /* close discards pending */
    assert_released();
    evaluate(
        "direct=Canvas.connect('default');eq(desc.state,'busy','direct Canvas owner');"
        "code(function(){desc.open();},'EBUSY');direct.close();direct.close();"
        "eq(desc.state,'idle','direct owner released');h=desc.open();paint(h);");
    js_canvas_present();
    assert(updates > refreshes); assert_frame();
    io = io_count(); js_canvas_present(); assert(io_count() == io);
    evaluate("h.close();h=null;old=null;direct=null;c=null;");
    assert_released();
}
static void failures_and_recovery(void) {
    phase = "failure / unavailable / recovery";
    unsigned io = io_count(), refreshes = updates;
    fail_native_alloc = true;
    evaluate("code(function(){desc.open();},'ERR_RESOURCE_EXHAUSTED');");
    assert(!fail_native_alloc); assert_released();
    assert(io_count() == io && updates == refreshes);
#ifdef MCUJS_CANVAS_STICKY
    /* This fixture exposes init faults; ePaper's fixture only exposes SPI/BUSY
     * presentation faults. Do not fake a production init result to fill a gap.
     */
    fail_gpio = 1;
    evaluate("code(function(){desc.open();},'EIO');eq(desc.state,'unavailable','init failure');");
    fail_gpio = 0;
    assert_released();
#endif
    evaluate("h=desc.open();paint(h);h.present();h.close();eq(desc.state,'idle','init retry');"
             "h=desc.open();paint(h);");
    assert_frame();
    refreshes = updates;
#ifdef MCUJS_CANVAS_STICKY
    fail_spi = (int)transfers + 1;
#else
    fail_spi = 1;
#endif
    evaluate(
        "code(function(){h.present();},'EIO');eq(h.state,'error','failed handle');"
        "eq(desc.state,'unavailable','presentation failure');"
        "h.close();h.close();eq(h.state,'error','close preserves error');"
        "code(function(){h.present();},'ENXIO');"
        "code(function(){paint(h);},'ENXIO');old=h;");
    fail_spi = 0;
    assert_released(); assert(updates == refreshes);
    evaluate(
        "h=desc.open();old.close();eq(h.state,'open','retained failed handle isolation');"
        "eq(desc.state,'busy','new lease');code(function(){old.present();},'ENXIO');"
        "paint(h);h.present();eq(old.state,'error','old error survives successful retry');"
        "h.close();eq(desc.state,'idle','successful recovery');h=desc.open();old=null;");
    assert_frame();
    (void)retained_heap(); /* finalizing old handles must not release the new owner */
    assert(live == 1 && bus_owned == 1 && native_live == 1);
    evaluate("eq(h.state,'open','old handle GC isolation');paint(h);");
    stuck = 1;
    int64_t before = now;
    refreshes = updates;
    js_canvas_present(); /* auto-presentation failure must share state semantics */
    stuck = 0;
    assert(now > before && updates == refreshes); assert_released();
    evaluate(
        "eq(h.state,'error','auto-present failed handle');"
        "eq(desc.state,'unavailable','auto-present unavailable');"
        "h.close();eq(h.state,'error','auto error survives close');"
        "code(function(){h.present();},'ENXIO');old=h;h=desc.open();"
        "old.close();paint(h);h.present();h.close();eq(desc.state,'idle','timeout retry');"
        "h=null;old=null;c=null;direct=null;");
    assert_frame(); assert_released();
}
static void canvas_text_pixels(void) {
    phase = "bounded Canvas text / real rasterizer and packed SPI pixels";
    evaluate("h=desc.open();var t=h.canvas.getContext('2d');"
             "eq(t.font,'8px monospace','default bitmap font');"
             "eq(t.measureText('MCU.js 123').width,60,'literal width');"
             "t.clearRect(0,0,h.canvas.width,h.canvas.height);"
             "t.fillStyle='white';t.fillText('A',0,7);t.save();"
             "t.font='16px monospace';t.fillText('!',20,20);"
             "t.fillText('!',h.canvas.width-5,h.canvas.height+10);"
             "t.restore();eq(t.font,'8px monospace','restored font');h.present();");
    static const char *glyph[7] = {".###.","#...#","#...#","#...#","#####","#...#","#...#"};
    assert(ram_len == sizeof(ram));
    for (int y=0; y<EXPECTED_HEIGHT; y++) for (int x=0; x<EXPECTED_WIDTH; x++) {
        bool white=(x<5 && y<7 && glyph[y][x]=='#') ||
            (x>=24 && x<26 && ((y>=6 && y<16) || (y>=18 && y<20))) ||
            (x==EXPECTED_WIDTH-1 && y>=EXPECTED_HEIGHT-4);
        int row=y;
#ifdef MCUJS_CANVAS_STICKY
        row=EXPECTED_HEIGHT-1-y;
#endif
        bool actual=(ram[row*(EXPECTED_WIDTH/8)+x/8] & (0x80u>>(x%8)))!=0;
        assert(actual==white);
    }
    evaluate("var ascii='';for(var n=32;n<=126;n++)ascii+=String.fromCharCode(n);"
             "eq(t.measureText(ascii).width,570,'ASCII metrics');t.fillText(ascii,0,40);"
             "t.fillText(new Array(129).join('W'),0,60);"
             "h.close();code(function(){t.fillText('A',0,7);},'ENXIO');"
             "code(function(){t.measureText('A');},'ENXIO');"
             "h=desc.open();eq(h.canvas.getContext('2d').font,'8px monospace','fresh font');"
             "h.close();h=null;t=null;ascii=null;");
    assert_released();
    puts("PASS Canvas text: literal A/2x ! pixels, right/bottom clipping, ASCII/bound, metrics, close/reopen");
}

static void canvas_circle_pixels(void) {
    phase = "bounded Canvas arcs / independent circle distance oracle";
    /* Real JS module + rasterizer + SPI packing. Skip only the two-pixel
     * antialiased polygon boundary; expected interiors use circle equations,
     * not production tessellation or its emitted path. */
    const int cases[][4] = {{64,64,48,0}, {0,64,24,0},
        {EXPECTED_WIDTH-1,64,40,1}, {64,0,24,0},
        {64,EXPECTED_HEIGHT-1,24,0}, {64,64,128,0}};
    for (unsigned n=0; n<sizeof(cases)/sizeof(cases[0]); n++) {
        int cx=cases[n][0], cy=cases[n][1], radius=cases[n][2], stroke=cases[n][3];
        char script[600];
        snprintf(script,sizeof(script),
            "h=desc.open();var t=h.canvas.getContext('2d');"
            "t.clearRect(0,0,h.canvas.width,h.canvas.height);"
            "t.fillStyle='white';t.strokeStyle='white';t.lineWidth=4;"
            "t.beginPath();t.arc(%d,%d,%d,0,2*Math.PI);t.closePath();t.%s();h.present();",
            cx,cy,radius,stroke?"stroke":"fill");
        evaluate(script);
        assert(ram_len==sizeof(ram));
        for (int y=0; y<EXPECTED_HEIGHT; y++) for (int x=0; x<EXPECTED_WIDTH; x++) {
            double dx=x+0.5-cx, dy=y+0.5-cy, d2=dx*dx+dy*dy;
            bool inside=stroke ? d2>(radius-0.75)*(radius-0.75) && d2<(radius+0.75)*(radius+0.75)
                               : d2<(radius-2.0)*(radius-2.0);
            bool outside=stroke ? d2<(radius-3.0)*(radius-3.0) || d2>(radius+3.0)*(radius+3.0)
                                : d2>(radius+2.0)*(radius+2.0);
            int row=y;
#ifdef MCUJS_CANVAS_STICKY
            row=EXPECTED_HEIGHT-1-y;
#endif
            bool actual=(ram[row*(EXPECTED_WIDTH/8)+x/8] & (0x80u>>(x%8)))!=0;
            if(inside) assert(actual);
            if(outside) assert(!actual);
        }
        evaluate("h.close();code(function(){t.fill();},'ENXIO');"
                 "code(function(){t.stroke();},'ENXIO');h=null;t=null;");
        assert_released();
    }
    puts("PASS Canvas circles: fill/stroke, radius 128, four-edge clipping, close/reopen; distance oracle");
}

static void cycles_and_cleanup(unsigned vm) {
    phase = "bounded GC / VM destruction";
    const char *cycle = "(function(){var d=desc.open();paint(d);d.present();"
                        "d.close();d.close();eq(d.state,'closed','cycle closed');})();";
    /* Warm parser/builtin allocations before comparing retained, not peak, heap.
     * C drives short evals so one large test script does not occupy the 64 KiB VM.
     */
    for (unsigned i = 0; i < 4; i++) { evaluate(cycle); (void)retained_heap(); }
    size_t baseline = retained_heap();
    assert(native_live == 0); assert_released();
    for (unsigned i = 0; i < 24; i++) {
        unsigned refreshes = updates;
        evaluate(cycle);
        assert(updates > refreshes); assert_frame(); assert_released();
        size_t retained = retained_heap();
        assert(native_live == 0);
        assert(retained <= baseline + 1024u); /* bounded bookkeeping slack */
    }
    size_t retained = retained_heap();
    printf("%s VM %u: retained JS heap %zu -> %zu bytes after 24 cycles\n",
           ADAPTER_NAME, vm, baseline, retained);
    unsigned refreshes = updates;
    evaluate("h=desc.open();paint(h);eq(desc.state,'busy','shutdown with live owner');");
    assert(live == 1 && bus_owned == 1);
    js_canvas_reset(); /* production engine teardown hook */
    assert_released();
    evaluate("eq(h.state,'closed','reset invalidates live handle');eq(desc.state,'idle','reset releases lease');");
    fail_native_alloc=true;
    evaluate("code(function(){desc.open();},'ERR_RESOURCE_EXHAUSTED');eq(desc.state,'unavailable','failure before reset');");
    js_canvas_reset();
    evaluate("eq(desc.state,'idle','reset clears failure residue');");
    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        if (modules[i].loaded) jerry_value_free(modules[i].value);
        modules[i].loaded = false;
        modules[i].value = 0;
    }
    jerry_cleanup(); /* deliberately leave a rooted, now-invalid handle */
    assert_released(); assert(native_live == 0 && updates == refreshes);
}
int main(void) {
    for (unsigned vm = 1; vm <= 2; vm++) {
        setup_vm();
        discovery_and_lifecycle();
        failures_and_recovery();
        canvas_text_pixels();
        canvas_circle_pixels();
        cycles_and_cleanup(vm);
    }
    printf("PASS %s: real devices/Canvas factories, registry and driver; lifecycle, "
           "SPI bytes, exclusive ownership, faults, GC and VM recreation\n", ADAPTER_NAME);
    return 0;
}
