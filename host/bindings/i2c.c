/*
 * mcujs - I2C Bindings
 * 
 * Implements: I2C.init(), I2C.write(), I2C.read()
 */

#include "bindings.h"
#include "jerryscript.h"

#include "pico/stdlib.h"
#include "hardware/i2c.h"

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);
extern double js_get_number_arg(const jerry_value_t args[], jerry_length_t argc,
                                jerry_length_t index, double default_value);

/* Maximum I2C transfer size */
#define MAX_I2C_TRANSFER 256

/* I2C instances */
static i2c_inst_t *get_i2c_instance(int bus) {
    switch (bus) {
        case 0: return i2c0;
        case 1: return i2c1;
        default: return NULL;
    }
}

/*
 * I2C.init(bus, sda, scl, baudrate)
 * Initialize I2C bus
 */
static jerry_value_t i2c_init_handler(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 4) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "I2C.init requires bus, sda, scl, baudrate");
    }
    
    int bus = (int)js_get_number_arg(args, argc, 0, 0);
    uint sda_pin = (uint)js_get_number_arg(args, argc, 1, 0);
    uint scl_pin = (uint)js_get_number_arg(args, argc, 2, 0);
    uint32_t baudrate = (uint32_t)js_get_number_arg(args, argc, 3, 100000);
    
    i2c_inst_t *i2c = get_i2c_instance(bus);
    if (i2c == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid I2C bus (0 or 1)");
    }
    
    /* Initialize I2C */
    i2c_init(i2c, baudrate);
    
    /* Set up pins */
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);
    
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
    
    if (argc < 3) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "I2C.write requires bus, address, data");
    }
    
    int bus = (int)js_get_number_arg(args, argc, 0, 0);
    uint8_t addr = (uint8_t)js_get_number_arg(args, argc, 1, 0);
    
    i2c_inst_t *i2c = get_i2c_instance(bus);
    if (i2c == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid I2C bus");
    }
    
    uint8_t buffer[MAX_I2C_TRANSFER];
    size_t len = 0;
    
    /* Check if data is an array or single value */
    if (jerry_value_is_array(args[2])) {
        uint32_t array_len = jerry_array_length(args[2]);
        if (array_len > MAX_I2C_TRANSFER) {
            array_len = MAX_I2C_TRANSFER;
        }
        
        for (uint32_t i = 0; i < array_len; i++) {
            jerry_value_t elem = jerry_object_get_index(args[2], i);
            buffer[len++] = (uint8_t)jerry_value_as_number(elem);
            jerry_value_free(elem);
        }
    } else {
        buffer[0] = (uint8_t)js_get_number_arg(args, argc, 2, 0);
        len = 1;
    }
    
    /* Write to I2C */
    int result = i2c_write_blocking(i2c, addr, buffer, len, false);
    
    if (result < 0) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "I2C write failed");
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
    
    if (argc < 3) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "I2C.read requires bus, address, length");
    }
    
    int bus = (int)js_get_number_arg(args, argc, 0, 0);
    uint8_t addr = (uint8_t)js_get_number_arg(args, argc, 1, 0);
    size_t len = (size_t)js_get_number_arg(args, argc, 2, 1);
    
    i2c_inst_t *i2c = get_i2c_instance(bus);
    if (i2c == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid I2C bus");
    }
    
    if (len > MAX_I2C_TRANSFER) {
        len = MAX_I2C_TRANSFER;
    }
    
    uint8_t buffer[MAX_I2C_TRANSFER];
    
    /* Read from I2C */
    int result = i2c_read_blocking(i2c, addr, buffer, len, false);
    
    if (result < 0) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "I2C read failed");
    }
    
    /* Create result array */
    jerry_value_t array = jerry_array((uint32_t)result);
    
    for (int i = 0; i < result; i++) {
        jerry_value_t byte_val = jerry_number((double)buffer[i]);
        jerry_object_set_index(array, (uint32_t)i, byte_val);
        jerry_value_free(byte_val);
    }
    
    return array;
}

/*
 * Register I2C bindings
 */
void js_bind_i2c(void) {
    jerry_value_t i2c = jerry_object();
    
    js_set_function(i2c, "init", i2c_init_handler);
    js_set_function(i2c, "write", i2c_write_handler);
    js_set_function(i2c, "read", i2c_read_handler);
    
    js_register_global("I2C", i2c);
    jerry_value_free(i2c);
}
