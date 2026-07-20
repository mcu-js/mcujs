#include "bindings.h"
#include "runtime_validation_backend_stubs.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int mcujs_test_i2c_result;
unsigned mcujs_test_i2c_write_calls;
unsigned mcujs_test_i2c_read_calls;
size_t mcujs_test_i2c_last_length;

void js_set_property(jerry_value_t object, const char *name, jerry_value_t value) {
    jerry_value_t result = jerry_object_set_sz(object, name, value);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
}

void js_set_function(jerry_value_t object, const char *name,
                     jerry_external_handler_t handler) {
    jerry_value_t function = jerry_function_external(handler);
    js_set_property(object, name, function);
    jerry_value_free(function);
}

void js_set_number(jerry_value_t object, const char *name, double value) {
    jerry_value_t number = jerry_number(value);
    js_set_property(object, name, number);
    jerry_value_free(number);
}

void js_register_global(const char *name, jerry_value_t object) {
    jerry_value_t global = jerry_current_realm();
    js_set_property(global, name, object);
    jerry_value_free(global);
}

double js_get_number_arg(const jerry_value_t args[], jerry_length_t argc,
                         jerry_length_t index, double default_value) {
    if (index >= argc || !jerry_value_is_number(args[index])) return default_value;
    return jerry_value_as_number(args[index]);
}

bool js_get_boolean_arg(const jerry_value_t args[], jerry_length_t argc,
                        jerry_length_t index, bool default_value) {
    if (index >= argc || !jerry_value_is_boolean(args[index])) return default_value;
    return jerry_value_is_true(args[index]);
}

#if defined(MCUJS_PLATFORM_RP2)

#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

static i2c_inst_t s_i2c_instances[] = {{.index = 0}, {.index = 1}};
i2c_inst_t *i2c0 = &s_i2c_instances[0];
i2c_inst_t *i2c1 = &s_i2c_instances[1];
static bool s_gpio_levels[NUM_BANK0_GPIOS];
unsigned mcujs_test_gpio_init_calls;
unsigned mcujs_test_i2c_init_calls;
unsigned mcujs_test_pwm_config_calls;
unsigned mcujs_test_pwm_divider_scaled;
unsigned mcujs_test_pwm_wrap;

void mcujs_test_reset_backend(void) {
    mcujs_test_i2c_result = 0;
    mcujs_test_i2c_write_calls = 0;
    mcujs_test_i2c_read_calls = 0;
    mcujs_test_i2c_last_length = 0;
    mcujs_test_gpio_init_calls = 0;
    mcujs_test_i2c_init_calls = 0;
    mcujs_test_pwm_config_calls = 0;
    mcujs_test_pwm_divider_scaled = 0;
    mcujs_test_pwm_wrap = 0;
    for (size_t i = 0; i < NUM_BANK0_GPIOS; i++) s_gpio_levels[i] = false;
}

void gpio_init(uint pin) { (void)pin; mcujs_test_gpio_init_calls++; }
void gpio_set_dir(uint pin, bool output) { (void)pin; (void)output; }
void gpio_disable_pulls(uint pin) { (void)pin; }
void gpio_pull_up(uint pin) { (void)pin; }
void gpio_pull_down(uint pin) { (void)pin; }
void gpio_put(uint pin, bool value) { if (pin < NUM_BANK0_GPIOS) s_gpio_levels[pin] = value; }
bool gpio_get(uint pin) { return pin < NUM_BANK0_GPIOS && s_gpio_levels[pin]; }
void gpio_set_function(uint pin, uint function) { (void)pin; (void)function; }

uint i2c_init(i2c_inst_t *instance, uint baudrate) {
    (void)instance;
    mcujs_test_i2c_init_calls++;
    return baudrate;
}

int i2c_write_blocking(i2c_inst_t *instance, uint8_t address,
                       const uint8_t *data, size_t length, bool nostop) {
    (void)instance;
    (void)address;
    (void)data;
    (void)nostop;
    mcujs_test_i2c_write_calls++;
    mcujs_test_i2c_last_length = length;
    return mcujs_test_i2c_result != 0 ? mcujs_test_i2c_result : (int)length;
}

int i2c_read_blocking(i2c_inst_t *instance, uint8_t address,
                      uint8_t *data, size_t length, bool nostop) {
    (void)instance;
    (void)address;
    (void)nostop;
    mcujs_test_i2c_read_calls++;
    mcujs_test_i2c_last_length = length;
    if (mcujs_test_i2c_result != 0) return mcujs_test_i2c_result;
    for (size_t i = 0; i < length; i++) data[i] = (uint8_t)i;
    return (int)length;
}

uint pwm_gpio_to_slice_num(uint pin) { return pin / 2u; }
void pwm_set_clkdiv_int_frac(uint slice, uint8_t div_int, uint8_t div_frac4) {
    (void)slice;
    mcujs_test_pwm_divider_scaled = (unsigned)div_int * 16u + div_frac4;
    mcujs_test_pwm_config_calls++;
}
void pwm_set_wrap(uint slice, uint16_t wrap) {
    (void)slice;
    mcujs_test_pwm_wrap = wrap;
}
void pwm_set_gpio_level(uint pin, uint16_t level) { (void)pin; (void)level; }
void pwm_set_enabled(uint slice, bool enabled) { (void)slice; (void)enabled; }
uint32_t clock_get_hz(int clock) { (void)clock; return 125000000u; }

#elif defined(MCUJS_PLATFORM_ESP32)

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"

static int s_gpio_levels[GPIO_NUM_MAX];
unsigned mcujs_test_i2c_param_config_calls;
unsigned mcujs_test_i2c_driver_install_calls;
unsigned mcujs_test_i2c_driver_delete_calls;
int mcujs_test_ledc_result;
unsigned mcujs_test_ledc_resolution;
unsigned mcujs_test_ledc_actual_frequency;
static uint32_t s_ledc_timer_frequency[4];

void mcujs_test_reset_backend(void) {
    mcujs_test_i2c_result = ESP_OK;
    mcujs_test_i2c_write_calls = 0;
    mcujs_test_i2c_read_calls = 0;
    mcujs_test_i2c_last_length = 0;
    mcujs_test_i2c_param_config_calls = 0;
    mcujs_test_i2c_driver_install_calls = 0;
    mcujs_test_i2c_driver_delete_calls = 0;
    mcujs_test_ledc_result = ESP_OK;
    mcujs_test_ledc_resolution = 10;
    mcujs_test_ledc_actual_frequency = 0;
    for (size_t i = 0; i < 4; i++) s_ledc_timer_frequency[i] = 0;
    for (size_t i = 0; i < GPIO_NUM_MAX; i++) s_gpio_levels[i] = 0;
}

esp_err_t gpio_reset_pin(gpio_num_t pin) { (void)pin; return ESP_OK; }
esp_err_t gpio_set_direction(gpio_num_t pin, int mode) { (void)pin; (void)mode; return ESP_OK; }
esp_err_t gpio_set_pull_mode(gpio_num_t pin, int mode) { (void)pin; (void)mode; return ESP_OK; }
esp_err_t gpio_set_level(gpio_num_t pin, int level) {
    if (pin >= 0 && pin < GPIO_NUM_MAX) s_gpio_levels[pin] = level;
    return ESP_OK;
}
int gpio_get_level(gpio_num_t pin) {
    return pin >= 0 && pin < GPIO_NUM_MAX ? s_gpio_levels[pin] : 0;
}

esp_err_t i2c_param_config(i2c_port_t port, const i2c_config_t *config) {
    (void)port;
    (void)config;
    mcujs_test_i2c_param_config_calls++;
    return ESP_OK;
}
esp_err_t i2c_driver_install(i2c_port_t port, int mode, size_t rx_buffer_length,
                             size_t tx_buffer_length, int interrupt_flags) {
    (void)port;
    (void)mode;
    (void)rx_buffer_length;
    (void)tx_buffer_length;
    (void)interrupt_flags;
    mcujs_test_i2c_driver_install_calls++;
    return ESP_OK;
}
esp_err_t i2c_driver_delete(i2c_port_t port) {
    (void)port;
    mcujs_test_i2c_driver_delete_calls++;
    return ESP_OK;
}
esp_err_t i2c_master_write_to_device(i2c_port_t port, uint8_t address,
                                     const uint8_t *data, size_t length,
                                     uint32_t timeout_ticks) {
    (void)port;
    (void)address;
    (void)data;
    (void)timeout_ticks;
    mcujs_test_i2c_write_calls++;
    mcujs_test_i2c_last_length = length;
    return mcujs_test_i2c_result;
}
esp_err_t i2c_master_read_from_device(i2c_port_t port, uint8_t address,
                                      uint8_t *data, size_t length,
                                      uint32_t timeout_ticks) {
    (void)port;
    (void)address;
    (void)timeout_ticks;
    mcujs_test_i2c_read_calls++;
    mcujs_test_i2c_last_length = length;
    if (mcujs_test_i2c_result != ESP_OK) return mcujs_test_i2c_result;
    for (size_t i = 0; i < length; i++) data[i] = (uint8_t)i;
    return ESP_OK;
}

esp_err_t ledc_timer_pause(int speed_mode, ledc_timer_t timer) {
    (void)speed_mode;
    (void)timer;
    return mcujs_test_ledc_result;
}

esp_err_t ledc_timer_config(const ledc_timer_config_t *config) {
    if (mcujs_test_ledc_result == ESP_OK && !config->deconfigure &&
        config->timer_num >= 0 && config->timer_num < 4) {
        s_ledc_timer_frequency[config->timer_num] = config->freq_hz;
    }
    return mcujs_test_ledc_result;
}

uint32_t ledc_get_freq(int speed_mode, ledc_timer_t timer) {
    (void)speed_mode;
    if (mcujs_test_ledc_actual_frequency != 0) {
        return mcujs_test_ledc_actual_frequency;
    }
    return timer >= 0 && timer < 4 ? s_ledc_timer_frequency[timer] : 0;
}

esp_err_t ledc_stop(int speed_mode, ledc_channel_t channel,
                    uint32_t idle_level) {
    (void)speed_mode;
    (void)channel;
    (void)idle_level;
    return mcujs_test_ledc_result;
}

uint32_t ledc_find_suitable_duty_resolution(uint32_t source_clock,
                                            uint32_t frequency) {
    (void)source_clock;
    (void)frequency;
    return mcujs_test_ledc_resolution;
}

esp_err_t ledc_channel_config(const ledc_channel_config_t *config) {
    (void)config;
    return mcujs_test_ledc_result;
}

esp_err_t ledc_set_duty(int speed_mode, ledc_channel_t channel,
                        uint32_t duty) {
    (void)speed_mode;
    (void)channel;
    (void)duty;
    return mcujs_test_ledc_result;
}

esp_err_t ledc_update_duty(int speed_mode, ledc_channel_t channel) {
    (void)speed_mode;
    (void)channel;
    return mcujs_test_ledc_result;
}

#else
#error "backend stubs require an explicit platform lane"
#endif
