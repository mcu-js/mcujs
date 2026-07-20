#include "onboard_led.h"

#include "board_config.h"
#include "gpio_internal.h"
#include "runtime_features.h"
#include "validation.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#if MCUJS_REGISTRY_ONBOARD_LED && !MCUJS_HAS_CYW43

static jerry_value_t throw_led_busy(void) {
    const mcujs_error_details_t details = {
        .resource = "gpio",
        .has_pin = true,
        .pin = MCUJS_LED_PIN,
    };
    return mcujs_throw_operational_error(
        MCUJS_ERROR_BUSY, "Onboard LED pin is owned by another peripheral",
        &details);
}

jerry_value_t mcujs_rp2_board_led_handler(
    const jerry_call_info_t *call_info_p, const jerry_value_t args[],
    jerry_length_t argc) {
    (void)call_info_p;

    bool on = false;
    if (argc > 0) {
        mcujs_arg_status_t status = mcujs_get_boolean(args, argc, 0, &on);
        if (status != MCUJS_ARG_OK) {
            return jerry_throw_sz(JERRY_ERROR_TYPE,
                                  "LED state must be boolean");
        }
    }

    if (!mcujs_rp2_gpio_prepare_soft_output(MCUJS_LED_PIN)) {
        return throw_led_busy();
    }
    if (argc < 1) return jerry_boolean(gpio_get(MCUJS_LED_PIN));

    gpio_put(MCUJS_LED_PIN, on);
    return jerry_undefined();
}

#endif
