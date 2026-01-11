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
 * Clear the module cache (useful for hot reloading)
 */
void js_require_clear_cache(void);

#endif /* MCUJS_BINDINGS_H */
