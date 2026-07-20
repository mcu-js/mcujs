#ifndef MCUJS_RP2_GPIO_INTERNAL_H
#define MCUJS_RP2_GPIO_INTERNAL_H

#include <stdbool.h>

/* Claim a board-safe pin as soft GPIO output and initialize/reinitialize its
 * native SIO state. Returns false while a hard peripheral owner is active. */
bool mcujs_rp2_gpio_prepare_soft_output(int pin);

#endif
