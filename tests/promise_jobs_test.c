/* Production engine and timer bindings with only hardware time/other modules stubbed. */
#include "engine.h"
#include "jerryscript.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t now_ms;
uint64_t get_absolute_time(void) { return now_ms; }
uint64_t to_ms_since_boot(uint64_t time) { return time; }
int64_t esp_timer_get_time(void) { return (int64_t)now_ms * 1000; }
#define STUB(name) void name(void) {}
STUB(js_bind_console) STUB(js_bind_board) STUB(js_bind_gpio) STUB(js_bind_pwm)
STUB(js_bind_i2c) STUB(js_bind_spi) STUB(js_bind_adc) STUB(js_bind_neopixel)
STUB(js_bind_process) STUB(js_bind_require) STUB(js_bind_graphics) STUB(js_bind_screen)
STUB(js_bind_dvi) STUB(js_module_loader_init) STUB(js_module_loader_cleanup)
static unsigned require_cleanup_calls, fs_cleanup_calls;
void js_fs_cleanup(void) { fs_cleanup_calls++; }
void js_require_cleanup(void) {
    /* Teardown is called while the owning VM is still valid. */
    jerry_value_free(jerry_object());
    require_cleanup_calls++;
}

static void exec(const char *source) {
    js_result_t status = js_engine_exec(source, strlen(source), NULL, 0);
    if (status != JS_OK) {
        char error[512]; js_engine_get_error(error, sizeof(error));
        fprintf(stderr, "Execution failed: %s\n", error);
    }
    assert(status == JS_OK);
}
static void expect(const char *expression, const char *expected) {
    char actual[256];
    assert(js_engine_exec(expression, strlen(expression), actual, sizeof(actual)) == JS_OK);
    if (strcmp(actual, expected)) {
        fprintf(stderr, "%s: expected %s, got %s\n", expression, expected, actual);
        assert(0);
    }
}
/* A host-owned counter outlives every VM: discarded callbacks cannot hide by
 * writing only to globals that disappear during cleanup. */
static unsigned observed_jobs;
static jerry_value_t record_job(const jerry_call_info_t *info,
                                const jerry_value_t args[], jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    observed_jobs++;
    return jerry_undefined();
}
static void install_job_recorder(void) {
    jerry_value_t global = jerry_current_realm();
    jerry_value_t callback = jerry_function_external(record_job);
    jerry_value_t result = jerry_object_set_sz(global, "recordJob", callback);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_free(callback);
    jerry_value_free(global);
}
static void expect_pending(bool expected) {
    bool pending = !expected;
    jerry_value_t result = mcujs_jerry_run_jobs(0, &pending);
    assert(!jerry_value_is_exception(result));
    jerry_value_free(result);
    if (pending != expected) {
        fprintf(stderr, "Pending expected %d, got %d; observed callbacks %u\n",
                expected, pending, observed_jobs);
    }
    assert(pending == expected);
}
static void queued_family_cleanup(void) {
    const struct { const char *name; const char *source; } cases[] = {
        {"reaction", "Promise.resolve().then(recordJob);"},
        {"async reaction", "(async function(){await Promise.resolve(1);recordJob();})();"},
        {"async generator continuation",
         "(async function*(){await Promise.resolve(1);recordJob();yield 1;})().next();"},
        {"thenable", "Promise.resolve({then:recordJob});"},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fprintf(stderr, "Checking queued %s teardown\n", cases[i].name);
        unsigned before = observed_jobs;
        unsigned cleanups = require_cleanup_calls;
        assert(js_engine_init() == JS_OK);
        install_job_recorder();
        exec(cases[i].source);
        expect_pending(true);
        assert(observed_jobs == before); /* Not inline. */
        /* Positive control: the public producer really schedules our callback. */
        jerry_value_t result = jerry_run_jobs();
        assert(!jerry_value_is_exception(result));
        jerry_value_free(result);
        assert(observed_jobs == before + 1);
        expect_pending(false);

        exec(cases[i].source);
        expect_pending(true);
        js_engine_gc(); /* Keep queued roots alive until actual teardown. */
        js_engine_cleanup();
        assert(observed_jobs == before + 1); /* Cleanup never executes jobs. */
        assert(require_cleanup_calls == cleanups + 1);
        assert(fs_cleanup_calls == cleanups + 1);
        assert(!js_engine_process_timers());

        assert(js_engine_init() == JS_OK);
        expect_pending(false);
        assert(!js_engine_process_timers());
        assert(observed_jobs == before + 1); /* No stale cross-context work. */
        install_job_recorder();
        exec("Promise.resolve().then(recordJob);");
        js_engine_process_timers();
        assert(observed_jobs == before + 2);
        expect_pending(false);
        js_engine_cleanup();
        assert(require_cleanup_calls == cleanups + 2);
        assert(fs_cleanup_calls == cleanups + 2);
        printf("Queued %s cleanup and fresh VM: PASS\n", cases[i].name);
    }
}
int main(void) {
    assert(js_engine_init() == JS_OK);
    exec("var answer = 0; Promise.resolve(42).then(function(v) { answer = v; });");
    expect("answer", "0"); /* Jobs are never invoked inline during evaluation. */
    js_engine_process_timers();
    expect("answer", "42");
    /* Callback mutation must not free or reschedule a replacement timer. */
    exec("var timerDone=0; var id=setInterval(function(){clearInterval(id);"
         "setTimeout(function(){timerDone++},1)},1);");
    now_ms++;
    js_engine_process_timers();
    now_ms++;
    js_engine_process_timers();
    expect("timerDone", "1");

    exec("var caught=''; Promise.reject(new Error('expected')).catch(function(e){caught=e.message});"
         "var asyncValue=0; (async function(){ asyncValue=await Promise.resolve(7); })();");
    js_engine_process_timers();
    expect("caught === 'expected' && asyncValue === 7", "true");

    exec("var order=[]; Promise.resolve().then(function(){order.push('p1')});"
         "setTimeout(function(){order.push('t1');Promise.resolve().then(function(){order.push('p2')})},0);"
         "setTimeout(function(){order.push('t2')},0);");
    js_engine_process_timers();
    expect("order.join(',') === 'p1,t1,t2,p2'", "true");

    /* Replenishing jobs must leave a turn for the timer and host/REPL. */
    exec("var spins=0, running=true, timerSpins=-1; function spin(){spins++;"
         "if(running) Promise.resolve().then(spin)}; Promise.resolve().then(spin);"
         "setTimeout(function(){timerSpins=spins},0);");
    assert(js_engine_process_timers());
    expect("spins", "32");
    expect("timerSpins", "16");
    exec("running=false"); /* A host can process another input between turns. */
    js_engine_process_timers();
    expect("spins", "33");
    assert(!js_engine_process_timers());

    /* No dropped/reordered jobs when the queue crosses multiple budgets. */
    exec("var sequence=[]; for(var n=0;n<75;n++)"
         "Promise.resolve(n).then(function(n){sequence.push(n)});");
    js_engine_process_timers();
    expect("sequence.length", "32");
    js_engine_process_timers();
    expect("sequence.length", "64");
    js_engine_process_timers();
    expect("sequence.length===75 && sequence.every(function(v,i){return v===i})", "true");

    /* Promise thenable and async-generator jobs also use the same FIFO. */
    exec("var thenable=0; Promise.resolve({then:function(resolve){resolve(11)}})"
         ".then(function(v){thenable=v}); var generated=0;"
         "(async function*(){yield 13})().next().then(function(v){generated=v.value});");
    js_engine_process_timers();
    expect("thenable===11 && generated===13", "true");

    /* The bounded extension leaves upstream drain-to-empty behavior intact. */
    exec("var direct=0; for(var i=0;i<40;i++) Promise.resolve().then(function(){direct++});");
    bool pending = false;
    jerry_value_t value = mcujs_jerry_run_jobs(0, &pending);
    assert(pending && !jerry_value_is_exception(value));
    jerry_value_free(value);
    expect("direct", "0");
    value = mcujs_jerry_run_jobs(1, &pending);
    assert(pending && !jerry_value_is_exception(value));
    jerry_value_free(value);
    expect("direct", "1");
    value = jerry_run_jobs();
    assert(!jerry_value_is_exception(value));
    jerry_value_free(value);
    expect("direct", "40");

    exec("running=true; Promise.resolve().then(spin)");
    js_engine_gc();
    js_memory_stats_t before, after;
    js_engine_get_memory_stats(&before);
    assert(before.heap_size > 0);
    for (int turn=0; turn<200; turn++) {
        assert(js_engine_process_timers());
    }
    js_engine_gc();
    js_engine_get_memory_stats(&after);
    assert(after.heap_used <= before.heap_used + 512);
    exec("running=false");
    js_engine_process_timers();

    /* Cleanup must release queued work and timer refs before Jerry teardown. */
    exec("setInterval(function(){throw new Error('stale timer')},1);"
         "Promise.resolve().then(function(){throw new Error('stale job')});");
    js_engine_cleanup();
    assert(require_cleanup_calls == 1);
    assert(fs_cleanup_calls == 1);
    assert(!js_engine_process_timers());
    assert(js_engine_init() == JS_OK);
    now_ms++;
    assert(!js_engine_process_timers());
    exec("var fresh=0; Promise.resolve(99).then(function(v){fresh=v});");
    js_engine_process_timers();
    expect("fresh", "99");
    js_engine_cleanup();
    assert(require_cleanup_calls == 2);
    assert(fs_cleanup_calls == 2);
    queued_family_cleanup();
    puts("Promise scheduling: PASS");
    return 0;
}
