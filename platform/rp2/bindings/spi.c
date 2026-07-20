/*
 * mcujs - SPI Bindings
 *
 * Implements: SPI.init(), SPI.transfer(), SPI.writeBufferDMA()
 */

#include "bindings.h"
#include "graphics.h"
#include "jerryscript.h"
#include "pin_policy.h"
#include "spi_options.h"
#include "validation.h"

#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include <stdbool.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name,
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);

#define MCUJS_SPI_NO_NATIVE_CODE INT_MIN

static int s_dma_tx_channel = -1;

typedef struct {
    bool initialized;
    int sck;
    int mosi;
    int miso;
} spi_bus_state_t;

static spi_bus_state_t s_spi_buses[2] = {
    {.sck = -1, .mosi = -1, .miso = -1},
    {.sck = -1, .mosi = -1, .miso = -1},
};

static spi_inst_t *get_spi_instance(int bus) {
    switch (bus) {
        case 0: return spi0;
        case 1: return spi1;
        default: return NULL;
    }
}

static mcujs_rp2_pin_owner_t spi_pin_owner(int bus) {
    return bus == 0 ? MCUJS_RP2_PIN_OWNER_SPI0 : MCUJS_RP2_PIN_OWNER_SPI1;
}

static uint32_t represented_spi_frequency(uint32_t frequency) {
    uint32_t source = clock_get_hz(clk_peri);
    uint32_t prescale;
    for (prescale = 2; prescale <= 254; prescale += 2) {
        if ((uint64_t)source <
            (uint64_t)prescale * 256u * frequency) {
            break;
        }
    }
    if (prescale > 254) return 0;

    uint32_t postdiv;
    for (postdiv = 256; postdiv > 1; postdiv--) {
        if (source / (prescale * (postdiv - 1u)) > frequency) break;
    }
    return source / (prescale * postdiv);
}

static jerry_value_t throw_spi_error(mcujs_operational_error_t error, int bus,
                                     int pin, int native_code,
                                     const char *message) {
    const mcujs_error_details_t details = {
        .resource = "spi",
        .has_pin = pin >= 0,
        .pin = pin,
        .has_bus = true,
        .bus = bus,
        .has_native_code = native_code != MCUJS_SPI_NO_NATIVE_CODE,
        .native_code = native_code,
    };
    return mcujs_throw_operational_error(error, message, &details);
}

static jerry_value_t throw_dma_exhausted(int bus) {
    const mcujs_error_details_t details = {
        .resource = "spiDmaChannel",
        .has_bus = true,
        .bus = bus,
        .has_limit = true,
        .limit = NUM_DMA_CHANNELS,
    };
    return mcujs_throw_operational_error(
        MCUJS_ERROR_RESOURCE_EXHAUSTED,
        "No SPI DMA channel is available", &details);
}

static bool route_contains(const spi_bus_state_t *route, int pin) {
    return route->sck == pin || route->mosi == pin || route->miso == pin;
}

static void reset_and_release_spi_pin(int pin, mcujs_rp2_pin_owner_t owner) {
    if (pin < 0) return;
    gpio_set_function((uint)pin, GPIO_FUNC_SIO);
    gpio_init((uint)pin);
    mcujs_rp2_pin_release(pin, owner);
}

static void release_old_spi_route(spi_bus_state_t *old_route,
                                  const spi_bus_state_t *new_route,
                                  mcujs_rp2_pin_owner_t owner) {
    const int old_pins[] = {old_route->sck, old_route->mosi, old_route->miso};
    for (size_t i = 0; i < sizeof(old_pins) / sizeof(old_pins[0]); i++) {
        if (old_pins[i] >= 0 && !route_contains(new_route, old_pins[i])) {
            reset_and_release_spi_pin(old_pins[i], owner);
        }
    }
    old_route->initialized = false;
    old_route->sck = -1;
    old_route->mosi = -1;
    old_route->miso = -1;
}

static jerry_value_t spi_init_handler(const jerry_call_info_t *call_info_p,
                                      const jerry_value_t args[],
                                      const jerry_length_t argc) {
    (void)call_info_p;

    mcujs_spi_init_options_t options;
    jerry_value_t parsed = mcujs_parse_spi_init_args(args, argc, &options);
    if (jerry_value_is_exception(parsed)) return parsed;
    jerry_value_free(parsed);
    int bus = options.bus;
    int sck_pin = options.sck;
    int mosi_pin = options.mosi;
    int miso_pin = options.miso;
    int baudrate = options.frequency;

    spi_inst_t *spi = get_spi_instance(bus);
    if (spi == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid SPI bus (0 or 1)");
    }

    uint32_t represented = represented_spi_frequency((uint32_t)baudrate);
    if (represented != (uint32_t)baudrate) {
        return throw_spi_error(
            MCUJS_ERROR_NOT_SUPPORTED, bus, -1, (int)represented,
            "SPI frequency cannot be represented exactly");
    }

    spi_bus_state_t candidate = {
        .initialized = false,
        .sck = sck_pin,
        .mosi = mosi_pin,
        .miso = miso_pin,
    };
    spi_bus_state_t *active = &s_spi_buses[bus];
    mcujs_rp2_pin_owner_t owner = spi_pin_owner(bus);
    const int pins[] = {sck_pin, mosi_pin, miso_pin};
    bool already_owned[3];

    for (size_t i = 0; i < 3; i++) {
        if (!mcujs_rp2_pin_can_claim(pins[i], owner)) {
            return throw_spi_error(MCUJS_ERROR_BUSY, bus, pins[i],
                                   MCUJS_SPI_NO_NATIVE_CODE,
                                   "SPI pin is owned by another peripheral");
        }
        already_owned[i] = mcujs_rp2_pin_owner(pins[i]) == owner;
    }
    for (size_t i = 0; i < 3; i++) {
        if (!mcujs_rp2_pin_claim(pins[i], owner)) {
            for (size_t rollback = 0; rollback < i; rollback++) {
                if (!already_owned[rollback]) {
                    mcujs_rp2_pin_release(pins[rollback], owner);
                }
            }
            return throw_spi_error(MCUJS_ERROR_BUSY, bus, pins[i],
                                   MCUJS_SPI_NO_NATIVE_CODE,
                                   "SPI pin claim failed");
        }
    }

    if (active->initialized) spi_deinit(spi);
    release_old_spi_route(active, &candidate, owner);

    uint actual_baudrate = spi_init(spi, (uint)baudrate);
    if (actual_baudrate == 0 || actual_baudrate != (uint)baudrate) {
        /* spi_init() may have configured native state even when its reported
         * rate is unusable. Tear that attempt down before releasing routes so
         * another binding cannot inherit a live SPI peripheral. */
        spi_deinit(spi);
        for (size_t i = 0; i < 3; i++) {
            reset_and_release_spi_pin(pins[i], owner);
        }
        return throw_spi_error(
            actual_baudrate == 0 ? MCUJS_ERROR_IO : MCUJS_ERROR_NOT_SUPPORTED,
            bus, -1, (int)actual_baudrate,
            actual_baudrate == 0 ? "SPI initialization failed"
                                 : "SPI baudrate cannot be represented exactly");
    }

    gpio_set_function((uint)sck_pin, GPIO_FUNC_SPI);
    gpio_set_function((uint)mosi_pin, GPIO_FUNC_SPI);
    gpio_set_function((uint)miso_pin, GPIO_FUNC_SPI);
    spi_set_format(spi, 8,
                   (options.mode & 2) != 0 ? SPI_CPOL_1 : SPI_CPOL_0,
                   (options.mode & 1) != 0 ? SPI_CPHA_1 : SPI_CPHA_0,
                   SPI_MSB_FIRST);

    *active = candidate;
    active->initialized = true;
    return jerry_undefined();
}

static jerry_value_t spi_transfer_handler(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc) {
    (void)call_info_p;

    int bus;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &bus);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "SPI bus must be a finite number",
                              "SPI bus must be an integer");
    }
    spi_inst_t *spi = get_spi_instance(bus);
    if (spi == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid SPI bus");
    }
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE,
                              "SPI.transfer requires bus and data");
    }

    uint8_t tx_buffer[MCUJS_RUNTIME_SPI_MAX_TRANSFER_BYTES];
    uint8_t rx_buffer[MCUJS_RUNTIME_SPI_MAX_TRANSFER_BYTES];
    size_t len = 0;
    bool array_input = jerry_value_is_array(args[1]);
    if (array_input) {
        jerry_value_t exception;
        status = mcujs_get_byte_array(args, argc, 1, tx_buffer,
                                      sizeof(tx_buffer), 1,
                                      MCUJS_RUNTIME_SPI_MAX_TRANSFER_BYTES,
                                      &len, &exception);
        if (status == MCUJS_ARG_EXCEPTION) return exception;
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status,
                                   "SPI data must be an array of numbers",
                                   "SPI data exceeds maxTransferBytes");
        }
    } else {
        status = mcujs_value_to_byte(args[1], &tx_buffer[0]);
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status,
                                   "SPI data must be a number or byte array",
                                   "SPI data byte must be an integer 0..255");
        }
        len = 1;
    }

    if (!s_spi_buses[bus].initialized) {
        return throw_spi_error(MCUJS_ERROR_BUSY, bus, -1,
                               MCUJS_SPI_NO_NATIVE_CODE,
                               "SPI bus is not initialized");
    }

    int result = spi_write_read_blocking(spi, tx_buffer, rx_buffer, len);
    if (result != (int)len) {
        return throw_spi_error(MCUJS_ERROR_IO, bus, -1, result,
                               "SPI transfer failed");
    }
    if (!array_input) return jerry_number((double)rx_buffer[0]);

    jerry_value_t array = jerry_array((uint32_t)len);
    if (jerry_value_is_exception(array)) return array;
    for (size_t i = 0; i < len; i++) {
        jerry_value_t byte_val = jerry_number((double)rx_buffer[i]);
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

static jerry_value_t spi_write_buffer_dma_handler(
    const jerry_call_info_t *call_info_p, const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;

    int bus;
    int handle_value;
    int byte_length_value;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &bus);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "SPI DMA bus must be a finite number",
                              "SPI DMA bus must be an integer");
    }
    status = mcujs_get_integer(args, argc, 1, &handle_value);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status,
                              "SPI DMA buffer handle must be a finite number",
                              "SPI DMA buffer handle must be an integer");
    }
    status = mcujs_get_integer(args, argc, 2, &byte_length_value);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status,
                              "SPI DMA byte length must be a finite number",
                              "SPI DMA byte length must be an integer");
    }
    if (handle_value < 0) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Invalid graphics buffer handle");
    }
    if (byte_length_value < 0) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "SPI DMA byte length must be non-negative");
    }

    spi_inst_t *spi = get_spi_instance(bus);
    if (spi == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid SPI bus");
    }
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)handle_value;
    uint16_t *data = graphics_get_buffer_data(handle);
    if (data == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Invalid graphics buffer handle");
    }

    uint32_t buffer_bytes = graphics_get_buffer_byte_length(handle);
    uint32_t byte_length = (uint32_t)byte_length_value;
    if (byte_length > buffer_bytes) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "SPI DMA byte length exceeds the graphics buffer");
    }
    if (!s_spi_buses[bus].initialized) {
        return throw_spi_error(MCUJS_ERROR_BUSY, bus, -1,
                               MCUJS_SPI_NO_NATIVE_CODE,
                               "SPI bus is not initialized");
    }
    if (byte_length == 0) return jerry_undefined();

    if (s_dma_tx_channel < 0) {
        s_dma_tx_channel = dma_claim_unused_channel(false);
        if (s_dma_tx_channel < 0) return throw_dma_exhausted(bus);
    }

    dma_channel_config config =
        dma_channel_get_default_config(s_dma_tx_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_dreq(&config, spi_get_dreq(spi, true));
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);

    dma_channel_configure(s_dma_tx_channel, &config, &spi_get_hw(spi)->dr,
                          data, byte_length, true);
    dma_channel_wait_for_finish_blocking(s_dma_tx_channel);
    while (spi_is_busy(spi)) tight_loop_contents();
    return jerry_undefined();
}

jerry_value_t js_create_spi_module(void) {
    jerry_value_t spi = jerry_object();
    js_set_function(spi, "init", spi_init_handler);
    js_set_function(spi, "transfer", spi_transfer_handler);
    js_set_function(spi, "writeBufferDMA", spi_write_buffer_dma_handler);
    return spi;
}

void js_bind_spi(void) {
    jerry_value_t spi = js_create_spi_module();
    js_register_global("SPI", spi);
    jerry_value_free(spi);
}
