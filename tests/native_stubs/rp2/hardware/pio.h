#ifndef MCUJS_TEST_RP2_PIO_H
#define MCUJS_TEST_RP2_PIO_H

#include "pico/stdlib.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct pio_hw {
    int index;
} *PIO;

typedef struct pio_program {
    const uint16_t *instructions;
    uint length;
    int origin;
} pio_program_t;

extern struct pio_hw mcujs_test_pio0;
#define pio0 (&mcujs_test_pio0)

bool pio_can_add_program(PIO pio, const pio_program_t *program);
uint pio_add_program(PIO pio, const pio_program_t *program);
int pio_claim_unused_sm(PIO pio, bool required);
void pio_sm_unclaim(PIO pio, uint state_machine);
void pio_sm_put_blocking(PIO pio, uint state_machine, uint32_t data);
void pio_sm_set_enabled(PIO pio, uint state_machine, bool enabled);

#endif
