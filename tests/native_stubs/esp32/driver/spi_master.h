#ifndef MCUJS_TEST_ESP_SPI_MASTER_H
#define MCUJS_TEST_ESP_SPI_MASTER_H

#include "driver/gpio.h"
#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

typedef int spi_host_device_t;
typedef void *spi_device_handle_t;

#define SPI2_HOST 0
#define SPI3_HOST 1
#define SPI_DMA_DISABLED 0
#define SPICOMMON_BUSFLAG_MASTER (1u << 0)
#define SOC_SPI_MAXIMUM_BUFFER_SIZE 64

typedef struct {
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
    int data4_io_num;
    int data5_io_num;
    int data6_io_num;
    int data7_io_num;
    int max_transfer_sz;
    unsigned flags;
    int intr_flags;
} spi_bus_config_t;

typedef struct {
    int mode;
    int clock_speed_hz;
    int spics_io_num;
    int queue_size;
} spi_device_interface_config_t;

typedef struct {
    size_t length;
    const void *tx_buffer;
    void *rx_buffer;
} spi_transaction_t;

esp_err_t spi_bus_initialize(spi_host_device_t host,
                             const spi_bus_config_t *config,
                             int dma_channel);
esp_err_t spi_bus_free(spi_host_device_t host);
esp_err_t spi_bus_add_device(spi_host_device_t host,
                             const spi_device_interface_config_t *config,
                             spi_device_handle_t *device);
esp_err_t spi_bus_remove_device(spi_device_handle_t device);
esp_err_t spi_device_transmit(spi_device_handle_t device,
                              spi_transaction_t *transaction);
int spi_get_actual_clock(int source_hz, int requested_hz, int duty_cycle);

#endif
