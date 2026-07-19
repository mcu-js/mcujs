#include "bindings.h"
#include "runtime_registry.h"

#include <string.h>

static jerry_value_t object_get(jerry_value_t object, const char *name) {
    jerry_value_t key = jerry_string_sz(name);
    jerry_value_t value = jerry_object_get(object, key);
    jerry_value_free(key);
    return value;
}

static jerry_value_t current_board(void) {
    jerry_value_t global = jerry_current_realm();
    jerry_value_t board = object_get(global, "board");
    jerry_value_free(global);
    return board;
}

static jerry_value_t parse_json(const char *json) {
    return jerry_json_parse((const jerry_char_t *)json,
                            (jerry_size_t)strlen(json));
}

static jerry_value_t board_capability_handler(const jerry_call_info_t *call_info,
                                               const jerry_value_t args[],
                                               jerry_length_t argc) {
    (void)call_info;
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "board.capability name must be a string");
    }

    char name[MCUJS_RUNTIME_CAPABILITY_NAME_MAX + 1];
    jerry_size_t length = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    if (length == 0 || length >= sizeof(name)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "board capability name is invalid");
    }
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                           (jerry_char_t *)name, length);
    name[length] = '\0';

    const mcujs_runtime_capability_t *capability =
        mcujs_runtime_find_capability(name);
    if (capability == NULL) return jerry_undefined();
    return parse_json(capability->json);
}

static jerry_value_t board_capabilities_handler(const jerry_call_info_t *call_info,
                                                 const jerry_value_t args[],
                                                 jerry_length_t argc) {
    (void)call_info;
    (void)args;
    (void)argc;
    const mcujs_runtime_registry_t *registry = mcujs_runtime_registry();
    jerry_value_t capabilities = jerry_object();
    for (size_t i = 0; i < registry->capability_count; i++) {
        const mcujs_runtime_capability_t *entry = &registry->capabilities[i];
        jerry_value_t value = parse_json(entry->json);
        if (jerry_value_is_exception(value)) {
            jerry_value_free(capabilities);
            return value;
        }
        jerry_value_t key = jerry_string_sz(entry->name);
        jerry_value_t result = jerry_object_set(capabilities, key, value);
        jerry_value_free(key);
        jerry_value_free(value);
        if (jerry_value_is_exception(result)) {
            jerry_value_free(capabilities);
            return result;
        }
        jerry_value_free(result);
    }
    return capabilities;
}

static bool apply_gated_method(jerry_value_t board, const char *name,
                               bool advertised,
                               jerry_external_handler_t handler) {
    if (advertised != (handler != NULL)) return false;
    if (advertised) js_set_function(board, name, handler);
    return true;
}

bool js_board_apply_registry(jerry_value_t board,
                             jerry_external_handler_t safe_mode_handler,
                             jerry_external_handler_t storage_ready_handler) {
    const mcujs_runtime_registry_t *registry = mcujs_runtime_registry();
    if (!apply_gated_method(board, "safeMode", registry->safe_mode,
                            safe_mode_handler) ||
        !apply_gated_method(board, "storageReady", registry->storage_ready,
                            storage_ready_handler)) {
        return false;
    }
    jerry_value_t identity = parse_json(registry->board_json);
    if (jerry_value_is_exception(identity)) {
        jerry_value_free(identity);
        return false;
    }

    jerry_value_t api_version = jerry_string_sz(registry->api_version);
    jerry_value_t exposed_pins = object_get(identity, "exposedPins");
    jerry_value_t pins = object_get(identity, "pins");
    jerry_value_t devices = object_get(identity, "devices");

    js_set_property(board, "apiVersion", api_version);
    js_set_property(board, "exposedPins", exposed_pins);
    js_set_property(board, "pins", pins);
    js_set_property(board, "devices", devices);
    js_set_function(board, "capability", board_capability_handler);
    js_set_function(board, "capabilities", board_capabilities_handler);

    jerry_value_free(devices);
    jerry_value_free(pins);
    jerry_value_free(exposed_pins);
    jerry_value_free(api_version);
    jerry_value_free(identity);
    return true;
}

jerry_value_t js_create_board_module(void) {
    return current_board();
}
