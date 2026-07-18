/* MCU.js I2C binding for ESP32-S3 master controllers. */

#include "binding_utils.h"
#include "bindings.h"
#include "jerryscript.h"
#include "pin_policy.h"

#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MCUJS_I2C_MAX_TRANSFER 256
#define MCUJS_I2C_TIMEOUT_MS 1000

typedef struct {
    bool initialized;
    i2c_port_t port;
    int sda;
    int scl;
} i2c_bus_state_t;

static i2c_bus_state_t s_buses[2] = {
    {.port = I2C_NUM_0, .sda = -1, .scl = -1},
    {.port = I2C_NUM_1, .sda = -1, .scl = -1},
};

static i2c_bus_state_t *get_bus(int index) {
    return index >= 0 && index < 2 ? &s_buses[index] : NULL;
}

static mcujs_pin_owner_t bus_owner(int index) {
    return index == 0 ? MCUJS_PIN_OWNER_I2C0 : MCUJS_PIN_OWNER_I2C1;
}

static void release_routes(i2c_bus_state_t *bus, int index) {
    mcujs_pin_owner_t owner = bus_owner(index);
    if (bus->sda >= 0) {
        (void)gpio_reset_pin((gpio_num_t)bus->sda);
        mcujs_pin_release(bus->sda, owner);
        bus->sda = -1;
    }
    if (bus->scl >= 0) {
        (void)gpio_reset_pin((gpio_num_t)bus->scl);
        mcujs_pin_release(bus->scl, owner);
        bus->scl = -1;
    }
}

static esp_err_t release_bus(i2c_bus_state_t *bus, int index) {
    if (!bus->initialized) return ESP_OK;
    esp_err_t err = i2c_driver_delete(bus->port);
    if (err != ESP_OK) return err;
    bus->initialized = false;
    release_routes(bus, index);
    return ESP_OK;
}

static jerry_value_t i2c_init_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    int index, sda, scl, baudrate;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &index);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C bus must be a finite number",
                              "I2C bus must be an integer");
    }
    status = mcujs_get_integer(args, argc, 1, &sda);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C SDA pin must be a finite number",
                              "I2C SDA pin must be an integer");
    }
    status = mcujs_get_integer(args, argc, 2, &scl);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C SCL pin must be a finite number",
                              "I2C SCL pin must be an integer");
    }
    status = mcujs_get_integer(args, argc, 3, &baudrate);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C baudrate must be a finite number",
                              "I2C baudrate must be an integer");
    }

    i2c_bus_state_t *bus = get_bus(index);
    if (bus == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid I2C bus (0 or 1)");
    }
    if (!mcujs_pin_is_peripheral_output(sda) || !mcujs_pin_is_peripheral_output(scl) ||
        sda == scl) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Invalid I2C SDA/SCL pins (use distinct GPIO1..GPIO9)");
    }
    if (baudrate < 1 || baudrate > 1000000) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "I2C baudrate must be 1..1000000");
    }
    mcujs_pin_owner_t owner = bus_owner(index);
    if (!mcujs_pin_can_claim(sda, owner) || !mcujs_pin_can_claim(scl, owner)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "I2C pin is owned by another peripheral");
    }
    if (release_bus(bus, index) != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "I2C reinitialization failed");
    }
    if (!mcujs_pin_claim(sda, owner)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "I2C SDA pin claim failed");
    }
    bus->sda = sda;
    if (!mcujs_pin_claim(scl, owner)) {
        release_routes(bus, index);
        return jerry_throw_sz(JERRY_ERROR_COMMON, "I2C SCL pin claim failed");
    }
    bus->scl = scl;

    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (gpio_num_t)sda,
        .scl_io_num = (gpio_num_t)scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = (uint32_t)baudrate,
        .clk_flags = 0,
    };
    esp_err_t err = i2c_param_config(bus->port, &config);
    if (err == ESP_OK) {
        err = i2c_driver_install(bus->port, I2C_MODE_MASTER, 0, 0, 0);
    }
    if (err != ESP_OK) {
        release_routes(bus, index);
        return jerry_throw_sz(JERRY_ERROR_COMMON, "I2C initialization failed");
    }
    bus->initialized = true;
    return jerry_undefined();
}

static jerry_value_t parse_bus_address(const jerry_value_t args[], jerry_length_t argc,
                                       i2c_bus_state_t **bus, int *address) {
    int index;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &index);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C bus must be a finite number",
                              "I2C bus must be an integer");
    }
    status = mcujs_get_integer(args, argc, 1, address);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C address must be a finite number",
                              "I2C address must be an integer");
    }
    *bus = get_bus(index);
    if (*bus == NULL || !(*bus)->initialized) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "I2C bus is not initialized");
    }
    if (*address < 0 || *address > 0x7f) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid 7-bit I2C address");
    }
    return jerry_undefined();
}

static jerry_value_t i2c_write_handler(const jerry_call_info_t *info,
                                       const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    i2c_bus_state_t *bus;
    int address;
    jerry_value_t validation = parse_bus_address(args, argc, &bus, &address);
    if (jerry_value_is_exception(validation)) return validation;
    jerry_value_free(validation);
    if (argc < 3) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "I2C.write requires data");
    }

    uint8_t buffer[MCUJS_I2C_MAX_TRANSFER];
    size_t length = 0;
    if (jerry_value_is_array(args[2])) {
        uint32_t array_length = jerry_array_length(args[2]);
        if (array_length == 0 || array_length > MCUJS_I2C_MAX_TRANSFER) {
            return jerry_throw_sz(JERRY_ERROR_RANGE, "I2C data length must be 1..256");
        }
        for (uint32_t i = 0; i < array_length; i++) {
            jerry_value_t value = jerry_object_get_index(args[2], i);
            mcujs_arg_status_t status = mcujs_value_to_byte(value, &buffer[length]);
            jerry_value_free(value);
            if (status != MCUJS_ARG_OK) {
                return mcujs_throw_arg(status, "I2C data must contain finite numbers",
                                      "I2C data bytes must be integers 0..255");
            }
            length++;
        }
    } else {
        mcujs_arg_status_t status = mcujs_value_to_byte(args[2], &buffer[0]);
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status, "I2C data must be a finite number or array",
                                  "I2C data byte must be an integer 0..255");
        }
        length = 1;
    }

    esp_err_t err = i2c_master_write_to_device(
        bus->port, (uint8_t)address, buffer, length, pdMS_TO_TICKS(MCUJS_I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "I2C write failed");
    }
    return jerry_number((double)length);
}

static jerry_value_t i2c_read_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    i2c_bus_state_t *bus;
    int address;
    jerry_value_t validation = parse_bus_address(args, argc, &bus, &address);
    if (jerry_value_is_exception(validation)) return validation;
    jerry_value_free(validation);

    int requested;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 2, &requested);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C read length must be a finite number",
                              "I2C read length must be an integer");
    }
    if (requested < 1 || requested > MCUJS_I2C_MAX_TRANSFER) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "I2C read length must be 1..256");
    }

    uint8_t buffer[MCUJS_I2C_MAX_TRANSFER];
    esp_err_t err = i2c_master_read_from_device(
        bus->port, (uint8_t)address, buffer, (size_t)requested,
        pdMS_TO_TICKS(MCUJS_I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "I2C read failed");
    }

    jerry_value_t result = jerry_array((uint32_t)requested);
    if (jerry_value_is_exception(result)) return result;
    for (int i = 0; i < requested; i++) {
        jerry_value_t value = jerry_number((double)buffer[i]);
        jerry_value_t set_result = jerry_object_set_index(result, (uint32_t)i, value);
        jerry_value_free(value);
        if (jerry_value_is_exception(set_result)) {
            jerry_value_free(result);
            return set_result;
        }
        jerry_value_free(set_result);
    }
    return result;
}

jerry_value_t js_create_i2c_module(void) {
    jerry_value_t i2c = jerry_object();
    js_set_function(i2c, "init", i2c_init_handler);
    js_set_function(i2c, "write", i2c_write_handler);
    js_set_function(i2c, "read", i2c_read_handler);
    return i2c;
}

void js_bind_i2c(void) {
    jerry_value_t i2c = js_create_i2c_module();
    js_register_global("I2C", i2c);
    jerry_value_free(i2c);
}
