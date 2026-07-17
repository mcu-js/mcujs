/*
 * mcujs - Runtime feature selection
 *
 * Platform backends can disable unavailable subsystems while the port is
 * brought up incrementally. Defaults preserve the full RP2 runtime.
 */

#ifndef MCUJS_RUNTIME_FEATURES_H
#define MCUJS_RUNTIME_FEATURES_H

#ifndef MCUJS_FEATURE_MODULE_LOADER
#define MCUJS_FEATURE_MODULE_LOADER 1
#endif
#ifndef MCUJS_FEATURE_CONSOLE
#define MCUJS_FEATURE_CONSOLE 1
#endif
#ifndef MCUJS_FEATURE_TIMERS
#define MCUJS_FEATURE_TIMERS 1
#endif
#ifndef MCUJS_FEATURE_BOARD
#define MCUJS_FEATURE_BOARD 1
#endif
#ifndef MCUJS_FEATURE_GPIO
#define MCUJS_FEATURE_GPIO 1
#endif
#ifndef MCUJS_FEATURE_PWM
#define MCUJS_FEATURE_PWM 1
#endif
#ifndef MCUJS_FEATURE_I2C
#define MCUJS_FEATURE_I2C 1
#endif
#ifndef MCUJS_FEATURE_SPI
#define MCUJS_FEATURE_SPI 1
#endif
#ifndef MCUJS_FEATURE_ADC
#define MCUJS_FEATURE_ADC 1
#endif
#ifndef MCUJS_FEATURE_NEOPIXEL
#define MCUJS_FEATURE_NEOPIXEL 1
#endif
#ifndef MCUJS_FEATURE_PROCESS
#define MCUJS_FEATURE_PROCESS 1
#endif
#ifndef MCUJS_FEATURE_REQUIRE
#define MCUJS_FEATURE_REQUIRE 1
#endif
#ifndef MCUJS_FEATURE_GRAPHICS
#define MCUJS_FEATURE_GRAPHICS 1
#endif
#ifndef MCUJS_FEATURE_SCREEN
#define MCUJS_FEATURE_SCREEN 1
#endif

#endif /* MCUJS_RUNTIME_FEATURES_H */
