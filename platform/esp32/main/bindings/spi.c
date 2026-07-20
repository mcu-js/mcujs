/* MCU.js SPI binding for ESP32-S3 master buses. */

#include "binding_utils.h"
#include "bindings.h"
#include "jerryscript.h"
#include "pin_policy.h"
#include "runtime_features.h"
#include "spi_options.h"
#include "validation.h"

#include "driver/spi_master.h"
#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(MCUJS_RUNTIME_SPI_MAX_TRANSFER_BYTES <=
                   SOC_SPI_MAXIMUM_BUFFER_SIZE,
               "advertised ESP SPI transfer exceeds polling driver limit");

typedef struct {
    bool bus_allocated;
    bool initialized;
    spi_host_device_t host;
    spi_device_handle_t device;
    int sck;
    int mosi;
    int miso;
} spi_bus_state_t;

static spi_bus_state_t s_buses[2] = {
    {.host = SPI2_HOST, .sck = -1, .mosi = -1, .miso = -1},
    {.host = SPI3_HOST, .sck = -1, .mosi = -1, .miso = -1},
};

static spi_bus_state_t *get_bus(int index) {
    return index >= 0 && index < 2 ? &s_buses[index] : NULL;
}

static mcujs_pin_owner_t bus_owner(int index) {
    return index == 0 ? MCUJS_PIN_OWNER_SPI0 : MCUJS_PIN_OWNER_SPI1;
}

static jerry_value_t throw_spi_error(mcujs_operational_error_t error,
                                     esp_err_t native_error, int index,
                                     int pin, const char *message) {
    const mcujs_error_details_t details = {
        .resource = "spi",
        .has_pin = pin >= 0,
        .pin = pin,
        .has_bus = true,
        .bus = index,
        .has_limit = error == MCUJS_ERROR_RESOURCE_EXHAUSTED,
        .limit = 2,
        .has_native_code = native_error != ESP_OK,
        .native_code = native_error,
    };
    return mcujs_throw_operational_error(error, message, &details);
}

static mcujs_operational_error_t map_spi_error(esp_err_t error) {
    if (error == ESP_ERR_INVALID_STATE || error == ESP_ERR_TIMEOUT) {
        return MCUJS_ERROR_BUSY;
    }
    if (error == ESP_ERR_NO_MEM) return MCUJS_ERROR_RESOURCE_EXHAUSTED;
    if (error == ESP_ERR_NOT_SUPPORTED) return MCUJS_ERROR_NOT_SUPPORTED;
    return MCUJS_ERROR_IO;
}

static int represented_spi_frequency(int requested) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    int actual = spi_get_actual_clock(80000000, requested, 128);
#pragma GCC diagnostic pop
    return actual;
}

static jerry_value_t throw_spi_native(esp_err_t error, int index,
                                      const char *message) {
    return throw_spi_error(map_spi_error(error), error, index, -1, message);
}

static esp_err_t release_routes(spi_bus_state_t *bus, int index) {
    mcujs_pin_owner_t owner = bus_owner(index);
    esp_err_t result = ESP_OK;
    int *routes[] = {&bus->sck, &bus->mosi, &bus->miso};
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        if (*routes[i] >= 0) {
            esp_err_t error = gpio_reset_pin((gpio_num_t)*routes[i]);
            if (error == ESP_OK) {
                mcujs_pin_release(*routes[i], owner);
                *routes[i] = -1;
            } else if (result == ESP_OK) {
                result = error;
            }
        }
    }
    return result;
}

static esp_err_t release_bus(spi_bus_state_t *bus, int index) {
    esp_err_t err = ESP_OK;
    if (bus->initialized) {
        err = spi_bus_remove_device(bus->device);
        if (err != ESP_OK) return err;
        bus->device = NULL;
        bus->initialized = false;
    }
    if (bus->bus_allocated) {
        err = spi_bus_free(bus->host);
        if (err != ESP_OK) return err;
        bus->bus_allocated = false;
    }

    return release_routes(bus, index);
}

static jerry_value_t spi_init_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    mcujs_spi_init_options_t options;
    jerry_value_t parsed = mcujs_parse_spi_init_args(args, argc, &options);
    if (jerry_value_is_exception(parsed)) return parsed;
    jerry_value_free(parsed);
    int index = options.bus;
    int sck = options.sck;
    int mosi = options.mosi;
    int miso = options.miso;
    int baudrate = options.frequency;

    spi_bus_state_t *bus = get_bus(index);
    if (bus == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid SPI bus (0 or 1)");
    }
    if (!mcujs_pin_is_peripheral_output(sck) || !mcujs_pin_is_peripheral_output(mosi) ||
        !mcujs_pin_is_peripheral(miso) || sck == mosi || sck == miso || mosi == miso) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Invalid SPI pins (use distinct GPIO1..GPIO9)");
    }
    int represented = represented_spi_frequency(baudrate);
    if (represented != baudrate) {
        return throw_spi_error(
            MCUJS_ERROR_NOT_SUPPORTED, ESP_OK, index, -1,
            "SPI frequency cannot be represented exactly");
    }
    mcujs_pin_owner_t owner = bus_owner(index);
    if (!mcujs_pin_can_claim(sck, owner) || !mcujs_pin_can_claim(mosi, owner) ||
        !mcujs_pin_can_claim(miso, owner)) {
        return throw_spi_error(MCUJS_ERROR_BUSY, ESP_OK, index, sck,
                               "SPI pin is owned by another peripheral");
    }
    esp_err_t release_error = release_bus(bus, index);
    if (release_error != ESP_OK) {
        return throw_spi_native(release_error, index,
                                "SPI reinitialization failed");
    }
    if (!mcujs_pin_claim(sck, owner)) {
        return throw_spi_error(MCUJS_ERROR_BUSY, ESP_OK, index, sck,
                               "SPI SCK pin claim failed");
    }
    bus->sck = sck;
    if (!mcujs_pin_claim(mosi, owner)) {
        esp_err_t cleanup_error = release_routes(bus, index);
        if (cleanup_error != ESP_OK) {
            return throw_spi_native(cleanup_error, index,
                                    "SPI route rollback failed");
        }
        return throw_spi_error(MCUJS_ERROR_BUSY, ESP_OK, index, mosi,
                               "SPI MOSI pin claim failed");
    }
    bus->mosi = mosi;
    if (!mcujs_pin_claim(miso, owner)) {
        esp_err_t cleanup_error = release_routes(bus, index);
        if (cleanup_error != ESP_OK) {
            return throw_spi_native(cleanup_error, index,
                                    "SPI route rollback failed");
        }
        return throw_spi_error(MCUJS_ERROR_BUSY, ESP_OK, index, miso,
                               "SPI MISO pin claim failed");
    }
    bus->miso = miso;

    spi_bus_config_t bus_config = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = MCUJS_RUNTIME_SPI_MAX_TRANSFER_BYTES,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0,
    };
    esp_err_t err = spi_bus_initialize(bus->host, &bus_config, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        esp_err_t cleanup_error = release_routes(bus, index);
        if (cleanup_error != ESP_OK) {
            return throw_spi_native(cleanup_error, index,
                                    "SPI bus initialization rollback failed");
        }
        return throw_spi_native(err, index, "SPI bus initialization failed");
    }
    bus->bus_allocated = true;

    spi_device_interface_config_t device_config = {
        .mode = options.mode,
        .clock_speed_hz = baudrate,
        .spics_io_num = -1,
        .queue_size = 1,
    };
    err = spi_bus_add_device(bus->host, &device_config, &bus->device);
    if (err != ESP_OK) {
        bus->device = NULL;
        esp_err_t cleanup_error = spi_bus_free(bus->host);
        if (cleanup_error == ESP_OK) {
            bus->bus_allocated = false;
            cleanup_error = release_routes(bus, index);
        }
        if (cleanup_error != ESP_OK) {
            return throw_spi_native(cleanup_error, index,
                                    "SPI device initialization rollback failed");
        }
        return throw_spi_native(err, index, "SPI device initialization failed");
    }
    bus->initialized = true;
    return jerry_undefined();
}

static jerry_value_t spi_transfer_handler(const jerry_call_info_t *info,
                                          const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    int index;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &index);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "SPI bus must be a finite number",
                              "SPI bus must be an integer");
    }
    spi_bus_state_t *bus = get_bus(index);
    if (bus == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid SPI bus (0 or 1)");
    }
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "SPI.transfer requires data");
    }

    uint8_t tx[MCUJS_RUNTIME_SPI_MAX_TRANSFER_BYTES];
    uint8_t rx[MCUJS_RUNTIME_SPI_MAX_TRANSFER_BYTES];
    size_t length = 0;
    bool array_input = jerry_value_is_array(args[1]);
    if (array_input) {
        jerry_value_t exception;
        status = mcujs_get_byte_array(
            args, argc, 1, tx, sizeof(tx), 1,
            MCUJS_RUNTIME_SPI_MAX_TRANSFER_BYTES, &length, &exception);
        if (status == MCUJS_ARG_EXCEPTION) return exception;
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status,
                                   "SPI data must be an array of numbers",
                                   "SPI data exceeds maxTransferBytes");
        }
    } else {
        status = mcujs_value_to_byte(args[1], &tx[0]);
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status, "SPI data must be a finite number or array",
                                  "SPI data byte must be an integer 0..255");
        }
        length = 1;
    }
    if (!bus->initialized) {
        return throw_spi_error(MCUJS_ERROR_BUSY, ESP_OK, index, -1,
                               "SPI bus is not initialized");
    }
    memset(rx, 0, length);

    spi_transaction_t transaction = {
        .length = length * 8u,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t error = spi_device_transmit(bus->device, &transaction);
    if (error != ESP_OK) {
        return throw_spi_native(error, index, "SPI transfer failed");
    }
    if (!array_input) {
        return jerry_number((double)rx[0]);
    }

    jerry_value_t result = jerry_array((uint32_t)length);
    if (jerry_value_is_exception(result)) return result;
    for (size_t i = 0; i < length; i++) {
        jerry_value_t value = jerry_number((double)rx[i]);
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

jerry_value_t js_create_spi_module(void) {
    jerry_value_t spi = jerry_object();
    js_set_function(spi, "init", spi_init_handler);
    js_set_function(spi, "transfer", spi_transfer_handler);
    return spi;
}

void js_bind_spi(void) {
    jerry_value_t spi = js_create_spi_module();
    js_register_global("SPI", spi);
    jerry_value_free(spi);
}
