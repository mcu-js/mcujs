/* MCU.js SPI binding for ESP32-S3 master buses. */

#include "binding_utils.h"
#include "bindings.h"
#include "jerryscript.h"
#include "pin_policy.h"

#include "driver/spi_master.h"
#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MCUJS_SPI_MAX_TRANSFER SOC_SPI_MAXIMUM_BUFFER_SIZE

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

static void release_routes(spi_bus_state_t *bus, int index) {
    mcujs_pin_owner_t owner = bus_owner(index);
    int *routes[] = {&bus->sck, &bus->mosi, &bus->miso};
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        if (*routes[i] >= 0) {
            (void)gpio_reset_pin((gpio_num_t)*routes[i]);
            mcujs_pin_release(*routes[i], owner);
            *routes[i] = -1;
        }
    }
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

    release_routes(bus, index);
    return ESP_OK;
}

static jerry_value_t spi_init_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    int index, sck, mosi, miso, baudrate;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &index);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "SPI bus must be a finite number",
                              "SPI bus must be an integer");
    }
    status = mcujs_get_integer(args, argc, 1, &sck);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "SPI SCK pin must be a finite number",
                              "SPI SCK pin must be an integer");
    }
    status = mcujs_get_integer(args, argc, 2, &mosi);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "SPI MOSI pin must be a finite number",
                              "SPI MOSI pin must be an integer");
    }
    status = mcujs_get_integer(args, argc, 3, &miso);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "SPI MISO pin must be a finite number",
                              "SPI MISO pin must be an integer");
    }
    status = mcujs_get_integer(args, argc, 4, &baudrate);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "SPI baudrate must be a finite number",
                              "SPI baudrate must be an integer");
    }

    spi_bus_state_t *bus = get_bus(index);
    if (bus == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid SPI bus (0 or 1)");
    }
    if (!mcujs_pin_is_peripheral_output(sck) || !mcujs_pin_is_peripheral_output(mosi) ||
        !mcujs_pin_is_peripheral(miso) || sck == mosi || sck == miso || mosi == miso) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Invalid SPI pins (use distinct GPIO1..GPIO9)");
    }
    if (baudrate < 1 || baudrate > 40000000) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "SPI baudrate must be 1..40000000");
    }
    mcujs_pin_owner_t owner = bus_owner(index);
    if (!mcujs_pin_can_claim(sck, owner) || !mcujs_pin_can_claim(mosi, owner) ||
        !mcujs_pin_can_claim(miso, owner)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "SPI pin is owned by another peripheral");
    }
    if (release_bus(bus, index) != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "SPI reinitialization failed");
    }
    if (!mcujs_pin_claim(sck, owner)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "SPI SCK pin claim failed");
    }
    bus->sck = sck;
    if (!mcujs_pin_claim(mosi, owner)) {
        release_routes(bus, index);
        return jerry_throw_sz(JERRY_ERROR_COMMON, "SPI MOSI pin claim failed");
    }
    bus->mosi = mosi;
    if (!mcujs_pin_claim(miso, owner)) {
        release_routes(bus, index);
        return jerry_throw_sz(JERRY_ERROR_COMMON, "SPI MISO pin claim failed");
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
        .max_transfer_sz = MCUJS_SPI_MAX_TRANSFER,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0,
    };
    esp_err_t err = spi_bus_initialize(bus->host, &bus_config, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        release_routes(bus, index);
        return jerry_throw_sz(JERRY_ERROR_COMMON, "SPI bus initialization failed");
    }
    bus->bus_allocated = true;

    spi_device_interface_config_t device_config = {
        .mode = 0,
        .clock_speed_hz = baudrate,
        .spics_io_num = -1,
        .queue_size = 1,
    };
    err = spi_bus_add_device(bus->host, &device_config, &bus->device);
    if (err != ESP_OK) {
        bus->device = NULL;
        if (spi_bus_free(bus->host) == ESP_OK) {
            bus->bus_allocated = false;
            release_routes(bus, index);
        }
        return jerry_throw_sz(JERRY_ERROR_COMMON, "SPI device initialization failed");
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
    if (bus == NULL || !bus->initialized) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "SPI bus is not initialized");
    }
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "SPI.transfer requires data");
    }

    uint8_t tx[MCUJS_SPI_MAX_TRANSFER];
    uint8_t rx[MCUJS_SPI_MAX_TRANSFER];
    size_t length = 0;
    bool array_input = jerry_value_is_array(args[1]);
    if (array_input) {
        uint32_t array_length = jerry_array_length(args[1]);
        if (array_length == 0 || array_length > MCUJS_SPI_MAX_TRANSFER) {
            return jerry_throw_sz(JERRY_ERROR_RANGE, "SPI data length must be 1..64");
        }
        for (uint32_t i = 0; i < array_length; i++) {
            jerry_value_t value = jerry_object_get_index(args[1], i);
            status = mcujs_value_to_byte(value, &tx[length]);
            jerry_value_free(value);
            if (status != MCUJS_ARG_OK) {
                return mcujs_throw_arg(status, "SPI data must contain finite numbers",
                                      "SPI data bytes must be integers 0..255");
            }
            length++;
        }
    } else {
        status = mcujs_value_to_byte(args[1], &tx[0]);
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status, "SPI data must be a finite number or array",
                                  "SPI data byte must be an integer 0..255");
        }
        length = 1;
    }
    memset(rx, 0, length);

    spi_transaction_t transaction = {
        .length = length * 8u,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    if (spi_device_transmit(bus->device, &transaction) != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "SPI transfer failed");
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
