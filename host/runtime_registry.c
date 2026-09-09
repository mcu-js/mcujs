#include "runtime_registry.h"
#include "runtime_features.h"

#include <string.h>

#define MCUJS_MODULE_NAME(name) name,
static const char *const s_builtin_modules[] = {
    MCUJS_RUNTIME_BUILTIN_MODULES(MCUJS_MODULE_NAME)
#ifdef MCUJS_EXPERIMENTAL_CANVAS
    "mcujs:canvas-native",
    "canvas",
    "displays/st7789",
#endif
};
#undef MCUJS_MODULE_NAME

#define MCUJS_CAPABILITY(name, json) {name, json},
static const mcujs_runtime_capability_t s_capabilities[] = {
    MCUJS_RUNTIME_CAPABILITIES(MCUJS_CAPABILITY)
};
#undef MCUJS_CAPABILITY

static const mcujs_runtime_registry_t s_registry = {
    .board_id = MCUJS_RUNTIME_BOARD_ID,
    .api_version = MCUJS_RUNTIME_API_VERSION,
    .board_json = MCUJS_RUNTIME_BOARD_JSON,
    .manifest_json = MCUJS_RUNTIME_MANIFEST_JSON,
    .builtin_modules = s_builtin_modules,
    .builtin_module_count = sizeof(s_builtin_modules) / sizeof(s_builtin_modules[0]),
    .capabilities = s_capabilities,
    .capability_count = sizeof(s_capabilities) / sizeof(s_capabilities[0]),
    .onboard_led = MCUJS_REGISTRY_ONBOARD_LED != 0,
    .onboard_neopixel = MCUJS_REGISTRY_ONBOARD_NEOPIXEL != 0,
    .safe_mode = MCUJS_REGISTRY_SAFE_MODE != 0,
    .storage_ready = MCUJS_REGISTRY_STORAGE_READY != 0,
};

const mcujs_runtime_registry_t *mcujs_runtime_registry(void) {
    return &s_registry;
}

bool mcujs_runtime_has_module(const char *specifier) {
    if (specifier == NULL) return false;
    for (size_t i = 0; i < s_registry.builtin_module_count; i++) {
        if (strcmp(specifier, s_registry.builtin_modules[i]) == 0) return true;
    }
    return false;
}

const mcujs_runtime_capability_t *mcujs_runtime_find_capability(const char *name) {
    if (name == NULL) return NULL;
    for (size_t i = 0; i < s_registry.capability_count; i++) {
        if (strcmp(name, s_registry.capabilities[i].name) == 0) {
            return &s_registry.capabilities[i];
        }
    }
    return NULL;
}
