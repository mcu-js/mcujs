#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define ESP_OK 0
#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP_ENABLE 1
#define SPI2_HOST 1
#define SPI_DMA_CH_AUTO 1
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define pdMS_TO_TICKS(x) (x)
typedef void *spi_device_handle_t;
typedef struct {size_t length;const void *tx_buffer;} spi_transaction_t;
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en;} gpio_config_t;
typedef struct {int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num,max_transfer_sz;} spi_bus_config_t;
typedef struct {int clock_speed_hz,mode,spics_io_num,queue_size;} spi_device_interface_config_t;
void vTaskDelay(unsigned);int64_t esp_timer_get_time(void);int gpio_get_level(int);int gpio_set_level(int,int);int gpio_config(const gpio_config_t*);
int esp_task_wdt_status(void*);int esp_task_wdt_reset(void);void usb_cdc_puts(const char*);void usb_cdc_task(void);
int spi_device_polling_transmit(spi_device_handle_t,spi_transaction_t*);int spi_bus_initialize(int,const spi_bus_config_t*,int);int spi_bus_add_device(int,const spi_device_interface_config_t*,spi_device_handle_t*);int spi_bus_remove_device(spi_device_handle_t);int spi_bus_free(int);void *heap_caps_malloc(size_t,int);
