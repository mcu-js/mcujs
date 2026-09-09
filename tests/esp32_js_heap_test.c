/* Actual ESP port and JerryScript; fake only the ESP-IDF memory/timer boundary. */
#include "jerryscript.h"
#include "jerryscript-port.h"
#include "canvas_epaper_stubs/fake_idf.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void *allocation;
static size_t allocation_size;
static unsigned allocations, releases;
static bool fail_allocation;
void *heap_caps_malloc(size_t bytes, int caps) {
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    assert(!allocation);
    if (fail_allocation) return NULL;
    allocation = malloc(bytes);
    assert(allocation);
    allocation_size = bytes;
    allocations++;
    return allocation;
}
void heap_caps_free(void *p) {
    assert(p == allocation && p);
    free(p);
    allocation = NULL;
    releases++;
}
void vTaskDelay(unsigned ticks) { (void)ticks; }
int64_t esp_timer_get_time(void) { return 0; }

#if MCUJS_JS_HEAP_EXTERNAL
static void aborted(int sig) { (void)sig; _Exit(86); }
static void allocation_failure(bool overflow) {
    pid_t pid = fork();
    assert(pid >= 0);
    if (!pid) {
        signal(SIGABRT, aborted);
        fail_allocation = true;
        if (overflow) jerry_port_context_alloc(SIZE_MAX);
        else jerry_init(JERRY_INIT_EMPTY);
        _Exit(1);
    }
    int status;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 86);
}
#endif

int main(void) {
#if MCUJS_JS_HEAP_EXTERNAL
    /* Literal alignment and arena-size expectations, independent of implementation. */
    size_t total = jerry_port_context_alloc(13);
    assert(total == 16 + 262144 && total == allocation_size);
    assert((uintptr_t)jerry_port_context_get() % 8 == 0);
    assert((void *)jerry_port_context_get() == allocation);
    jerry_port_context_free();
    assert(!jerry_port_context_get() && !allocation);
    jerry_port_context_free(); /* no double free */
    assert(allocations == 1 && releases == 1);
    allocation_failure(false);
    allocation_failure(true);
#endif
    const char *source =
        "var objects=[]; for(var i=0;i<5000;i++) objects.push({value:i});"
        "if(objects[4999].value!==4999) throw Error('corrupt graph');";
    for (unsigned run = 0; run < 3; run++) {
        jerry_init(JERRY_INIT_EMPTY);
        jerry_value_t result = jerry_eval((const jerry_char_t *)source, strlen(source), JERRY_PARSE_NO_OPTS);
        assert(!jerry_value_is_exception(result));
        jerry_value_free(result);
        jerry_heap_stats_t stats;
        assert(jerry_heap_stats(&stats));
        assert(stats.allocated_bytes > 131072);
        printf("live JS object graph: %zu bytes in %zu-byte heap\n", stats.allocated_bytes, stats.size);
        const char *clear = "objects=null;";
        result = jerry_eval((const jerry_char_t *)clear, strlen(clear), JERRY_PARSE_NO_OPTS);
        assert(!jerry_value_is_exception(result));
        jerry_value_free(result);
        jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);
        assert(jerry_heap_stats(&stats));
        assert(stats.allocated_bytes < 16384);
        jerry_cleanup();
#if MCUJS_JS_HEAP_EXTERNAL
        assert(!allocation && !jerry_port_context_get());
        assert(allocations == releases);
#endif
    }
    puts("PASS JS heap: capacity, object integrity, GC reclamation, cleanup/reinit and PSRAM-only failure behavior");
    return 0;
}
