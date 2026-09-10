#ifndef MCUJS_RP2_PIN_POLICY_H
#define MCUJS_RP2_PIN_POLICY_H

#include "runtime_features.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MCUJS_RP2_PIN_OWNER_NONE = 0,
    MCUJS_RP2_PIN_OWNER_GPIO,
    MCUJS_RP2_PIN_OWNER_PWM,
    MCUJS_RP2_PIN_OWNER_I2C0,
    MCUJS_RP2_PIN_OWNER_I2C1,
    MCUJS_RP2_PIN_OWNER_SPI0,
    MCUJS_RP2_PIN_OWNER_SPI1,
    MCUJS_RP2_PIN_OWNER_ADC,
    MCUJS_RP2_PIN_OWNER_NEOPIXEL,
    MCUJS_RP2_PIN_OWNER_TOUCH,
} mcujs_rp2_pin_owner_t;

static inline bool mcujs_rp2_pin_in_mask(int pin, uint64_t mask) {
    return pin >= 0 && pin < 64 && (mask & (1ULL << (unsigned)pin)) != 0;
}

static inline bool mcujs_rp2_gpio_pin_allowed(int pin) {
    return mcujs_rp2_pin_in_mask(pin, MCUJS_RUNTIME_GPIO_PIN_MASK);
}

static inline bool mcujs_rp2_gpio_output_pin_allowed(int pin) {
    return mcujs_rp2_pin_in_mask(pin, MCUJS_RUNTIME_GPIO_OUTPUT_PIN_MASK);
}

static inline bool mcujs_rp2_pwm_pin_allowed(int pin) {
    return mcujs_rp2_pin_in_mask(pin, MCUJS_RUNTIME_PWM_PIN_MASK);
}

static inline bool mcujs_rp2_adc_pin_allowed(int pin) {
    return mcujs_rp2_pin_in_mask(pin, MCUJS_RUNTIME_ADC_PIN_MASK);
}

static inline bool mcujs_rp2_adc_channel_allowed(int channel) {
    return mcujs_rp2_pin_in_mask(channel, MCUJS_RUNTIME_ADC_CHANNEL_MASK);
}

static inline bool mcujs_rp2_neopixel_pin_allowed(int pin) {
    return mcujs_rp2_pin_in_mask(pin, MCUJS_RUNTIME_NEOPIXEL_PIN_MASK);
}

mcujs_rp2_pin_owner_t mcujs_rp2_pin_owner(int pin);
bool mcujs_rp2_pin_can_claim(int pin, mcujs_rp2_pin_owner_t owner);
bool mcujs_rp2_pin_claim(int pin, mcujs_rp2_pin_owner_t owner);
void mcujs_rp2_pin_release(int pin, mcujs_rp2_pin_owner_t owner);

#endif /* MCUJS_RP2_PIN_POLICY_H */
