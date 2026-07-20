#ifndef MCUJS_TEST_LED_STRIP_RMT_H
#define MCUJS_TEST_LED_STRIP_RMT_H

#include "led_strip.h"

#include <stdint.h>

#define RMT_CLK_SRC_DEFAULT 0

typedef struct {
    int clk_src;
    uint32_t resolution_hz;
    uint32_t mem_block_symbols;
    struct {
        unsigned with_dma : 1;
    } flags;
} led_strip_rmt_config_t;

esp_err_t led_strip_new_rmt_device(const led_strip_config_t *strip_config,
                                   const led_strip_rmt_config_t *rmt_config,
                                   led_strip_handle_t *strip);

#endif /* MCUJS_TEST_LED_STRIP_RMT_H */
