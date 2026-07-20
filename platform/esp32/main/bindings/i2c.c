/* MCU.js I2C binding for ESP32-S3 master controllers. */

#include "binding_utils.h"
#include "bindings.h"
#include "i2c_options.h"
#include "jerryscript.h"
#include "pin_policy.h"
#include "runtime_features.h"

#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MCUJS_I2C_TIMEOUT_MS 1000
#define MCUJS_I2C_SOURCE_HZ 40000000u

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


/* ESP-IDF 5.3.2 selects the 40 MHz XTAL first for clk_flags=0 on ESP32-S3,
 * then programs an integer source divider and integer SCL half-cycle. */
static bool frequency_is_exact(uint32_t frequency) {
    uint32_t clock_divider = MCUJS_I2C_SOURCE_HZ / (frequency * 1024u) + 1u;
    uint32_t divided_clock = MCUJS_I2C_SOURCE_HZ / clock_divider;
    uint32_t half_cycle = divided_clock / frequency / 2u;
    return half_cycle > 0 &&
           (uint64_t)frequency * clock_divider * half_cycle * 2u ==
               MCUJS_I2C_SOURCE_HZ;
}

static mcujs_pin_owner_t bus_owner(int index) {
    return index == 0 ? MCUJS_PIN_OWNER_I2C0 : MCUJS_PIN_OWNER_I2C1;
}

static jerry_value_t throw_i2c_error(mcujs_operational_error_t error,
                                     esp_err_t native_error, int index,
                                     int pin, const char *message) {
    const mcujs_error_details_t details = {
        .resource = "i2c",
        .has_pin = pin >= 0,
        .pin = pin,
        .has_bus = true,
        .bus = index,
        .has_limit = error == MCUJS_ERROR_RESOURCE_EXHAUSTED,
        .limit = MCUJS_RUNTIME_I2C_MAX_TRANSFER_BYTES,
        .has_native_code = native_error != ESP_OK,
        .native_code = native_error,
    };
    return mcujs_throw_operational_error(error, message, &details);
}

static mcujs_operational_error_t map_i2c_error(esp_err_t error,
                                               bool transfer) {
    if (transfer && error == ESP_FAIL) return MCUJS_ERROR_NO_DEVICE;
    if (error == ESP_ERR_INVALID_STATE ||
        (!transfer && error == ESP_ERR_TIMEOUT)) {
        return MCUJS_ERROR_BUSY;
    }
    if (error == ESP_ERR_NO_MEM) return MCUJS_ERROR_RESOURCE_EXHAUSTED;
    if (error == ESP_ERR_NOT_SUPPORTED) return MCUJS_ERROR_NOT_SUPPORTED;
    return MCUJS_ERROR_IO;
}

static jerry_value_t throw_i2c_native(esp_err_t error, int index,
                                      bool transfer, const char *message) {
    return throw_i2c_error(map_i2c_error(error, transfer), error, index, -1,
                           message);
}

static esp_err_t release_routes(i2c_bus_state_t *bus, int index) {
    mcujs_pin_owner_t owner = bus_owner(index);
    esp_err_t result = ESP_OK;
    if (bus->sda >= 0) {
        esp_err_t error = gpio_reset_pin((gpio_num_t)bus->sda);
        if (error == ESP_OK) {
            mcujs_pin_release(bus->sda, owner);
            bus->sda = -1;
        } else {
            result = error;
        }
    }
    if (bus->scl >= 0) {
        esp_err_t error = gpio_reset_pin((gpio_num_t)bus->scl);
        if (error == ESP_OK) {
            mcujs_pin_release(bus->scl, owner);
            bus->scl = -1;
        } else if (result == ESP_OK) {
            result = error;
        }
    }
    return result;
}

static esp_err_t release_bus(i2c_bus_state_t *bus, int index) {
    if (bus->initialized) {
        esp_err_t error = i2c_driver_delete(bus->port);
        if (error != ESP_OK) return error;
        bus->initialized = false;
    }
    return release_routes(bus, index);
}

static jerry_value_t i2c_init_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    mcujs_i2c_init_options_t options;
    jerry_value_t parsed = mcujs_parse_i2c_init_args(args, argc, &options);
    if (jerry_value_is_exception(parsed)) return parsed;
    jerry_value_free(parsed);
    int index = options.bus;
    int sda = options.sda;
    int scl = options.scl;
    int baudrate = options.frequency;

    i2c_bus_state_t *bus = get_bus(index);
    if (bus == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid I2C bus (0 or 1)");
    }
    if (!mcujs_pin_is_peripheral_output(sda) || !mcujs_pin_is_peripheral_output(scl) ||
        sda == scl) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Invalid I2C SDA/SCL pins (use distinct GPIO1..GPIO9)");
    }

    if (!frequency_is_exact((uint32_t)baudrate)) {
        return throw_i2c_error(
            MCUJS_ERROR_NOT_SUPPORTED, ESP_OK, index, -1,
            "I2C baudrate cannot be represented exactly");
    }
    mcujs_pin_owner_t owner = bus_owner(index);
    if (!mcujs_pin_can_claim(sda, owner) || !mcujs_pin_can_claim(scl, owner)) {
        return throw_i2c_error(MCUJS_ERROR_BUSY, ESP_OK, index, sda,
                               "I2C pin is owned by another peripheral");
    }
    esp_err_t release_error = release_bus(bus, index);
    if (release_error != ESP_OK) {
        return throw_i2c_native(release_error, index, false,
                                "I2C reinitialization failed");
    }
    if (!mcujs_pin_claim(sda, owner)) {
        return throw_i2c_error(MCUJS_ERROR_BUSY, ESP_OK, index, sda,
                               "I2C SDA pin claim failed");
    }
    bus->sda = sda;
    if (!mcujs_pin_claim(scl, owner)) {
        esp_err_t cleanup_error = release_routes(bus, index);
        if (cleanup_error != ESP_OK) {
            return throw_i2c_native(cleanup_error, index, false,
                                    "I2C route rollback failed");
        }
        return throw_i2c_error(MCUJS_ERROR_BUSY, ESP_OK, index, scl,
                               "I2C SCL pin claim failed");
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
        esp_err_t cleanup_error = release_routes(bus, index);
        if (cleanup_error != ESP_OK) {
            return throw_i2c_native(cleanup_error, index, false,
                                    "I2C initialization rollback failed");
        }
        return throw_i2c_native(err, index, false,
                                "I2C initialization failed");
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
    if (*bus == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid I2C bus (0 or 1)");
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

    uint8_t buffer[MCUJS_RUNTIME_I2C_MAX_TRANSFER_BYTES];
    size_t length = 0;
    if (jerry_value_is_array(args[2])) {
        jerry_value_t exception;
        mcujs_arg_status_t status = mcujs_get_byte_array(
            args, argc, 2, buffer, sizeof(buffer), 1,
            MCUJS_RUNTIME_I2C_MAX_TRANSFER_BYTES, &length, &exception);
        if (status == MCUJS_ARG_EXCEPTION) return exception;
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status,
                                  "I2C data must be an array of numbers",
                                  "I2C data exceeds maxTransferBytes");
        }
    } else {
        mcujs_arg_status_t status = mcujs_value_to_byte(args[2], &buffer[0]);
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status, "I2C data must be a finite number or array",
                                  "I2C data byte must be an integer 0..255");
        }
        length = 1;
    }

    if (!bus->initialized) {
        return throw_i2c_error(MCUJS_ERROR_BUSY, ESP_OK, (int)bus->port, -1,
                               "I2C bus is not initialized");
    }

    esp_err_t err = i2c_master_write_to_device(
        bus->port, (uint8_t)address, buffer, length, pdMS_TO_TICKS(MCUJS_I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        return throw_i2c_native(err, (int)bus->port, true,
                                "I2C write failed");
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
    if (requested < 1 ||
        requested > MCUJS_RUNTIME_I2C_MAX_TRANSFER_BYTES) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "I2C read length exceeds maxTransferBytes");
    }
    if (!bus->initialized) {
        return throw_i2c_error(MCUJS_ERROR_BUSY, ESP_OK, (int)bus->port, -1,
                               "I2C bus is not initialized");
    }

    uint8_t buffer[MCUJS_RUNTIME_I2C_MAX_TRANSFER_BYTES];
    esp_err_t err = i2c_master_read_from_device(
        bus->port, (uint8_t)address, buffer, (size_t)requested,
        pdMS_TO_TICKS(MCUJS_I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        return throw_i2c_native(err, (int)bus->port, true,
                                "I2C read failed");
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
