/* MCU.js timer bindings for ESP32-S3. */

#include "bindings.h"
#include "jerryscript.h"

#include "esp_timer.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define MAX_TIMERS 16

typedef struct {
    uint32_t id;
    jerry_value_t callback;
    uint32_t trigger_time_ms;
    uint32_t interval_ms;
    bool active;
} timer_entry_t;

static timer_entry_t s_timers[MAX_TIMERS];
static uint32_t s_next_timer_id = 1;
static bool s_initialized;

static uint32_t get_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void timers_init(void) {
    if (!s_initialized) {
        memset(s_timers, 0, sizeof(s_timers));
        s_initialized = true;
    }
}

static timer_entry_t *find_timer(uint32_t id) {
    for (size_t i = 0; i < MAX_TIMERS; i++) {
        if (s_timers[i].active && s_timers[i].id == id) {
            return &s_timers[i];
        }
    }
    return NULL;
}

static timer_entry_t *find_free_timer(void) {
    for (size_t i = 0; i < MAX_TIMERS; i++) {
        if (!s_timers[i].active) {
            return &s_timers[i];
        }
    }
    return NULL;
}

static jerry_value_t create_timer(const jerry_value_t args[], jerry_length_t argc,
                                  bool interval) {
    timers_init();
    if (argc < 1 || !jerry_value_is_function(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "First argument must be a function");
    }
    timer_entry_t *timer = find_free_timer();
    if (timer == NULL) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Maximum number of timers reached");
    }
    double requested_delay = js_get_number_arg(args, argc, 1, 0);
    if (!isfinite(requested_delay) || requested_delay < 0) {
        requested_delay = 0;
    } else if (requested_delay > UINT32_MAX) {
        requested_delay = UINT32_MAX;
    }
    uint32_t delay = (uint32_t)requested_delay;
    if (interval && delay == 0) {
        delay = 1;
    }
    timer->id = s_next_timer_id++;
    timer->callback = jerry_value_copy(args[0]);
    timer->trigger_time_ms = get_time_ms() + delay;
    timer->interval_ms = interval ? delay : 0;
    timer->active = true;
    return jerry_number((double)timer->id);
}

static jerry_value_t set_timeout_handler(const jerry_call_info_t *info,
                                         const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    return create_timer(args, argc, false);
}

static jerry_value_t set_interval_handler(const jerry_call_info_t *info,
                                          const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    return create_timer(args, argc, true);
}

static jerry_value_t clear_timer_handler(const jerry_call_info_t *info,
                                         const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    uint32_t id = (uint32_t)js_get_number_arg(args, argc, 0, 0);
    timer_entry_t *timer = find_timer(id);
    if (timer != NULL) {
        jerry_value_free(timer->callback);
        timer->active = false;
    }
    return jerry_undefined();
}

/* Release retained callbacks while their JerryScript context is still alive. */
void js_timers_cleanup(void) {
    for (size_t i = 0; i < MAX_TIMERS; i++) {
        if (s_timers[i].active) {
            jerry_value_free(s_timers[i].callback);
        }
    }
    memset(s_timers, 0, sizeof(s_timers));
    s_initialized = false;
    s_next_timer_id = 1;
}

void js_bind_timers(void) {
    timers_init();
    jerry_value_t global = jerry_current_realm();
    js_set_function(global, "setTimeout", set_timeout_handler);
    js_set_function(global, "setInterval", set_interval_handler);
    js_set_function(global, "clearTimeout", clear_timer_handler);
    js_set_function(global, "clearInterval", clear_timer_handler);
    jerry_value_free(global);
}

bool js_timers_process(void) {
    if (!s_initialized) {
        return false;
    }
    uint32_t now = get_time_ms();
    bool active = false;
    for (size_t i = 0; i < MAX_TIMERS; i++) {
        timer_entry_t *timer = &s_timers[i];
        if (!timer->active) {
            continue;
        }
        active = true;
        if ((int32_t)(now - timer->trigger_time_ms) < 0) {
            continue;
        }
        /* Transition the slot before invoking JavaScript. The callback may
         * clear itself or allocate another timer into this same slot. */
        jerry_value_t callback = jerry_value_copy(timer->callback);
        if (timer->interval_ms > 0) {
            timer->trigger_time_ms = now + timer->interval_ms;
        } else {
            jerry_value_free(timer->callback);
            timer->active = false;
        }
        jerry_value_t result = jerry_call(callback, jerry_undefined(), NULL, 0);
        jerry_value_free(result);
        jerry_value_free(callback);
    }
    return active;
}
