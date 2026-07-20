#include "pin_policy.h"

#define MCUJS_RP2_PIN_COUNT 64

static mcujs_rp2_pin_owner_t s_pin_owners[MCUJS_RP2_PIN_COUNT];

mcujs_rp2_pin_owner_t mcujs_rp2_pin_owner(int pin) {
    return mcujs_rp2_gpio_pin_allowed(pin) ? s_pin_owners[pin]
                                           : MCUJS_RP2_PIN_OWNER_NONE;
}

bool mcujs_rp2_pin_can_claim(int pin, mcujs_rp2_pin_owner_t owner) {
    if (!mcujs_rp2_gpio_pin_allowed(pin) || owner == MCUJS_RP2_PIN_OWNER_NONE) {
        return false;
    }
    mcujs_rp2_pin_owner_t current = s_pin_owners[pin];
    /* GPIO is deliberately a soft owner. A peripheral may take over a pin
     * after validating its complete route, while active peripheral owners
     * remain exclusive until their binding tears them down. */
    return current == MCUJS_RP2_PIN_OWNER_NONE || current == owner ||
           current == MCUJS_RP2_PIN_OWNER_GPIO;
}

bool mcujs_rp2_pin_claim(int pin, mcujs_rp2_pin_owner_t owner) {
    if (!mcujs_rp2_pin_can_claim(pin, owner)) return false;
    s_pin_owners[pin] = owner;
    return true;
}

void mcujs_rp2_pin_release(int pin, mcujs_rp2_pin_owner_t owner) {
    if (mcujs_rp2_gpio_pin_allowed(pin) && s_pin_owners[pin] == owner) {
        s_pin_owners[pin] = MCUJS_RP2_PIN_OWNER_NONE;
    }
}
