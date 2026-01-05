/*
 * mcujs - JavaScript Bindings Interface
 * 
 * Common interface for all native JavaScript API bindings
 */

#ifndef MCUJS_BINDINGS_H
#define MCUJS_BINDINGS_H

#include <stdbool.h>

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
 * Register I2C bindings
 * I2C.init(), I2C.write(), I2C.read()
 */
void js_bind_i2c(void);

/*
 * Register SPI bindings
 * SPI.init(), SPI.transfer()
 */
void js_bind_spi(void);

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
