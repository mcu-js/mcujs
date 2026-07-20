#ifndef MCUJS_TEST_ESP_LEDC_H
#define MCUJS_TEST_ESP_LEDC_H

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

typedef int ledc_timer_bit_t;
typedef int ledc_channel_t;
typedef int ledc_timer_t;

#define LEDC_LOW_SPEED_MODE 0
#define LEDC_AUTO_CLK 0
#define LEDC_INTR_DISABLE 0

typedef struct {
    int speed_mode;
    ledc_timer_bit_t duty_resolution;
    ledc_timer_t timer_num;
    uint32_t freq_hz;
    int clk_cfg;
    bool deconfigure;
} ledc_timer_config_t;

typedef struct {
    int gpio_num;
    int speed_mode;
    ledc_channel_t channel;
    int intr_type;
    ledc_timer_t timer_sel;
    uint32_t duty;
    uint32_t hpoint;
    struct {
        unsigned output_invert : 1;
    } flags;
} ledc_channel_config_t;

esp_err_t ledc_timer_pause(int speed_mode, ledc_timer_t timer);
esp_err_t ledc_timer_config(const ledc_timer_config_t *config);
uint32_t ledc_get_freq(int speed_mode, ledc_timer_t timer);
esp_err_t ledc_stop(int speed_mode, ledc_channel_t channel, uint32_t idle_level);
uint32_t ledc_find_suitable_duty_resolution(uint32_t source_clock,
                                            uint32_t frequency);
esp_err_t ledc_channel_config(const ledc_channel_config_t *config);
esp_err_t ledc_set_duty(int speed_mode, ledc_channel_t channel, uint32_t duty);
esp_err_t ledc_update_duty(int speed_mode, ledc_channel_t channel);

#endif
