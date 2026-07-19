#ifndef MCUJS_RUNTIME_REGISTRY_H
#define MCUJS_RUNTIME_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

#define MCUJS_RUNTIME_CAPABILITY_NAME_MAX 31

typedef struct {
    const char *name;
    const char *json;
} mcujs_runtime_capability_t;

typedef struct {
    const char *board_id;
    const char *api_version;
    const char *board_json;
    const char *manifest_json;
    const char *const *builtin_modules;
    size_t builtin_module_count;
    const mcujs_runtime_capability_t *capabilities;
    size_t capability_count;
    bool onboard_led;
    bool onboard_neopixel;
    bool safe_mode;
    bool storage_ready;
} mcujs_runtime_registry_t;

const mcujs_runtime_registry_t *mcujs_runtime_registry(void);
bool mcujs_runtime_has_module(const char *specifier);
const mcujs_runtime_capability_t *mcujs_runtime_find_capability(const char *name);

#endif /* MCUJS_RUNTIME_REGISTRY_H */
