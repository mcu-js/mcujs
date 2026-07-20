#include "bindings.h"
#include "pin_policy.h"
#include "runtime_features.h"
#include "runtime_validation_backend_stubs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

_Static_assert(MCUJS_RUNTIME_I2C_DEFAULT_BUS == 1,
               "fixture must exercise a defaultBus 1 descriptor");
_Static_assert(MCUJS_RUNTIME_I2C_DEFAULT_SDA == 6,
               "fixture default SDA changed");
_Static_assert(MCUJS_RUNTIME_I2C_DEFAULT_SCL == 7,
               "fixture default SCL changed");

static bool eval_source(const char *source) {
    jerry_value_t result = jerry_eval((const jerry_char_t *)source,
                                      strlen(source), JERRY_PARSE_NO_OPTS);
    if (jerry_value_is_exception(result)) {
        jerry_value_t error = jerry_exception_value(result, true);
        jerry_value_t text = jerry_value_to_string(error);
        char buffer[256];
        jerry_size_t size = jerry_string_size(text, JERRY_ENCODING_UTF8);
        if (size >= sizeof(buffer)) size = sizeof(buffer) - 1;
        jerry_string_to_buffer(text, JERRY_ENCODING_UTF8,
                               (jerry_char_t *)buffer, size);
        buffer[size] = '\0';
        fprintf(stderr, "I2C options board test exception: %s\n", buffer);
        jerry_value_free(text);
        jerry_value_free(error);
        return false;
    }
    jerry_value_free(result);
    return true;
}

static void install_module(const char *name, jerry_value_t module) {
    jerry_value_t global = jerry_current_realm();
    jerry_value_t result = jerry_object_set_sz(global, name, module);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_free(global);
    jerry_value_free(module);
}

int main(void) {
    mcujs_test_reset_backend();
    jerry_init(JERRY_INIT_EMPTY);
    install_module("I2C", js_create_i2c_module());

    assert(eval_source("I2C.init({frequency: 100000});"));
    assert(mcujs_test_i2c_last_init_bus == MCUJS_RUNTIME_I2C_DEFAULT_BUS);
    assert(mcujs_rp2_pin_owner(MCUJS_RUNTIME_I2C_DEFAULT_SDA) ==
           MCUJS_RP2_PIN_OWNER_I2C1);
    assert(mcujs_rp2_pin_owner(MCUJS_RUNTIME_I2C_DEFAULT_SCL) ==
           MCUJS_RP2_PIN_OWNER_I2C1);
    assert(eval_source("if (I2C.write(1, 0x50, [1]) !== 1) "
                       "throw new Error('defaultBus 1 transfer failed');"));

    assert(eval_source(
        "I2C.init({bus: 0, sda: 4, scl: 5, frequency: 100000});"));
    assert(mcujs_test_i2c_last_init_bus == 0);
    assert(mcujs_rp2_pin_owner(4) == MCUJS_RP2_PIN_OWNER_I2C0);
    assert(mcujs_rp2_pin_owner(5) == MCUJS_RP2_PIN_OWNER_I2C0);
    assert(eval_source("if (I2C.write(0, 0x50, [2]) !== 1) "
                       "throw new Error('explicit non-default route failed');"));

    jerry_cleanup();
    puts("production I2C options parser honored defaultBus 1 and an explicit non-default route");
    return 0;
}
