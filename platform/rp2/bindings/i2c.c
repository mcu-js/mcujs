/*
 * mcujs - I2C Bindings
 * 
 * Implements: I2C.init(), I2C.write(), I2C.read()
 */

#include "bindings.h"
#include "i2c_options.h"
#include "jerryscript.h"
#include "pin_policy.h"
#include "validation.h"

#include "pico/error.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"

#include <stdint.h>

/* Bound synchronous user transfers without rejecting the current descriptors'
 * slowest exactly representable maximum-size transfer. */
#define MCUJS_RP2_I2C_TRANSFER_TIMEOUT_US 5000000u

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);


static bool s_i2c_initialized[2];
static bool s_touch_owned;
static int s_i2c_sda[2] = {-1, -1};
static int s_i2c_scl[2] = {-1, -1};

static mcujs_rp2_pin_owner_t i2c_pin_owner(int bus) {
    return bus == 0 ? MCUJS_RP2_PIN_OWNER_I2C0 : MCUJS_RP2_PIN_OWNER_I2C1;
}


/* I2C instances */
static i2c_inst_t *get_i2c_instance(int bus) {
    switch (bus) {
        case 0: return i2c0;
        case 1: return i2c1;
        default: return NULL;
    }
}

static jerry_value_t throw_i2c_failure(int result, int bus,
                                       const char *message) {
    mcujs_operational_error_t error = MCUJS_ERROR_IO;
    if (result == PICO_ERROR_GENERIC) {
        error = MCUJS_ERROR_NO_DEVICE;
    } else if (result == PICO_ERROR_TIMEOUT) {
        error = MCUJS_ERROR_BUSY;
    }
    const mcujs_error_details_t details = {
        .resource = "i2c",
        .has_bus = true,
        .bus = bus,
        .has_native_code = true,
        .native_code = result,
    };
    return mcujs_throw_operational_error(error, message, &details);
}

static jerry_value_t throw_i2c_busy(int bus, const char *message) {
    const mcujs_error_details_t details = {
        .resource = "i2c",
        .has_bus = true,
        .bus = bus,
    };
    return mcujs_throw_operational_error(MCUJS_ERROR_BUSY, message, &details);
}

static jerry_value_t throw_i2c_pin_busy(int bus, int pin,
                                        const char *message) {
    const mcujs_error_details_t details = {
        .resource = "i2c",
        .has_pin = true,
        .pin = pin,
        .has_bus = true,
        .bus = bus,
    };
    return mcujs_throw_operational_error(MCUJS_ERROR_BUSY, message, &details);
}

static void release_i2c_route(int pin, mcujs_rp2_pin_owner_t owner,
                              int new_sda, int new_scl) {
    if (pin < 0 || pin == new_sda || pin == new_scl) return;
    gpio_set_function((uint)pin, GPIO_FUNC_SIO);
    gpio_init((uint)pin);
    mcujs_rp2_pin_release(pin, owner);
}

static void release_i2c_pin(int pin, mcujs_rp2_pin_owner_t owner) {
    if (pin < 0 || mcujs_rp2_pin_owner(pin) != owner) return;
    gpio_set_function((uint)pin, GPIO_FUNC_SIO);
    gpio_init((uint)pin);
    mcujs_rp2_pin_release(pin, owner);
}

static void rollback_i2c_init(int bus, int sda_pin, int scl_pin,
                              mcujs_rp2_pin_owner_t owner) {
    release_i2c_pin(s_i2c_sda[bus], owner);
    release_i2c_pin(s_i2c_scl[bus], owner);
    release_i2c_pin(sda_pin, owner);
    release_i2c_pin(scl_pin, owner);
    s_i2c_sda[bus] = -1;
    s_i2c_scl[bus] = -1;
    s_i2c_initialized[bus] = false;
}

static jerry_value_t throw_i2c_short(int result, int bus,
                                     const char *message) {
    const mcujs_error_details_t details = {
        .resource = "i2c",
        .has_bus = true,
        .bus = bus,
        .has_native_code = true,
        .native_code = result,
    };
    return mcujs_throw_operational_error(MCUJS_ERROR_IO, message, &details);
}

/*
 * I2C.init(options) or I2C.init(bus, sda, scl, frequency)
 * Initialize an I2C bus using the preferred portable options form or the 0.x
 * positional compatibility form.
 */
static jerry_value_t i2c_init_handler(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc) {
    (void)call_info_p;

    mcujs_i2c_init_options_t options;
    jerry_value_t parsed = mcujs_parse_i2c_init_args(args, argc, &options);
    if (jerry_value_is_exception(parsed)) return parsed;
    jerry_value_free(parsed);
    int bus = options.bus;
    int sda_pin = options.sda;
    int scl_pin = options.scl;
    int baudrate = options.frequency;

    i2c_inst_t *i2c = get_i2c_instance(bus);
    if (i2c == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid I2C bus (0 or 1)");
    }
    if (!mcujs_rp2_gpio_pin_allowed(sda_pin) ||
        !mcujs_rp2_gpio_pin_allowed(scl_pin) || sda_pin == scl_pin) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Invalid I2C SDA/SCL pin route");
    }

    if (bus == 1 && s_touch_owned)
        return throw_i2c_busy(bus, "I2C1 is exclusively owned by Canvas touch");

    uint32_t clock_frequency = clock_get_hz(clk_sys);
    uint32_t period =
        (clock_frequency + (uint32_t)baudrate / 2u) / (uint32_t)baudrate;
    uint32_t lcnt = period * 3u / 5u;
    uint32_t hcnt = period - lcnt;
    uint32_t represented_baudrate = clock_frequency / period;
    if (hcnt > UINT16_MAX || lcnt > UINT16_MAX || hcnt < 8u || lcnt < 8u ||
        (uint64_t)(uint32_t)baudrate * period != clock_frequency) {
        const mcujs_error_details_t details = {
            .resource = "i2c",
            .has_bus = true,
            .bus = bus,
            .has_native_code = true,
            .native_code = (int)represented_baudrate,
        };
        return mcujs_throw_operational_error(
            MCUJS_ERROR_NOT_SUPPORTED,
            "I2C baudrate cannot be represented exactly", &details);
    }

    mcujs_rp2_pin_owner_t owner = i2c_pin_owner(bus);
    if (!mcujs_rp2_pin_can_claim(sda_pin, owner)) {
        return throw_i2c_pin_busy(bus, sda_pin,
                                  "I2C SDA pin is owned by another peripheral");
    }
    if (!mcujs_rp2_pin_can_claim(scl_pin, owner)) {
        return throw_i2c_pin_busy(bus, scl_pin,
                                  "I2C SCL pin is owned by another peripheral");
    }
    bool sda_owned = mcujs_rp2_pin_owner(sda_pin) == owner;
    if (!mcujs_rp2_pin_claim(sda_pin, owner)) {
        return throw_i2c_pin_busy(bus, sda_pin, "I2C SDA pin claim failed");
    }
    if (!mcujs_rp2_pin_claim(scl_pin, owner)) {
        if (!sda_owned) mcujs_rp2_pin_release(sda_pin, owner);
        return throw_i2c_pin_busy(bus, scl_pin, "I2C SCL pin claim failed");
    }

    if (s_i2c_initialized[bus]) {
        i2c_deinit(i2c);
        s_i2c_initialized[bus] = false;
    }

    uint actual_baudrate = i2c_init(i2c, (uint)baudrate);
    if (actual_baudrate == 0) {
        rollback_i2c_init(bus, sda_pin, scl_pin, owner);
        return throw_i2c_short(0, bus, "I2C initialization failed");
    }
    if (actual_baudrate != (uint)baudrate) {
        i2c_deinit(i2c);
        rollback_i2c_init(bus, sda_pin, scl_pin, owner);
        const mcujs_error_details_t details = {
            .resource = "i2c",
            .has_bus = true,
            .bus = bus,
            .has_native_code = true,
            .native_code = (int)actual_baudrate,
        };
        return mcujs_throw_operational_error(
            MCUJS_ERROR_NOT_SUPPORTED,
            "I2C baudrate cannot be represented exactly", &details);
    }
    gpio_set_function((uint)sda_pin, GPIO_FUNC_I2C);
    gpio_set_function((uint)scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up((uint)sda_pin);
    gpio_pull_up((uint)scl_pin);

    release_i2c_route(s_i2c_sda[bus], owner, sda_pin, scl_pin);
    release_i2c_route(s_i2c_scl[bus], owner, sda_pin, scl_pin);
    s_i2c_sda[bus] = sda_pin;
    s_i2c_scl[bus] = scl_pin;
    s_i2c_initialized[bus] = true;

    return jerry_undefined();
}

/*
 * I2C.write(bus, address, data)
 * Write data to I2C device
 * data can be an array of bytes or a single byte
 */
static jerry_value_t i2c_write_handler(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args[],
                                        const jerry_length_t argc) {
    (void)call_info_p;

    int bus;
    int address;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &bus);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C bus must be a finite number",
                              "I2C bus must be an integer");
    }
    status = mcujs_get_integer(args, argc, 1, &address);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C address must be a finite number",
                              "I2C address must be an integer");
    }
    i2c_inst_t *i2c = get_i2c_instance(bus);
    if (i2c == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid I2C bus");
    }
    if (address < 0 || address > 0x7f) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid 7-bit I2C address");
    }

    uint8_t buffer[MCUJS_RUNTIME_I2C_MAX_TRANSFER_BYTES];
    size_t len = 0;
    if (argc > 2 && jerry_value_is_array(args[2])) {
        jerry_value_t exception;
        status = mcujs_get_byte_array(
            args, argc, 2, buffer, sizeof(buffer), 1,
            MCUJS_RUNTIME_I2C_MAX_TRANSFER_BYTES, &len, &exception);
        if (status == MCUJS_ARG_EXCEPTION) return exception;
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status,
                                  "I2C data must be an array of numbers",
                                  "I2C data exceeds maxTransferBytes");
        }
    } else {
        if (argc < 3) {
            return jerry_throw_sz(JERRY_ERROR_TYPE, "I2C.write requires data");
        }
        status = mcujs_value_to_byte(args[2], &buffer[0]);
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status,
                                  "I2C data must be a number or byte array",
                                  "I2C data byte must be an integer 0..255");
        }
        len = 1;
    }

    if (!s_i2c_initialized[bus]) {
        return throw_i2c_busy(bus, "I2C bus is not initialized");
    }

    int result = i2c_write_timeout_us(
        i2c, (uint8_t)address, buffer, len, false,
        MCUJS_RP2_I2C_TRANSFER_TIMEOUT_US);
    if (result < 0) {
        return throw_i2c_failure(result, bus, "I2C write failed");
    }
    if ((size_t)result != len) {
        return throw_i2c_short(result, bus, "I2C write was incomplete");
    }
    return jerry_number((double)result);
}

/*
 * I2C.read(bus, address, length)
 * Read data from I2C device
 * Returns array of bytes
 */
static jerry_value_t i2c_read_handler(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc) {
    (void)call_info_p;

    int bus;
    int address;
    int requested;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &bus);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C bus must be a finite number",
                              "I2C bus must be an integer");
    }
    status = mcujs_get_integer(args, argc, 1, &address);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C address must be a finite number",
                              "I2C address must be an integer");
    }
    status = mcujs_get_integer(args, argc, 2, &requested);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C read length must be a finite number",
                              "I2C read length must be an integer");
    }
    i2c_inst_t *i2c = get_i2c_instance(bus);
    if (i2c == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid I2C bus");
    }
    if (address < 0 || address > 0x7f) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid 7-bit I2C address");
    }
    if (requested < 1 ||
        requested > MCUJS_RUNTIME_I2C_MAX_TRANSFER_BYTES) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "I2C read length exceeds maxTransferBytes");
    }

    if (!s_i2c_initialized[bus]) {
        return throw_i2c_busy(bus, "I2C bus is not initialized");
    }

    uint8_t buffer[MCUJS_RUNTIME_I2C_MAX_TRANSFER_BYTES];
    int result = i2c_read_timeout_us(
        i2c, (uint8_t)address, buffer, (size_t)requested, false,
        MCUJS_RP2_I2C_TRANSFER_TIMEOUT_US);
    if (result < 0) {
        return throw_i2c_failure(result, bus, "I2C read failed");
    }
    if (result != requested) {
        return throw_i2c_short(result, bus, "I2C read was incomplete");
    }

    jerry_value_t array = jerry_array((uint32_t)result);
    if (jerry_value_is_exception(array)) return array;
    for (int i = 0; i < result; i++) {
        jerry_value_t byte_val = jerry_number((double)buffer[i]);
        jerry_value_t set_result =
            jerry_object_set_index(array, (uint32_t)i, byte_val);
        jerry_value_free(byte_val);
        if (jerry_value_is_exception(set_result)) {
            jerry_value_free(array);
            return set_result;
        }
        jerry_value_free(set_result);
    }
    return array;
}

/* This first touch slice deliberately owns I2C1 exclusively, including its
 * onboard IMU route. Keep the lease beside the user driver's initialized state:
 * never reinitialize a user bus (even on different pins), nor expose touch's bus
 * to I2C.read/write. No interrupt-level contact inference or shared-bus framework.
 * Protocol evidence and orientation derivation: docs/docs/development/canvas-pointer.md.
 */
#if defined(MCUJS_EXPERIMENTAL_CANVAS) && defined(MCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_1_69)
#include "touch_169.h"
static const char *touch_error="Touch sample failed";
const char *mcujs_touch_169_error(void) { return touch_error; }
#define TOUCH_TIMEOUT_US 5000u
static const int touch_pins[] = {6, 7, 22};
static bool touch_read(uint8_t reg, uint8_t *data, size_t size) {
    /* Match the stable STOP-separated register reads observed on the panel.
     * This bus is exclusively leased, so no other owner can interleave. */
    int w=i2c_write_timeout_us(i2c1,0x15,&reg,1,false,TOUCH_TIMEOUT_US);
    int r=w==1?i2c_read_timeout_us(i2c1,0x15,data,size,false,TOUCH_TIMEOUT_US):-999;
    if(w!=1) touch_error=w==-2?"Touch register write timed out":"Touch register write failed";
    else if(r!=(int)size) touch_error=r==-2?"Touch report read timed out":"Touch report read failed";
    return w==1 && r==(int)size;
}
static bool touch_write(uint8_t reg, uint8_t value) {
    uint8_t data[]={reg,value};
    return i2c_write_timeout_us(i2c1,0x15,data,2,false,TOUCH_TIMEOUT_US)==2;
}
void mcujs_touch_169_close(void) {
    if (!s_touch_owned) return;
    s_touch_owned=false;
    /* Hold only our controller in reset; no teardown I/O can block on a failed bus. */
    gpio_put(22,false);
    i2c_deinit(i2c1);
    for (unsigned i=0;i<sizeof(touch_pins)/sizeof(*touch_pins);i++) {
        gpio_init((uint)touch_pins[i]);
        mcujs_rp2_pin_release(touch_pins[i],MCUJS_RP2_PIN_OWNER_TOUCH);
    }
}
int mcujs_touch_169_open(void) {
    if (s_touch_owned || s_i2c_initialized[1]) return 1;
    /* Unlike generic peripheral takeover, even a configured GPIO is a conflict. */
    for (unsigned i=0;i<sizeof(touch_pins)/sizeof(*touch_pins);i++)
        if (!mcujs_rp2_gpio_pin_allowed(touch_pins[i]) ||
            mcujs_rp2_pin_owner(touch_pins[i])!=MCUJS_RP2_PIN_OWNER_NONE) return 1;
    for (unsigned i=0;i<sizeof(touch_pins)/sizeof(*touch_pins);i++)
        mcujs_rp2_pin_claim(touch_pins[i],MCUJS_RP2_PIN_OWNER_TOUCH);
    s_touch_owned=true;
    gpio_init(22);gpio_put(22,false);gpio_set_dir(22,GPIO_OUT);
    if (i2c_init(i2c1,400000)!=400000) { mcujs_touch_169_close(); return 2; }
    gpio_set_function(6,GPIO_FUNC_I2C);gpio_set_function(7,GPIO_FUNC_I2C);
    gpio_pull_up(6);gpio_pull_up(7);
    sleep_ms(100);gpio_put(22,true);sleep_ms(100);
    uint8_t id;
    if (!touch_read(0xa7,&id,1) || id!=0xb5 || !touch_write(0xfe,7) || !touch_write(0xfa,0x60)) {
        mcujs_touch_169_close(); return 2;
    }
    return 0;
}
bool mcujs_touch_169_sample(bool horizontal, bool *pressed, int *x, int *y) {
    uint8_t data[5];
    if (!s_touch_owned) { touch_error="Touch ownership lost"; return false; }
    if (!touch_read(2,data,sizeof(data))) return false;
    /* FingerNum is the contact oracle, never the pulsed INT pin. */
    if (data[0]>1) { touch_error="Touch contact count invalid"; return false; }
    *pressed=data[0]==1;
    if (!*pressed) { *x=*y=0; return true; }
    int raw_x=((data[1]&15)<<8)|data[2], raw_y=((data[3]&15)<<8)|data[4];
    if (raw_x>=240 || raw_y>=280) { touch_error="Touch coordinates outside panel"; return false; }
    /* ST7789 MADCTL 00 portrait / 70 landscape (MV|MX, ML is refresh order).
     * RAM window offsets of 20 are not touch/Canvas coordinates. */
    *x=horizontal?raw_y:raw_x; *y=horizontal?239-raw_x:raw_y;
    return true;
}
#endif

/*
 * Create I2C module object
 */
jerry_value_t js_create_i2c_module(void) {
    jerry_value_t i2c = jerry_object();

    js_set_function(i2c, "init", i2c_init_handler);
    js_set_function(i2c, "write", i2c_write_handler);
    js_set_function(i2c, "read", i2c_read_handler);

    return i2c;
}

/*
 * Register I2C bindings
 */
void js_bind_i2c(void) {
    jerry_value_t i2c = js_create_i2c_module();
    js_register_global("I2C", i2c);
    jerry_value_free(i2c);
}
