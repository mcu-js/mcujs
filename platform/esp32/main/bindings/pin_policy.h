#ifndef MCUJS_ESP32_PIN_POLICY_H
#define MCUJS_ESP32_PIN_POLICY_H

#include <stdbool.h>

#include "driver/gpio.h"

typedef enum {
    MCUJS_PIN_OWNER_NONE = 0,
    MCUJS_PIN_OWNER_GPIO,
    MCUJS_PIN_OWNER_ADC,
    MCUJS_PIN_OWNER_PWM,
    MCUJS_PIN_OWNER_I2C0,
    MCUJS_PIN_OWNER_I2C1,
    MCUJS_PIN_OWNER_SPI0,
    MCUJS_PIN_OWNER_SPI1,
    MCUJS_PIN_OWNER_NEOPIXEL,
} mcujs_pin_owner_t;

/* XIAO exposed D0-D5 and D8-D10 pins plus the onboard discrete LED. D6/D7
 * (GPIO43/44) remain reserved for the independent UART recovery console. */
bool mcujs_pin_is_exposed(int pin);
bool mcujs_pin_is_exposed_output(int pin);

/* External peripheral modules use header pins only. GPIO21 is the onboard
 * discrete LED and is intentionally excluded from peripheral claims. */
bool mcujs_pin_is_peripheral(int pin);
bool mcujs_pin_is_peripheral_output(int pin);

mcujs_pin_owner_t mcujs_pin_owner(int pin);
bool mcujs_pin_can_claim(int pin, mcujs_pin_owner_t owner);
bool mcujs_pin_claim(int pin, mcujs_pin_owner_t owner);
void mcujs_pin_release(int pin, mcujs_pin_owner_t owner);
bool mcujs_pin_gpio_access_allowed(int pin);

#endif /* MCUJS_ESP32_PIN_POLICY_H */
