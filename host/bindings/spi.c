/*
 * mcujs - SPI Bindings
 * 
 * Implements: SPI.init(), SPI.transfer(), SPI.writeBufferDMA()
 */

#include "bindings.h"
#include "graphics.h"
#include "jerryscript.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);
extern double js_get_number_arg(const jerry_value_t args[], jerry_length_t argc,
                                jerry_length_t index, double default_value);

/* Maximum SPI transfer size for non-DMA transfers */
#define MAX_SPI_TRANSFER 256

/* DMA channel for SPI TX (-1 = not claimed) */
static int s_dma_tx_channel = -1;

/* SPI instances */
static spi_inst_t *get_spi_instance(int bus) {
    switch (bus) {
        case 0: return spi0;
        case 1: return spi1;
        default: return NULL;
    }
}

/*
 * SPI.init(bus, sck, mosi, miso, baudrate)
 * Initialize SPI bus
 */
static jerry_value_t spi_init_handler(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 5) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "SPI.init requires bus, sck, mosi, miso, baudrate");
    }
    
    int bus = (int)js_get_number_arg(args, argc, 0, 0);
    uint sck_pin = (uint)js_get_number_arg(args, argc, 1, 0);
    uint mosi_pin = (uint)js_get_number_arg(args, argc, 2, 0);
    uint miso_pin = (uint)js_get_number_arg(args, argc, 3, 0);
    uint32_t baudrate = (uint32_t)js_get_number_arg(args, argc, 4, 1000000);
    
    spi_inst_t *spi = get_spi_instance(bus);
    if (spi == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid SPI bus (0 or 1)");
    }
    
    /* Initialize SPI */
    spi_init(spi, baudrate);
    
    /* Set up pins */
    gpio_set_function(sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
    gpio_set_function(miso_pin, GPIO_FUNC_SPI);
    
    /* Default format: 8 bits, CPOL=0, CPHA=0, MSB first */
    spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    return jerry_undefined();
}

/*
 * SPI.transfer(bus, data)
 * Transfer data over SPI (full duplex)
 * data can be an array of bytes or a single byte
 * Returns received data
 */
static jerry_value_t spi_transfer_handler(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "SPI.transfer requires bus and data");
    }
    
    int bus = (int)js_get_number_arg(args, argc, 0, 0);
    
    spi_inst_t *spi = get_spi_instance(bus);
    if (spi == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid SPI bus");
    }
    
    uint8_t tx_buffer[MAX_SPI_TRANSFER];
    uint8_t rx_buffer[MAX_SPI_TRANSFER];
    size_t len = 0;
    
    /* Check if data is an array or single value */
    if (jerry_value_is_array(args[1])) {
        uint32_t array_len = jerry_array_length(args[1]);
        if (array_len > MAX_SPI_TRANSFER) {
            array_len = MAX_SPI_TRANSFER;
        }
        
        for (uint32_t i = 0; i < array_len; i++) {
            jerry_value_t elem = jerry_object_get_index(args[1], i);
            tx_buffer[len++] = (uint8_t)jerry_value_as_number(elem);
            jerry_value_free(elem);
        }
    } else {
        tx_buffer[0] = (uint8_t)js_get_number_arg(args, argc, 1, 0);
        len = 1;
    }
    
    /* Transfer data */
    int result = spi_write_read_blocking(spi, tx_buffer, rx_buffer, len);
    
    if (result != (int)len) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "SPI transfer failed");
    }
    
    /* Return single byte or array depending on input */
    if (len == 1 && !jerry_value_is_array(args[1])) {
        return jerry_number((double)rx_buffer[0]);
    }
    
    /* Create result array */
    jerry_value_t array = jerry_array((uint32_t)len);
    
    for (size_t i = 0; i < len; i++) {
        jerry_value_t byte_val = jerry_number((double)rx_buffer[i]);
        jerry_object_set_index(array, (uint32_t)i, byte_val);
        jerry_value_free(byte_val);
    }
    
    return array;
}

/*
 * SPI.writeBufferDMA(bus, bufferHandle, byteLength)
 * Write graphics buffer data via DMA (blocking until complete)
 * Uses the native graphics buffer directly for zero-copy transfer
 */
static jerry_value_t spi_write_buffer_dma_handler(const jerry_call_info_t *call_info_p,
                                                   const jerry_value_t args[],
                                                   const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 3) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, 
            "SPI.writeBufferDMA requires bus, bufferHandle, byteLength");
    }
    
    int bus = (int)js_get_number_arg(args, argc, 0, 0);
    graphics_buffer_handle_t handle = (graphics_buffer_handle_t)jerry_value_as_number(args[1]);
    uint32_t byte_length = (uint32_t)js_get_number_arg(args, argc, 2, 0);
    
    spi_inst_t *spi = get_spi_instance(bus);
    if (spi == NULL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid SPI bus");
    }
    
    /* Get buffer data pointer */
    uint16_t *data = graphics_get_buffer_data(handle);
    if (data == NULL) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Invalid graphics buffer handle");
    }
    
    /* Validate byte length */
    uint32_t buffer_bytes = graphics_get_buffer_byte_length(handle);
    if (byte_length > buffer_bytes) {
        byte_length = buffer_bytes;
    }
    
    if (byte_length == 0) {
        return jerry_undefined();
    }
    
    /* Claim DMA channel if not already claimed */
    if (s_dma_tx_channel < 0) {
        s_dma_tx_channel = dma_claim_unused_channel(true);
    }
    
    /* Configure DMA for SPI TX */
    dma_channel_config c = dma_channel_get_default_config(s_dma_tx_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);  /* 8-bit transfers */
    channel_config_set_dreq(&c, spi_get_dreq(spi, true));   /* SPI TX DREQ */
    channel_config_set_read_increment(&c, true);            /* Increment read addr */
    channel_config_set_write_increment(&c, false);          /* Fixed write addr */
    
    /* Start DMA transfer */
    dma_channel_configure(
        s_dma_tx_channel,
        &c,
        &spi_get_hw(spi)->dr,   /* Write to SPI data register */
        data,                    /* Read from buffer */
        byte_length,             /* Transfer count */
        true                     /* Start immediately */
    );
    
    /* Wait for DMA to complete (blocking) */
    dma_channel_wait_for_finish_blocking(s_dma_tx_channel);
    
    /* Wait for SPI to finish transmitting */
    while (spi_is_busy(spi)) {
        tight_loop_contents();
    }
    
    return jerry_undefined();
}

/*
 * Create SPI module object
 */
jerry_value_t js_create_spi_module(void) {
    jerry_value_t spi = jerry_object();

    js_set_function(spi, "init", spi_init_handler);
    js_set_function(spi, "transfer", spi_transfer_handler);
    js_set_function(spi, "writeBufferDMA", spi_write_buffer_dma_handler);

    return spi;
}

/*
 * Register SPI bindings
 */
void js_bind_spi(void) {
    jerry_value_t spi = js_create_spi_module();
    js_register_global("SPI", spi);
    jerry_value_free(spi);
}
