#ifndef MCUJS_TEST_ESP_I2C_H
#define MCUJS_TEST_ESP_I2C_H

#include "driver/gpio.h"
#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

typedef int i2c_port_t;

#define I2C_NUM_0 0
#define I2C_NUM_1 1
#define I2C_MODE_MASTER 1
#define GPIO_PULLUP_ENABLE 1

typedef struct {
    int mode;
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
    int sda_pullup_en;
    int scl_pullup_en;
    struct {
        uint32_t clk_speed;
    } master;
    uint32_t clk_flags;
} i2c_config_t;

esp_err_t i2c_param_config(i2c_port_t port, const i2c_config_t *config);
esp_err_t i2c_driver_install(i2c_port_t port, int mode, size_t rx_buffer_length,
                             size_t tx_buffer_length, int interrupt_flags);
esp_err_t i2c_driver_delete(i2c_port_t port);
esp_err_t i2c_master_write_to_device(i2c_port_t port, uint8_t address,
                                     const uint8_t *data, size_t length,
                                     uint32_t timeout_ticks);
esp_err_t i2c_master_read_from_device(i2c_port_t port, uint8_t address,
                                      uint8_t *data, size_t length,
                                      uint32_t timeout_ticks);

#endif
