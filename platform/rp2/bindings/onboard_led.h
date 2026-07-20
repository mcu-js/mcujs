#ifndef MCUJS_RP2_ONBOARD_LED_H
#define MCUJS_RP2_ONBOARD_LED_H

#include "jerryscript.h"

jerry_value_t mcujs_rp2_board_led_handler(
    const jerry_call_info_t *call_info_p, const jerry_value_t args[],
    jerry_length_t argc);

#endif
