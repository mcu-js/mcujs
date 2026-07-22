#include "bindings.h"
#include "runtime_validation_backend_stubs.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(MCUJS_PLATFORM_RP2)
#include "pico/unique_id.h"
#endif

int mcujs_test_i2c_result;
unsigned mcujs_test_i2c_write_calls;
unsigned mcujs_test_i2c_read_calls;
size_t mcujs_test_i2c_last_length;
unsigned mcujs_test_adc_read_calls;
unsigned mcujs_test_adc_calibrated_calls;
int mcujs_test_adc_raw_value;

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
static bool s_gpio_outputs[NUM_BANK0_GPIOS];
static int s_gpio_pulls[NUM_BANK0_GPIOS];
unsigned mcujs_test_gpio_init_calls;
int mcujs_test_i2c_init_result;
unsigned mcujs_test_i2c_init_calls;
unsigned mcujs_test_i2c_deinit_calls;
int mcujs_test_i2c_last_init_bus;
unsigned mcujs_test_i2c_last_write_timeout_us;
unsigned mcujs_test_i2c_last_read_timeout_us;
unsigned mcujs_test_pwm_config_calls;
unsigned mcujs_test_pwm_divider_scaled;
unsigned mcujs_test_pwm_wrap;
unsigned mcujs_test_pwm_duty_calls;
unsigned mcujs_test_pwm_level;
unsigned mcujs_test_pwm_enable_calls;
unsigned mcujs_test_pwm_disable_calls;
int mcujs_test_spi_init_result;
unsigned mcujs_test_spi_init_calls;
unsigned mcujs_test_spi_deinit_calls;
int mcujs_test_spi_last_init_bus;
unsigned mcujs_test_spi_last_format_bits;
int mcujs_test_spi_last_format_cpol;
int mcujs_test_spi_last_format_cpha;
int mcujs_test_spi_last_format_order;
int mcujs_test_spi_transfer_result;
size_t mcujs_test_spi_last_transfer_length;
unsigned mcujs_test_adc_gpio_init_calls;
unsigned mcujs_test_adc_last_pin;
unsigned mcujs_test_adc_init_calls;
unsigned mcujs_test_adc_select_calls;
bool mcujs_test_pio_can_add_program;
unsigned mcujs_test_pio_claimed_sm_mask;
unsigned mcujs_test_pio_claim_calls;
unsigned mcujs_test_pio_unclaim_calls;
int mcujs_test_pio_last_claimed_sm;
int mcujs_test_pio_last_unclaimed_sm;
unsigned mcujs_test_neopixel_init_calls;
unsigned mcujs_test_neopixel_last_pin;
unsigned mcujs_test_neopixel_last_sm;
unsigned mcujs_test_neopixel_disable_calls;
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
    mcujs_test_i2c_deinit_calls = 0;
    mcujs_test_i2c_last_init_bus = -1;
    mcujs_test_i2c_last_write_timeout_us = 0;
    mcujs_test_i2c_last_read_timeout_us = 0;
    mcujs_test_pwm_config_calls = 0;
    mcujs_test_pwm_divider_scaled = 0;
    mcujs_test_pwm_wrap = 0;
    mcujs_test_pwm_duty_calls = 0;
    mcujs_test_pwm_level = 0;
    mcujs_test_pwm_enable_calls = 0;
    mcujs_test_pwm_disable_calls = 0;
    mcujs_test_spi_init_result = -1;
    mcujs_test_spi_init_calls = 0;
    mcujs_test_spi_deinit_calls = 0;
    mcujs_test_spi_last_init_bus = -1;
    mcujs_test_spi_last_format_bits = 0;
    mcujs_test_spi_last_format_cpol = -1;
    mcujs_test_spi_last_format_cpha = -1;
    mcujs_test_spi_last_format_order = -1;
    mcujs_test_spi_transfer_result = 0;
    mcujs_test_spi_last_transfer_length = 0;
    mcujs_test_adc_gpio_init_calls = 0;
    mcujs_test_adc_last_pin = UINT32_MAX;
    mcujs_test_adc_init_calls = 0;
    mcujs_test_adc_select_calls = 0;
    mcujs_test_adc_read_calls = 0;
    mcujs_test_adc_calibrated_calls = 0;
    mcujs_test_adc_raw_value = 2048;
    mcujs_test_pio_can_add_program = true;
    mcujs_test_pio_claimed_sm_mask = 0;
    mcujs_test_pio_claim_calls = 0;
    mcujs_test_pio_unclaim_calls = 0;
    mcujs_test_pio_last_claimed_sm = -1;
    mcujs_test_pio_last_unclaimed_sm = -1;
    mcujs_test_neopixel_init_calls = 0;
    mcujs_test_neopixel_last_pin = UINT32_MAX;
    mcujs_test_neopixel_last_sm = UINT32_MAX;
    mcujs_test_neopixel_disable_calls = 0;
    mcujs_test_neopixel_write_calls = 0;
    mcujs_test_neopixel_last_word = 0;
    mcujs_test_dma_claim_result = 0;
    mcujs_test_dma_claim_calls = 0;
    mcujs_test_dma_claim_required = false;
    mcujs_test_dma_configure_calls = 0;
    mcujs_test_dma_last_length = 0;
    for (size_t i = 0; i < NUM_BANK0_GPIOS; i++) {
        s_gpio_levels[i] = false;
        s_gpio_outputs[i] = false;
        s_gpio_pulls[i] = 0;
    }
}

bool mcujs_test_gpio_is_output(unsigned pin) {
    return pin < NUM_BANK0_GPIOS && s_gpio_outputs[pin];
}
int mcujs_test_gpio_level_at(unsigned pin) {
    return pin < NUM_BANK0_GPIOS && s_gpio_levels[pin];
}
int mcujs_test_gpio_pull_at(unsigned pin) {
    return pin < NUM_BANK0_GPIOS ? s_gpio_pulls[pin] : -1;
}
void gpio_init(uint pin) {
    mcujs_test_gpio_init_calls++;
    if (pin < NUM_BANK0_GPIOS) {
        s_gpio_outputs[pin] = false;
        s_gpio_levels[pin] = false;
        s_gpio_pulls[pin] = 0;
    }
}
void gpio_set_dir(uint pin, bool output) {
    if (pin < NUM_BANK0_GPIOS) s_gpio_outputs[pin] = output;
}
void gpio_disable_pulls(uint pin) {
    if (pin < NUM_BANK0_GPIOS) s_gpio_pulls[pin] = 0;
}
void gpio_pull_up(uint pin) {
    if (pin < NUM_BANK0_GPIOS) s_gpio_pulls[pin] = 1;
}
void gpio_pull_down(uint pin) {
    if (pin < NUM_BANK0_GPIOS) s_gpio_pulls[pin] = 2;
}
void gpio_put(uint pin, bool value) { if (pin < NUM_BANK0_GPIOS) s_gpio_levels[pin] = value; }
bool gpio_get(uint pin) { return pin < NUM_BANK0_GPIOS && s_gpio_levels[pin]; }
void gpio_set_function(uint pin, uint function) { (void)pin; (void)function; }

uint i2c_init(i2c_inst_t *instance, uint baudrate) {
    mcujs_test_i2c_init_calls++;
    mcujs_test_i2c_last_init_bus = instance->index;
    return mcujs_test_i2c_init_result >= 0
        ? (uint)mcujs_test_i2c_init_result
        : baudrate;
}

void i2c_deinit(i2c_inst_t *instance) {
    (void)instance;
    mcujs_test_i2c_deinit_calls++;
}

int i2c_write_timeout_us(i2c_inst_t *instance, uint8_t address,
                         const uint8_t *data, size_t length, bool nostop,
                         uint timeout_us) {
    (void)instance;
    (void)address;
    (void)data;
    (void)nostop;
    mcujs_test_i2c_write_calls++;
    mcujs_test_i2c_last_length = length;
    mcujs_test_i2c_last_write_timeout_us = timeout_us;
    return mcujs_test_i2c_result != 0 ? mcujs_test_i2c_result : (int)length;
}

int i2c_read_timeout_us(i2c_inst_t *instance, uint8_t address,
                        uint8_t *data, size_t length, bool nostop,
                        uint timeout_us) {
    (void)instance;
    (void)address;
    (void)nostop;
    mcujs_test_i2c_read_calls++;
    mcujs_test_i2c_last_length = length;
    mcujs_test_i2c_last_read_timeout_us = timeout_us;
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
void pwm_set_enabled(uint slice, bool enabled) {
    (void)slice;
    if (enabled) {
        mcujs_test_pwm_enable_calls++;
    } else {
        mcujs_test_pwm_disable_calls++;
    }
}
uint32_t clock_get_hz(int clock) {
    (void)clock;
#if defined(MCUJS_BOARD_PICO2)
    return 150000000u;
#else
    return 125000000u;
#endif
}

uint spi_init(spi_inst_t *instance, uint baudrate) {
    mcujs_test_spi_init_calls++;
    mcujs_test_spi_last_init_bus = instance->index;
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
    mcujs_test_spi_last_format_bits = data_bits;
    mcujs_test_spi_last_format_cpol = cpol;
    mcujs_test_spi_last_format_cpha = cpha;
    mcujs_test_spi_last_format_order = order;
}
int spi_write_read_blocking(spi_inst_t *instance, const uint8_t *tx,
                            uint8_t *rx, size_t length) {
    (void)instance;
    mcujs_test_spi_last_transfer_length = length;
    if (mcujs_test_spi_transfer_result != 0) {
        return mcujs_test_spi_transfer_result;
    }
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
uint16_t adc_read(void) {
    mcujs_test_adc_read_calls++;
    return (uint16_t)mcujs_test_adc_raw_value;
}
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
int pio_claim_unused_sm(PIO pio, bool required) {
    assert(pio == pio0);
    assert(!required);
    mcujs_test_pio_claim_calls++;
    for (int state_machine = 0; state_machine < 4; state_machine++) {
        unsigned mask = 1u << (unsigned)state_machine;
        if ((mcujs_test_pio_claimed_sm_mask & mask) == 0) {
            mcujs_test_pio_claimed_sm_mask |= mask;
            mcujs_test_pio_last_claimed_sm = state_machine;
            return state_machine;
        }
    }
    return -1;
}
void pio_sm_unclaim(PIO pio, uint state_machine) {
    assert(pio == pio0);
    assert(state_machine < 4);
    unsigned mask = 1u << state_machine;
    assert((mcujs_test_pio_claimed_sm_mask & mask) != 0);
    mcujs_test_pio_claimed_sm_mask &= ~mask;
    mcujs_test_pio_unclaim_calls++;
    mcujs_test_pio_last_unclaimed_sm = (int)state_machine;
}
void pio_sm_put_blocking(PIO pio, uint state_machine, uint32_t data) {
    assert(pio == pio0);
    assert(state_machine < 4);
    assert((mcujs_test_pio_claimed_sm_mask & (1u << state_machine)) != 0);
    mcujs_test_neopixel_write_calls++;
    mcujs_test_neopixel_last_word = data;
}
void pio_sm_set_enabled(PIO pio, uint state_machine, bool enabled) {
    assert(pio == pio0);
    assert(state_machine < 4);
    assert((mcujs_test_pio_claimed_sm_mask & (1u << state_machine)) != 0);
    if (!enabled) mcujs_test_neopixel_disable_calls++;
}
void mcujs_ws2812_program_init(PIO pio, uint state_machine, uint offset,
                               uint pin, float frequency, bool rgbw) {
    assert(pio == pio0);
    assert(state_machine < 4);
    assert((mcujs_test_pio_claimed_sm_mask & (1u << state_machine)) != 0);
    (void)offset;
    (void)frequency;
    (void)rgbw;
    mcujs_test_neopixel_init_calls++;
    mcujs_test_neopixel_last_pin = pin;
    mcujs_test_neopixel_last_sm = state_machine;
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
#include "driver/spi_master.h"
#include "driver/temperature_sensor.h"
#include "esp_adc/adc_cali_scheme.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

static int s_gpio_levels[GPIO_NUM_MAX];
static int s_gpio_directions[GPIO_NUM_MAX];
static int s_gpio_pulls[GPIO_NUM_MAX];
int mcujs_test_gpio_result;
unsigned mcujs_test_i2c_param_config_calls;
unsigned mcujs_test_i2c_driver_install_calls;
unsigned mcujs_test_i2c_driver_delete_calls;
int mcujs_test_i2c_param_config_result;
int mcujs_test_i2c_driver_install_result;
int mcujs_test_i2c_driver_delete_result;
int mcujs_test_ledc_result;
unsigned mcujs_test_ledc_resolution;
unsigned mcujs_test_ledc_configured_resolution;
unsigned mcujs_test_ledc_actual_frequency;
unsigned mcujs_test_ledc_stop_calls;
unsigned mcujs_test_gpio_reset_calls;
unsigned mcujs_test_gpio_direction_calls;
unsigned mcujs_test_gpio_pull_calls;
unsigned mcujs_test_gpio_level_calls;
unsigned mcujs_test_ledc_duty_calls;
unsigned mcujs_test_ledc_duty;
int mcujs_test_ledc_timer_result;
int mcujs_test_ledc_timer_deconfigure_result;
int mcujs_test_ledc_channel_result;
int mcujs_test_ledc_stop_result;
int mcujs_test_ledc_pause_result;
int mcujs_test_gpio_reset_result;
unsigned mcujs_test_ledc_timer_config_calls;
unsigned mcujs_test_ledc_timer_deconfigure_calls;
unsigned mcujs_test_ledc_timer_pause_calls;
unsigned mcujs_test_ledc_channel_config_calls;
int mcujs_test_adc_unit_result;
int mcujs_test_adc_config_result;
int mcujs_test_adc_read_result;
int mcujs_test_adc_calibration_result;
int mcujs_test_adc_millivolts;
int mcujs_test_temperature_install_result;
int mcujs_test_temperature_enable_result;
int mcujs_test_temperature_read_result;
int mcujs_test_temperature_disable_result;
float mcujs_test_temperature_celsius;
int mcujs_test_spi_bus_init_result;
int mcujs_test_spi_bus_free_result;
int mcujs_test_spi_add_result;
int mcujs_test_spi_remove_result;
int mcujs_test_spi_transmit_result;
int mcujs_test_spi_last_host;
int mcujs_test_spi_last_frequency;
int mcujs_test_spi_last_mode;
int mcujs_test_spi_actual_frequency;
unsigned mcujs_test_spi_remove_calls;
size_t mcujs_test_spi_last_transfer_length;
int mcujs_test_led_strip_new_result;
int mcujs_test_led_strip_set_result;
int mcujs_test_led_strip_refresh_result;
int mcujs_test_led_strip_clear_result;
int mcujs_test_led_strip_del_result;
unsigned mcujs_test_led_strip_new_calls;
unsigned mcujs_test_led_strip_set_calls;
unsigned mcujs_test_led_strip_refresh_calls;
unsigned mcujs_test_led_strip_clear_calls;
unsigned mcujs_test_led_strip_del_calls;
int mcujs_test_led_strip_last_pin;
unsigned mcujs_test_led_strip_last_length;
int mcujs_test_led_strip_last_order;
unsigned mcujs_test_led_strip_last_index;
unsigned mcujs_test_led_strip_last_red;
unsigned mcujs_test_led_strip_last_green;
unsigned mcujs_test_led_strip_last_blue;
static int s_spi_devices[2];
static uint32_t s_ledc_timer_frequency[4];
static int s_adc_unit_storage;
static int s_adc_cali_storage;
static int s_temperature_sensor_storage;
struct mcujs_test_led_strip {
    bool active;
};
static struct mcujs_test_led_strip s_led_strip_storage;

void mcujs_test_reset_backend(void) {
    mcujs_test_i2c_result = ESP_OK;
    mcujs_test_i2c_write_calls = 0;
    mcujs_test_i2c_read_calls = 0;
    mcujs_test_i2c_last_length = 0;
    mcujs_test_gpio_result = ESP_OK;
    mcujs_test_i2c_param_config_calls = 0;
    mcujs_test_i2c_driver_install_calls = 0;
    mcujs_test_i2c_driver_delete_calls = 0;
    mcujs_test_i2c_param_config_result = ESP_OK;
    mcujs_test_i2c_driver_install_result = ESP_OK;
    mcujs_test_i2c_driver_delete_result = ESP_OK;
    mcujs_test_ledc_result = ESP_OK;
    mcujs_test_ledc_resolution = UINT32_MAX;
    mcujs_test_ledc_configured_resolution = 0;
    mcujs_test_ledc_actual_frequency = 0;
    mcujs_test_ledc_stop_calls = 0;
    mcujs_test_gpio_reset_calls = 0;
    mcujs_test_gpio_direction_calls = 0;
    mcujs_test_gpio_pull_calls = 0;
    mcujs_test_gpio_level_calls = 0;
    mcujs_test_ledc_duty_calls = 0;
    mcujs_test_ledc_duty = 0;
    mcujs_test_ledc_timer_result = ESP_OK;
    mcujs_test_ledc_timer_deconfigure_result = ESP_OK;
    mcujs_test_ledc_channel_result = ESP_OK;
    mcujs_test_ledc_stop_result = ESP_OK;
    mcujs_test_ledc_pause_result = ESP_OK;
    mcujs_test_gpio_reset_result = ESP_OK;
    mcujs_test_ledc_timer_config_calls = 0;
    mcujs_test_ledc_timer_deconfigure_calls = 0;
    mcujs_test_ledc_timer_pause_calls = 0;
    mcujs_test_ledc_channel_config_calls = 0;
    mcujs_test_adc_read_calls = 0;
    mcujs_test_adc_calibrated_calls = 0;
    mcujs_test_adc_raw_value = 2048;
    mcujs_test_adc_unit_result = ESP_OK;
    mcujs_test_adc_config_result = ESP_OK;
    mcujs_test_adc_read_result = ESP_OK;
    mcujs_test_adc_calibration_result = ESP_OK;
    mcujs_test_adc_millivolts = 1234;
    mcujs_test_temperature_install_result = ESP_OK;
    mcujs_test_temperature_enable_result = ESP_OK;
    mcujs_test_temperature_read_result = ESP_OK;
    mcujs_test_temperature_disable_result = ESP_OK;
    mcujs_test_temperature_celsius = 31.25f;
    mcujs_test_spi_bus_init_result = ESP_OK;
    mcujs_test_spi_bus_free_result = ESP_OK;
    mcujs_test_spi_add_result = ESP_OK;
    mcujs_test_spi_remove_result = ESP_OK;
    mcujs_test_spi_transmit_result = ESP_OK;
    mcujs_test_spi_last_host = -1;
    mcujs_test_spi_last_frequency = 0;
    mcujs_test_spi_last_mode = -1;
    mcujs_test_spi_actual_frequency = 0;
    mcujs_test_spi_remove_calls = 0;
    mcujs_test_spi_last_transfer_length = 0;
    mcujs_test_led_strip_new_result = ESP_OK;
    mcujs_test_led_strip_set_result = ESP_OK;
    mcujs_test_led_strip_refresh_result = ESP_OK;
    mcujs_test_led_strip_clear_result = ESP_OK;
    mcujs_test_led_strip_del_result = ESP_OK;
    mcujs_test_led_strip_new_calls = 0;
    mcujs_test_led_strip_set_calls = 0;
    mcujs_test_led_strip_refresh_calls = 0;
    mcujs_test_led_strip_clear_calls = 0;
    mcujs_test_led_strip_del_calls = 0;
    mcujs_test_led_strip_last_pin = -1;
    mcujs_test_led_strip_last_length = 0;
    mcujs_test_led_strip_last_order = -1;
    mcujs_test_led_strip_last_index = 0;
    mcujs_test_led_strip_last_red = 0;
    mcujs_test_led_strip_last_green = 0;
    mcujs_test_led_strip_last_blue = 0;
    s_led_strip_storage.active = false;
    for (size_t i = 0; i < 4; i++) s_ledc_timer_frequency[i] = 0;
    for (size_t i = 0; i < GPIO_NUM_MAX; i++) {
        s_gpio_levels[i] = 0;
        s_gpio_directions[i] = GPIO_MODE_INPUT;
        s_gpio_pulls[i] = GPIO_FLOATING;
    }
}

esp_err_t gpio_reset_pin(gpio_num_t pin) {
    mcujs_test_gpio_reset_calls++;
    if (mcujs_test_gpio_reset_result == ESP_OK && pin >= 0 &&
        pin < GPIO_NUM_MAX) {
        s_gpio_directions[pin] = GPIO_MODE_INPUT;
        s_gpio_pulls[pin] = GPIO_PULLUP_ONLY;
    }
    return mcujs_test_gpio_reset_result;
}
esp_err_t gpio_set_direction(gpio_num_t pin, int mode) {
    mcujs_test_gpio_direction_calls++;
    if (mcujs_test_gpio_result == ESP_OK && pin >= 0 && pin < GPIO_NUM_MAX) {
        s_gpio_directions[pin] = mode;
    }
    return mcujs_test_gpio_result;
}
esp_err_t gpio_set_pull_mode(gpio_num_t pin, int mode) {
    mcujs_test_gpio_pull_calls++;
    if (mcujs_test_gpio_result == ESP_OK && pin >= 0 && pin < GPIO_NUM_MAX) {
        s_gpio_pulls[pin] = mode;
    }
    return mcujs_test_gpio_result;
}
esp_err_t gpio_set_level(gpio_num_t pin, int level) {
    mcujs_test_gpio_level_calls++;
    if (mcujs_test_gpio_result != ESP_OK) return mcujs_test_gpio_result;
    if (pin >= 0 && pin < GPIO_NUM_MAX) s_gpio_levels[pin] = level;
    return ESP_OK;
}
int gpio_get_level(gpio_num_t pin) {
    return pin >= 0 && pin < GPIO_NUM_MAX ? s_gpio_levels[pin] : 0;
}
bool mcujs_test_gpio_is_output(unsigned pin) {
    return pin < GPIO_NUM_MAX &&
           s_gpio_directions[pin] == GPIO_MODE_INPUT_OUTPUT;
}
int mcujs_test_gpio_level_at(unsigned pin) {
    return pin < GPIO_NUM_MAX ? s_gpio_levels[pin] : -1;
}
int mcujs_test_gpio_pull_at(unsigned pin) {
    return pin < GPIO_NUM_MAX ? s_gpio_pulls[pin] : -1;
}

esp_err_t i2c_param_config(i2c_port_t port, const i2c_config_t *config) {
    (void)port;
    (void)config;
    mcujs_test_i2c_param_config_calls++;
    return mcujs_test_i2c_param_config_result;
}
esp_err_t i2c_driver_install(i2c_port_t port, int mode, size_t rx_buffer_length,
                             size_t tx_buffer_length, int interrupt_flags) {
    (void)port;
    (void)mode;
    (void)rx_buffer_length;
    (void)tx_buffer_length;
    (void)interrupt_flags;
    mcujs_test_i2c_driver_install_calls++;
    return mcujs_test_i2c_driver_install_result;
}
esp_err_t i2c_driver_delete(i2c_port_t port) {
    (void)port;
    mcujs_test_i2c_driver_delete_calls++;
    return mcujs_test_i2c_driver_delete_result;
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

esp_err_t spi_bus_initialize(spi_host_device_t host,
                             const spi_bus_config_t *config,
                             int dma_channel) {
    (void)config;
    (void)dma_channel;
    mcujs_test_spi_last_host = host;
    return mcujs_test_spi_bus_init_result;
}

esp_err_t spi_bus_free(spi_host_device_t host) {
    (void)host;
    return mcujs_test_spi_bus_free_result;
}

esp_err_t spi_bus_add_device(spi_host_device_t host,
                             const spi_device_interface_config_t *config,
                             spi_device_handle_t *device) {
    mcujs_test_spi_last_host = host;
    mcujs_test_spi_last_frequency = config->clock_speed_hz;
    mcujs_test_spi_last_mode = config->mode;
    if (mcujs_test_spi_add_result == ESP_OK) {
        *device = &s_spi_devices[host];
    }
    return mcujs_test_spi_add_result;
}

esp_err_t spi_bus_remove_device(spi_device_handle_t device) {
    (void)device;
    mcujs_test_spi_remove_calls++;
    return mcujs_test_spi_remove_result;
}

int spi_get_actual_clock(int source_hz, int requested_hz, int duty_cycle) {
    (void)source_hz;
    (void)duty_cycle;
    return mcujs_test_spi_actual_frequency != 0
               ? mcujs_test_spi_actual_frequency
               : requested_hz;
}

esp_err_t spi_device_transmit(spi_device_handle_t device,
                              spi_transaction_t *transaction) {
    (void)device;
    mcujs_test_spi_last_transfer_length = transaction->length / 8u;
    if (mcujs_test_spi_transmit_result != ESP_OK) {
        return mcujs_test_spi_transmit_result;
    }
    memcpy(transaction->rx_buffer, transaction->tx_buffer,
           mcujs_test_spi_last_transfer_length);
    return ESP_OK;
}

esp_err_t led_strip_new_rmt_device(const led_strip_config_t *strip_config,
                                   const led_strip_rmt_config_t *rmt_config,
                                   led_strip_handle_t *strip) {
    (void)rmt_config;
    mcujs_test_led_strip_new_calls++;
    mcujs_test_led_strip_last_pin = strip_config->strip_gpio_num;
    mcujs_test_led_strip_last_length = strip_config->max_leds;
    mcujs_test_led_strip_last_order = strip_config->color_component_format;
    if (mcujs_test_led_strip_new_result == ESP_OK) {
        s_led_strip_storage.active = true;
        *strip = &s_led_strip_storage;
    }
    return mcujs_test_led_strip_new_result;
}

esp_err_t led_strip_set_pixel(led_strip_handle_t strip, uint32_t index,
                              uint32_t red, uint32_t green, uint32_t blue) {
    assert(strip == &s_led_strip_storage && strip->active);
    mcujs_test_led_strip_set_calls++;
    mcujs_test_led_strip_last_index = index;
    mcujs_test_led_strip_last_red = red;
    mcujs_test_led_strip_last_green = green;
    mcujs_test_led_strip_last_blue = blue;
    return mcujs_test_led_strip_set_result;
}

esp_err_t led_strip_refresh(led_strip_handle_t strip) {
    assert(strip == &s_led_strip_storage && strip->active);
    mcujs_test_led_strip_refresh_calls++;
    return mcujs_test_led_strip_refresh_result;
}

esp_err_t led_strip_clear(led_strip_handle_t strip) {
    assert(strip == &s_led_strip_storage && strip->active);
    mcujs_test_led_strip_clear_calls++;
    return mcujs_test_led_strip_clear_result;
}

esp_err_t led_strip_del(led_strip_handle_t strip) {
    assert(strip == &s_led_strip_storage && strip->active);
    mcujs_test_led_strip_del_calls++;
    if (mcujs_test_led_strip_del_result == ESP_OK) strip->active = false;
    return mcujs_test_led_strip_del_result;
}

esp_err_t ledc_timer_pause(int speed_mode, ledc_timer_t timer) {
    (void)speed_mode;
    (void)timer;
    mcujs_test_ledc_timer_pause_calls++;
    return mcujs_test_ledc_pause_result;
}

esp_err_t ledc_timer_config(const ledc_timer_config_t *config) {
    mcujs_test_ledc_timer_config_calls++;
    if (config->deconfigure) mcujs_test_ledc_timer_deconfigure_calls++;
    esp_err_t result = config->deconfigure
                           ? mcujs_test_ledc_timer_deconfigure_result
                           : mcujs_test_ledc_timer_result;
    if (result == ESP_OK && !config->deconfigure &&
        config->timer_num >= 0 && config->timer_num < 4) {
        s_ledc_timer_frequency[config->timer_num] = config->freq_hz;
        mcujs_test_ledc_configured_resolution = config->duty_resolution;
    }
    if (result == ESP_OK && config->deconfigure && config->timer_num >= 0 &&
        config->timer_num < 4) {
        s_ledc_timer_frequency[config->timer_num] = 0;
    }
    return result;
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
    return mcujs_test_ledc_stop_result;
}

uint32_t ledc_find_suitable_duty_resolution(uint32_t source_clock,
                                            uint32_t frequency) {
    if (mcujs_test_ledc_resolution != UINT32_MAX) {
        return mcujs_test_ledc_resolution;
    }
    if (source_clock == 0u || frequency == 0u) return 0u;

    for (uint32_t resolution = 14u; resolution > 0u; resolution--) {
        uint64_t precision = (uint64_t)1u << resolution;
        uint64_t scaled_source = (uint64_t)source_clock << 8u;
        uint64_t denominator = (uint64_t)frequency * precision;
        uint64_t divider = (scaled_source + denominator / 2u) / denominator;
        if (divider >= (1u << 8u) && divider <= 0x3ffffu) {
            return resolution;
        }
    }
    return 0u;
}

esp_err_t ledc_channel_config(const ledc_channel_config_t *config) {
    mcujs_test_ledc_channel_config_calls++;
    if (mcujs_test_ledc_channel_result == ESP_OK) {
        mcujs_test_ledc_duty = config->duty;
    }
    return mcujs_test_ledc_channel_result;
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

esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *config,
                               adc_oneshot_unit_handle_t *handle) {
    (void)config;
    if (mcujs_test_adc_unit_result == ESP_OK) {
        *handle = (adc_oneshot_unit_handle_t)&s_adc_unit_storage;
    }
    return mcujs_test_adc_unit_result;
}

esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle,
                                     adc_channel_t channel,
                                     const adc_oneshot_chan_cfg_t *config) {
    (void)handle;
    (void)channel;
    (void)config;
    return mcujs_test_adc_config_result;
}

esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle,
                           adc_channel_t channel, int *raw) {
    (void)handle;
    (void)channel;
    mcujs_test_adc_read_calls++;
    if (mcujs_test_adc_read_result == ESP_OK) *raw = mcujs_test_adc_raw_value;
    return mcujs_test_adc_read_result;
}

esp_err_t adc_cali_create_scheme_curve_fitting(
    const adc_cali_curve_fitting_config_t *config,
    adc_cali_handle_t *handle) {
    (void)config;
    if (mcujs_test_adc_calibration_result == ESP_OK) {
        *handle = (adc_cali_handle_t)&s_adc_cali_storage;
    }
    return mcujs_test_adc_calibration_result;
}

esp_err_t adc_oneshot_get_calibrated_result(adc_oneshot_unit_handle_t unit,
                                             adc_cali_handle_t calibration,
                                             adc_channel_t channel,
                                             int *millivolts) {
    (void)unit;
    (void)calibration;
    (void)channel;
    mcujs_test_adc_calibrated_calls++;
    if (mcujs_test_adc_calibration_result == ESP_OK) {
        *millivolts = mcujs_test_adc_millivolts;
    }
    return mcujs_test_adc_calibration_result;
}

esp_err_t temperature_sensor_install(const temperature_sensor_config_t *config,
                                     temperature_sensor_handle_t *handle) {
    (void)config;
    if (mcujs_test_temperature_install_result == ESP_OK) {
        *handle = (temperature_sensor_handle_t)&s_temperature_sensor_storage;
    }
    return mcujs_test_temperature_install_result;
}

esp_err_t temperature_sensor_enable(temperature_sensor_handle_t handle) {
    (void)handle;
    return mcujs_test_temperature_enable_result;
}

esp_err_t temperature_sensor_get_celsius(temperature_sensor_handle_t handle,
                                         float *celsius) {
    (void)handle;
    if (mcujs_test_temperature_read_result == ESP_OK) {
        *celsius = mcujs_test_temperature_celsius;
    }
    return mcujs_test_temperature_read_result;
}

esp_err_t temperature_sensor_disable(temperature_sensor_handle_t handle) {
    (void)handle;
    return mcujs_test_temperature_disable_result;
}

#else
#error "backend stubs require an explicit platform lane"
#endif
