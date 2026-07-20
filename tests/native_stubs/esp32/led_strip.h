#ifndef MCUJS_TEST_LED_STRIP_H
#define MCUJS_TEST_LED_STRIP_H

#include "esp_err.h"

#include <stdint.h>

typedef struct mcujs_test_led_strip *led_strip_handle_t;

typedef enum {
    LED_MODEL_WS2812 = 0,
} led_model_t;

typedef enum {
    LED_STRIP_COLOR_COMPONENT_FMT_RGB = 0,
    LED_STRIP_COLOR_COMPONENT_FMT_GRB = 1,
} led_color_component_format_t;

typedef struct {
    int strip_gpio_num;
    uint32_t max_leds;
    led_model_t led_model;
    led_color_component_format_t color_component_format;
    struct {
        unsigned invert_out : 1;
    } flags;
} led_strip_config_t;

esp_err_t led_strip_set_pixel(led_strip_handle_t strip, uint32_t index,
                              uint32_t red, uint32_t green, uint32_t blue);
esp_err_t led_strip_refresh(led_strip_handle_t strip);
esp_err_t led_strip_clear(led_strip_handle_t strip);
esp_err_t led_strip_del(led_strip_handle_t strip);

#endif /* MCUJS_TEST_LED_STRIP_H */
