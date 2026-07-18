#include "pin_policy.h"

static mcujs_pin_owner_t s_pin_owners[GPIO_NUM_MAX];

bool mcujs_pin_is_exposed(int pin) {
    if (!GPIO_IS_VALID_GPIO((gpio_num_t)pin)) {
        return false;
    }
    return (pin >= 1 && pin <= 9) || pin == 21;
}

bool mcujs_pin_is_exposed_output(int pin) {
    return mcujs_pin_is_exposed(pin) && GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)pin);
}

bool mcujs_pin_is_peripheral(int pin) {
    return pin >= 1 && pin <= 9 && GPIO_IS_VALID_GPIO((gpio_num_t)pin);
}

bool mcujs_pin_is_peripheral_output(int pin) {
    return mcujs_pin_is_peripheral(pin) && GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)pin);
}

mcujs_pin_owner_t mcujs_pin_owner(int pin) {
    return mcujs_pin_is_exposed(pin) ? s_pin_owners[pin] : MCUJS_PIN_OWNER_NONE;
}

bool mcujs_pin_can_claim(int pin, mcujs_pin_owner_t owner) {
    if (!mcujs_pin_is_exposed(pin) || owner == MCUJS_PIN_OWNER_NONE) {
        return false;
    }
    mcujs_pin_owner_t current = s_pin_owners[pin];
    return current == MCUJS_PIN_OWNER_NONE || current == owner ||
           current == MCUJS_PIN_OWNER_GPIO || current == MCUJS_PIN_OWNER_ADC;
}

bool mcujs_pin_claim(int pin, mcujs_pin_owner_t owner) {
    if (!mcujs_pin_can_claim(pin, owner)) {
        return false;
    }
    s_pin_owners[pin] = owner;
    return true;
}

void mcujs_pin_release(int pin, mcujs_pin_owner_t owner) {
    if (mcujs_pin_is_exposed(pin) && s_pin_owners[pin] == owner) {
        s_pin_owners[pin] = MCUJS_PIN_OWNER_NONE;
    }
}

bool mcujs_pin_gpio_access_allowed(int pin) {
    if (!mcujs_pin_is_exposed(pin)) {
        return false;
    }
    mcujs_pin_owner_t owner = s_pin_owners[pin];
    return owner == MCUJS_PIN_OWNER_NONE || owner == MCUJS_PIN_OWNER_GPIO ||
           owner == MCUJS_PIN_OWNER_ADC;
}
