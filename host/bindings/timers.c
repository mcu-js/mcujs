/*
 * mcujs - Timer Bindings
 * 
 * Implements: setTimeout(), clearTimeout(), setInterval(), clearInterval()
 */

#include "bindings.h"
#include "jerryscript.h"

#include "pico/stdlib.h"
#include "hardware/timer.h"

#include <string.h>

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);
extern double js_get_number_arg(const jerry_value_t args[], jerry_length_t argc,
                                jerry_length_t index, double default_value);

/* Maximum number of concurrent timers */
#define MAX_TIMERS 16

/* Timer entry */
typedef struct {
    uint32_t id;
    jerry_value_t callback;
    uint32_t trigger_time_ms;
    uint32_t interval_ms;  /* 0 for setTimeout, >0 for setInterval */
    bool active;
} timer_entry_t;

/* Timer state */
static timer_entry_t s_timers[MAX_TIMERS];
static uint32_t s_next_timer_id = 1;
static bool s_initialized = false;

/*
 * Initialize timer system
 */
static void timers_init(void) {
    if (!s_initialized) {
        memset(s_timers, 0, sizeof(s_timers));
        s_initialized = true;
    }
}

/*
 * Find a free timer slot
 */
static timer_entry_t *find_free_slot(void) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!s_timers[i].active) {
            return &s_timers[i];
        }
    }
    return NULL;
}

/*
 * Find timer by ID
 */
static timer_entry_t *find_timer(uint32_t id) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (s_timers[i].active && s_timers[i].id == id) {
            return &s_timers[i];
        }
    }
    return NULL;
}

/*
 * Get current time in milliseconds
 */
static uint32_t get_time_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

/*
 * Create a timer (shared by setTimeout and setInterval)
 */
static jerry_value_t create_timer(const jerry_value_t args[], jerry_length_t argc,
                                   bool is_interval) {
    timers_init();
    
    if (argc < 1 || !jerry_value_is_function(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "First argument must be a function");
    }
    
    timer_entry_t *timer = find_free_slot();
    if (timer == NULL) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Maximum number of timers reached");
    }
    
    uint32_t delay_ms = (uint32_t)js_get_number_arg(args, argc, 1, 0);
    
    timer->id = s_next_timer_id++;
    timer->callback = jerry_value_copy(args[0]);
    timer->trigger_time_ms = get_time_ms() + delay_ms;
    timer->interval_ms = is_interval ? delay_ms : 0;
    timer->active = true;
    
    return jerry_number((double)timer->id);
}

/*
 * Clear a timer (shared by clearTimeout and clearInterval)
 */
static jerry_value_t clear_timer(const jerry_value_t args[], jerry_length_t argc) {
    if (argc < 1) {
        return jerry_undefined();
    }
    
    uint32_t id = (uint32_t)js_get_number_arg(args, argc, 0, 0);
    timer_entry_t *timer = find_timer(id);
    
    if (timer != NULL) {
        jerry_value_free(timer->callback);
        timer->active = false;
    }
    
    return jerry_undefined();
}

/*
 * setTimeout(callback, delay)
 */
static jerry_value_t set_timeout_handler(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc) {
    (void)call_info_p;
    return create_timer(args, argc, false);
}

/*
 * clearTimeout(id)
 */
static jerry_value_t clear_timeout_handler(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args[],
                                            const jerry_length_t argc) {
    (void)call_info_p;
    return clear_timer(args, argc);
}

/*
 * setInterval(callback, interval)
 */
static jerry_value_t set_interval_handler(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info_p;
    return create_timer(args, argc, true);
}

/*
 * clearInterval(id)
 */
static jerry_value_t clear_interval_handler(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args[],
                                             const jerry_length_t argc) {
    (void)call_info_p;
    return clear_timer(args, argc);
}

/*
 * Process pending timers
 * Should be called from main loop
 * Returns true if there are still active timers
 */
bool js_timers_process(void) {
    if (!s_initialized) {
        return false;
    }
    
    uint32_t now = get_time_ms();
    bool has_active = false;
    
    for (int i = 0; i < MAX_TIMERS; i++) {
        timer_entry_t *timer = &s_timers[i];
        
        if (!timer->active) {
            continue;
        }
        
        has_active = true;
        
        /* Check if timer should fire */
        if ((int32_t)(now - timer->trigger_time_ms) >= 0) {
            /* Call the callback */
            jerry_value_t result = jerry_call(timer->callback, 
                                              jerry_undefined(), 
                                              NULL, 0);
            jerry_value_free(result);
            
            if (timer->interval_ms > 0) {
                /* Reschedule interval timer */
                timer->trigger_time_ms = now + timer->interval_ms;
            } else {
                /* One-shot timer - clean up */
                jerry_value_free(timer->callback);
                timer->active = false;
            }
        }
    }
    
    return has_active;
}

/*
 * Register timer bindings
 */
void js_bind_timers(void) {
    timers_init();
    
    jerry_value_t global = jerry_current_realm();
    
    js_set_function(global, "setTimeout", set_timeout_handler);
    js_set_function(global, "clearTimeout", clear_timeout_handler);
    js_set_function(global, "setInterval", set_interval_handler);
    js_set_function(global, "clearInterval", clear_interval_handler);
    
    jerry_value_free(global);
}
