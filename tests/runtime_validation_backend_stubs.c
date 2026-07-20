#include "bindings.h"
#include "runtime_validation_backend_stubs.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(MCUJS_PLATFORM_RP2)
#include "pico/unique_id.h"
#endif

int mcujs_test_i2c_result;
unsigned mcujs_test_i2c_write_calls;
unsigned mcujs_test_i2c_read_calls;
size_t mcujs_test_i2c_last_length;

#if !defined(MCUJS_USE_PRODUCTION_BINDING_HELPERS)
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

void js_set_string(jerry_value_t object, const char *name, const char *value) {
    jerry_value_t string = jerry_string_sz(value);
    js_set_property(object, name, string);
    jerry_value_free(string);
}

jerry_value_t js_deep_freeze(jerry_value_t value) {
    (void)value;
    return jerry_undefined();
}

jerry_value_t js_define_immutable_property(jerry_value_t object,
                                           const char *name,
                                           jerry_value_t value) {
    return jerry_object_set_sz(object, name, value);
}

bool js_board_apply_registry(jerry_value_t board,
                             jerry_external_handler_t safe_mode_handler,
                             jerry_external_handler_t storage_ready_handler) {
    (void)board;
    (void)safe_mode_handler;
    (void)storage_ready_handler;
    return true;
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
#endif

#if defined(MCUJS_PLATFORM_RP2)

#include "graphics.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"
#include "neopixel.pio.h"
#include "pico/stdlib.h"

static i2c_inst_t s_i2c_instances[] = {{.index = 0}, {.index = 1}};
i2c_inst_t *i2c0 = &s_i2c_instances[0];
i2c_inst_t *i2c1 = &s_i2c_instances[1];
static bool s_gpio_levels[NUM_BANK0_GPIOS];
unsigned mcujs_test_gpio_init_calls;
int mcujs_test_i2c_init_result;
unsigned mcujs_test_i2c_init_calls;
unsigned mcujs_test_pwm_config_calls;
unsigned mcujs_test_pwm_divider_scaled;
unsigned mcujs_test_pwm_wrap;
unsigned mcujs_test_pwm_duty_calls;
unsigned mcujs_test_pwm_level;
int mcujs_test_spi_init_result;
unsigned mcujs_test_spi_init_calls;
unsigned mcujs_test_spi_deinit_calls;
unsigned mcujs_test_adc_gpio_init_calls;
unsigned mcujs_test_adc_last_pin;
unsigned mcujs_test_adc_init_calls;
unsigned mcujs_test_adc_select_calls;
unsigned mcujs_test_adc_read_calls;
bool mcujs_test_pio_can_add_program;
unsigned mcujs_test_neopixel_init_calls;
unsigned mcujs_test_neopixel_last_pin;
unsigned mcujs_test_neopixel_write_calls;
unsigned mcujs_test_neopixel_last_word;
int mcujs_test_dma_claim_result;
unsigned mcujs_test_dma_claim_calls;
bool mcujs_test_dma_claim_required;
unsigned mcujs_test_dma_configure_calls;
size_t mcujs_test_dma_last_length;

static spi_inst_t s_spi_instances[] = {{.index = 0}, {.index = 1}};
spi_inst_t *spi0 = &s_spi_instances[0];
spi_inst_t *spi1 = &s_spi_instances[1];
static spi_hw_t s_spi_hardware[2];
struct pio_hw mcujs_test_pio0 = {.index = 0};
const pio_program_t mcujs_ws2812_program = {
    .instructions = NULL,
    .length = 1,
    .origin = -1,
};

void mcujs_test_reset_backend(void) {
    mcujs_test_i2c_result = 0;
    mcujs_test_i2c_write_calls = 0;
    mcujs_test_i2c_read_calls = 0;
    mcujs_test_i2c_last_length = 0;
    mcujs_test_gpio_init_calls = 0;
    mcujs_test_i2c_init_result = -1;
    mcujs_test_i2c_init_calls = 0;
    mcujs_test_pwm_config_calls = 0;
    mcujs_test_pwm_divider_scaled = 0;
    mcujs_test_pwm_wrap = 0;
    mcujs_test_pwm_duty_calls = 0;
    mcujs_test_pwm_level = 0;
    mcujs_test_spi_init_result = -1;
    mcujs_test_spi_init_calls = 0;
    mcujs_test_spi_deinit_calls = 0;
    mcujs_test_adc_gpio_init_calls = 0;
    mcujs_test_adc_last_pin = UINT32_MAX;
    mcujs_test_adc_init_calls = 0;
    mcujs_test_adc_select_calls = 0;
    mcujs_test_adc_read_calls = 0;
    mcujs_test_pio_can_add_program = true;
    mcujs_test_neopixel_init_calls = 0;
    mcujs_test_neopixel_last_pin = UINT32_MAX;
    mcujs_test_neopixel_write_calls = 0;
    mcujs_test_neopixel_last_word = 0;
    mcujs_test_dma_claim_result = 0;
    mcujs_test_dma_claim_calls = 0;
    mcujs_test_dma_claim_required = false;
    mcujs_test_dma_configure_calls = 0;
    mcujs_test_dma_last_length = 0;
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
    return mcujs_test_i2c_init_result >= 0
        ? (uint)mcujs_test_i2c_init_result
        : baudrate;
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

uint pwm_gpio_to_slice_num(uint pin) {
    /* Pico SDK 2.2.0 PWM_GPIO_SLICE_NUM mapping for RP2040 and RP2350. */
    return pin < 32u ? ((pin >> 1u) & 7u)
                     : 8u + ((pin >> 1u) & 3u);
}
uint pwm_gpio_to_channel(uint pin) { return pin & 1u; }
void pwm_set_clkdiv_int_frac(uint slice, uint8_t div_int, uint8_t div_frac4) {
    (void)slice;
    mcujs_test_pwm_divider_scaled = (unsigned)div_int * 16u + div_frac4;
    mcujs_test_pwm_config_calls++;
}
void pwm_set_wrap(uint slice, uint16_t wrap) {
    (void)slice;
    mcujs_test_pwm_wrap = wrap;
}
void pwm_set_gpio_level(uint pin, uint16_t level) {
    (void)pin;
    mcujs_test_pwm_duty_calls++;
    mcujs_test_pwm_level = level;
}
void pwm_set_enabled(uint slice, bool enabled) { (void)slice; (void)enabled; }
uint32_t clock_get_hz(int clock) { (void)clock; return 125000000u; }

uint spi_init(spi_inst_t *instance, uint baudrate) {
    (void)instance;
    mcujs_test_spi_init_calls++;
    return mcujs_test_spi_init_result >= 0
        ? (uint)mcujs_test_spi_init_result
        : baudrate;
}
void spi_deinit(spi_inst_t *instance) {
    (void)instance;
    mcujs_test_spi_deinit_calls++;
}
void spi_set_format(spi_inst_t *instance, uint data_bits, int cpol, int cpha,
                    int order) {
    (void)instance;
    (void)data_bits;
    (void)cpol;
    (void)cpha;
    (void)order;
}
int spi_write_read_blocking(spi_inst_t *instance, const uint8_t *tx,
                            uint8_t *rx, size_t length) {
    (void)instance;
    for (size_t i = 0; i < length; i++) rx[i] = tx[i];
    return (int)length;
}
uint spi_get_dreq(spi_inst_t *instance, bool is_tx) {
    (void)instance;
    (void)is_tx;
    return 0;
}
spi_hw_t *spi_get_hw(spi_inst_t *instance) {
    return &s_spi_hardware[instance == spi1 ? 1 : 0];
}
bool spi_is_busy(spi_inst_t *instance) { (void)instance; return false; }

int dma_claim_unused_channel(bool required) {
    mcujs_test_dma_claim_calls++;
    mcujs_test_dma_claim_required = required;
    return mcujs_test_dma_claim_result;
}
dma_channel_config dma_channel_get_default_config(int channel) {
    (void)channel;
    return (dma_channel_config){0};
}
void channel_config_set_transfer_data_size(dma_channel_config *config, int size) {
    (void)config;
    (void)size;
}
void channel_config_set_dreq(dma_channel_config *config, unsigned dreq) {
    (void)config;
    (void)dreq;
}
void channel_config_set_read_increment(dma_channel_config *config, bool increment) {
    (void)config;
    (void)increment;
}
void channel_config_set_write_increment(dma_channel_config *config, bool increment) {
    (void)config;
    (void)increment;
}
void dma_channel_configure(int channel, const dma_channel_config *config,
                           volatile void *write_address, const void *read_address,
                           size_t transfer_count, bool trigger) {
    (void)channel;
    (void)config;
    (void)write_address;
    (void)read_address;
    (void)trigger;
    mcujs_test_dma_configure_calls++;
    mcujs_test_dma_last_length = transfer_count;
}
void dma_channel_wait_for_finish_blocking(int channel) { (void)channel; }

static uint16_t s_graphics_buffer[1];
uint16_t *graphics_get_buffer_data(graphics_buffer_handle_t handle) {
    return handle == 1 ? s_graphics_buffer : NULL;
}
uint32_t graphics_get_buffer_byte_length(graphics_buffer_handle_t handle) {
    return handle == 1 ? sizeof(s_graphics_buffer) : 0;
}

void adc_init(void) { mcujs_test_adc_init_calls++; }
void adc_gpio_init(uint pin) {
    mcujs_test_adc_gpio_init_calls++;
    mcujs_test_adc_last_pin = pin;
}
void adc_select_input(uint channel) { (void)channel; mcujs_test_adc_select_calls++; }
uint16_t adc_read(void) { mcujs_test_adc_read_calls++; return 2048; }
void adc_set_temp_sensor_enabled(bool enabled) { (void)enabled; }

bool pio_can_add_program(PIO pio, const pio_program_t *program) {
    (void)pio;
    (void)program;
    return mcujs_test_pio_can_add_program;
}
uint pio_add_program(PIO pio, const pio_program_t *program) {
    (void)pio;
    (void)program;
    return 0;
}
void pio_sm_put_blocking(PIO pio, uint state_machine, uint32_t data) {
    (void)pio;
    (void)state_machine;
    mcujs_test_neopixel_write_calls++;
    mcujs_test_neopixel_last_word = data;
}
void pio_sm_set_enabled(PIO pio, uint state_machine, bool enabled) {
    (void)pio;
    (void)state_machine;
    (void)enabled;
}
void mcujs_ws2812_program_init(PIO pio, uint state_machine, uint offset,
                               uint pin, float frequency, bool rgbw) {
    (void)pio;
    (void)state_machine;
    (void)offset;
    (void)frequency;
    (void)rgbw;
    mcujs_test_neopixel_init_calls++;
    mcujs_test_neopixel_last_pin = pin;
}
void sleep_us(uint64_t microseconds) { (void)microseconds; }
void sleep_ms(uint32_t milliseconds) { (void)milliseconds; }
void tight_loop_contents(void) {}
absolute_time_t get_absolute_time(void) { return 1234; }
uint64_t to_ms_since_boot(absolute_time_t time) { return time; }
void pico_get_unique_board_id(pico_unique_board_id_t *id) {
    for (size_t i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        id->id[i] = (uint8_t)i;
    }
}
void usb_cdc_reset_usb(uint32_t delay_ms) { (void)delay_ms; }
void board_enter_uf2(void) {}
bool fs_host_owned(void) { return false; }
void cyw43_arch_gpio_put(int pin, int value) { (void)pin; (void)value; }

#elif defined(MCUJS_PLATFORM_ESP32)

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"

static int s_gpio_levels[GPIO_NUM_MAX];
int mcujs_test_gpio_result;
unsigned mcujs_test_i2c_param_config_calls;
unsigned mcujs_test_i2c_driver_install_calls;
unsigned mcujs_test_i2c_driver_delete_calls;
int mcujs_test_ledc_result;
unsigned mcujs_test_ledc_resolution;
unsigned mcujs_test_ledc_configured_resolution;
unsigned mcujs_test_ledc_actual_frequency;
unsigned mcujs_test_ledc_stop_calls;
unsigned mcujs_test_gpio_reset_calls;
unsigned mcujs_test_ledc_duty_calls;
unsigned mcujs_test_ledc_duty;
static uint32_t s_ledc_timer_frequency[4];

void mcujs_test_reset_backend(void) {
    mcujs_test_i2c_result = ESP_OK;
    mcujs_test_i2c_write_calls = 0;
    mcujs_test_i2c_read_calls = 0;
    mcujs_test_i2c_last_length = 0;
    mcujs_test_gpio_result = ESP_OK;
    mcujs_test_i2c_param_config_calls = 0;
    mcujs_test_i2c_driver_install_calls = 0;
    mcujs_test_i2c_driver_delete_calls = 0;
    mcujs_test_ledc_result = ESP_OK;
    mcujs_test_ledc_resolution = 10;
    mcujs_test_ledc_configured_resolution = 0;
    mcujs_test_ledc_actual_frequency = 0;
    mcujs_test_ledc_stop_calls = 0;
    mcujs_test_gpio_reset_calls = 0;
    mcujs_test_ledc_duty_calls = 0;
    mcujs_test_ledc_duty = 0;
    for (size_t i = 0; i < 4; i++) s_ledc_timer_frequency[i] = 0;
    for (size_t i = 0; i < GPIO_NUM_MAX; i++) s_gpio_levels[i] = 0;
}

esp_err_t gpio_reset_pin(gpio_num_t pin) {
    (void)pin;
    mcujs_test_gpio_reset_calls++;
    return mcujs_test_gpio_result;
}
esp_err_t gpio_set_direction(gpio_num_t pin, int mode) {
    (void)pin;
    (void)mode;
    return mcujs_test_gpio_result;
}
esp_err_t gpio_set_pull_mode(gpio_num_t pin, int mode) {
    (void)pin;
    (void)mode;
    return mcujs_test_gpio_result;
}
esp_err_t gpio_set_level(gpio_num_t pin, int level) {
    if (mcujs_test_gpio_result != ESP_OK) return mcujs_test_gpio_result;
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
        mcujs_test_ledc_configured_resolution = config->duty_resolution;
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
    mcujs_test_ledc_stop_calls++;
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
    mcujs_test_ledc_duty_calls++;
    mcujs_test_ledc_duty = duty;
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
