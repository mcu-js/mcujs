/*
 * mcujs - JavaScript Bindings Interface
 * 
 * Common interface for all native JavaScript API bindings
 */

#ifndef MCUJS_BINDINGS_H
#define MCUJS_BINDINGS_H

#include <stdbool.h>
#include "jerryscript.h"

/*
 * Register console bindings
 * console.log(), console.warn(), console.error()
 */
void js_bind_console(void);

/*
 * Register GPIO bindings
 * GPIO.init(), GPIO.set(), GPIO.get(), GPIO.toggle()
 */
void js_bind_gpio(void);

/*
 * Create GPIO module object
 */
jerry_value_t js_create_gpio_module(void);

/*
 * Register timer bindings
 * setTimeout(), clearTimeout(), setInterval(), clearInterval()
 */
void js_bind_timers(void);

/*
 * Process pending timers
 * Returns true if there are still pending timers
 */
bool js_timers_process(void);

/*
 * Register PWM bindings
 * PWM.init(), PWM.setDuty(), PWM.stop()
 */
void js_bind_pwm(void);

/*
 * Create PWM module object
 */
jerry_value_t js_create_pwm_module(void);

/*
 * Register I2C bindings
 * I2C.init(), I2C.write(), I2C.read()
 */
void js_bind_i2c(void);

/*
 * Create I2C module object
 */
jerry_value_t js_create_i2c_module(void);

/*
 * Register SPI bindings
 * SPI.init(), SPI.transfer()
 */
void js_bind_spi(void);

/*
 * Create SPI module object
 */
jerry_value_t js_create_spi_module(void);

/*
 * Register ADC bindings
 * adc.readPin(), adc.readChannel(), adc.readTempC(), adc.TEMP
 */
void js_bind_adc(void);

/*
 * Create ADC module object
 */
jerry_value_t js_create_adc_module(void);

/*
 * Register board bindings
 * board.name, board.chip, board.freeMemory()
 */
void js_bind_board(void);

/*
 * Register NeoPixel bindings
 * neopixel.init(), neopixel.setPixel(), neopixel.show(), neopixel.clear()
 */
void js_bind_neopixel(void);

/*
 * Create NeoPixel module object
 */
jerry_value_t js_create_neopixel_module(void);

/*
 * Register filesystem bindings (Node.js-compatible)
 * fs.readFileSync(), fs.writeFileSync(), fs.existsSync(), etc.
 */
void js_bind_fs(void);

/*
 * Create filesystem module object
 */
jerry_value_t js_create_fs_module(void);

/*
 * Register process bindings (Node.js-compatible)
 * process.version, process.versions, process.arch, process.platform
 */
void js_bind_process(void);

/*
 * Register require bindings (CommonJS module system)
 * require(), module.exports, exports
 */
void js_bind_require(void);

/*
 * Helper functions for bindings
 */
void js_set_property(jerry_value_t object, const char *name, jerry_value_t value);
void js_set_function(jerry_value_t object, const char *name, jerry_external_handler_t handler);
void js_set_number(jerry_value_t object, const char *name, double value);
void js_set_string(jerry_value_t object, const char *name, const char *value);
void js_set_boolean(jerry_value_t object, const char *name, bool value);
void js_register_global(const char *name, jerry_value_t object);

double js_get_number_arg(const jerry_value_t args[], jerry_length_t argc,
                         jerry_length_t index, double default_value);
bool js_get_boolean_arg(const jerry_value_t args[], jerry_length_t argc,
                        jerry_length_t index, bool default_value);
size_t js_get_string_arg(const jerry_value_t args[], jerry_length_t argc,
                         jerry_length_t index, char *buffer, size_t buffer_size);

/*
 * Clear the module cache (useful for hot reloading)
 */
void js_require_clear_cache(void);

/*
 * Register graphics bindings
 * graphics.createBuffer(), graphics.fill(), graphics.setPixel(), etc.
 */
void js_bind_graphics(void);

/*
 * Create graphics module object
 */
jerry_value_t js_create_graphics_module(void);

/*
 * Create image module object
 */
jerry_value_t js_create_image_module(void);

/*
 * Create keyboard module object (USB HID keyboard)
 */
jerry_value_t js_create_keyboard_module(void);

/*
 * Create mouse module object (USB HID mouse)
 */
jerry_value_t js_create_mouse_module(void);

/*
 * Register screen bindings (unified display API)
 * screen.init(), screen.fill(), screen.fillRect(), screen.show(), etc.
 */
void js_bind_screen(void);

/*
 * Create screen module object
 */
jerry_value_t js_create_screen_module(void);

/*
 * Register DVI bindings (native DVI output via PicoDVI)
 * DVI.init(), DVI.start(), DVI.show(), DVI.stop()
 * Only available on boards with DVI support (MCUJS_HAS_DVI=1)
 */
void js_bind_dvi(void);

/*
 * Create DVI module object
 */
jerry_value_t js_create_dvi_module(void);

#endif /* MCUJS_BINDINGS_H */
