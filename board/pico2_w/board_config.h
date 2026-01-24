/*
 * mcujs - Raspberry Pi Pico 2 W Board Configuration
 * 
 * Board: Raspberry Pi Pico 2 W
 * Chip: RP2350
 * Flash: 4MB
 * RAM: 520KB
 * Wireless: CYW43439 (WiFi + Bluetooth)
 * 
 * Note: The onboard LED is connected to the CYW43 wireless chip,
 * not directly to GPIO. GPIO 25 is used for SPI communication
 * with the wireless chip.
 */

#ifndef MCUJS_BOARD_CONFIG_H
#define MCUJS_BOARD_CONFIG_H

#include "../flash_config.h"

/* Board identification */
#define MCUJS_BOARD_NAME        "pico2_w"
#define MCUJS_BOARD_CHIP        "RP2350"
#define MCUJS_BOARD_CHIP_ID     0x2350
#define MCUJS_HAS_UF2           1

/* Wireless chip present */
#define MCUJS_HAS_CYW43         1

/* Memory configuration */
#define MCUJS_FLASH_SIZE        FLASH_SIZE_4MB
#define MCUJS_RAM_SIZE          (520 * 1024)

/* Clock configuration */
#define MCUJS_CPU_FREQ_HZ       (150 * 1000 * 1000)  /* 150 MHz */

/* Pin configuration */
/* LED is on CYW43 chip, not GPIO - set to 255 to indicate no direct GPIO LED */
#define MCUJS_LED_PIN           255
#define MCUJS_CYW43_LED_PIN     0       /* CYW43 WL GPIO 0 is the LED */

#define MCUJS_NEOPIXEL_PIN      255     /* No onboard NeoPixel */
#define MCUJS_HAS_NEOPIXEL      0
#define MCUJS_NEOPIXEL_LENGTH   0
#define MCUJS_NEOPIXEL_ORDER_GRB 1

/* Default I2C pins */
#define MCUJS_I2C0_SDA_PIN      4
#define MCUJS_I2C0_SCL_PIN      5
#define MCUJS_I2C1_SDA_PIN      6
#define MCUJS_I2C1_SCL_PIN      7

/* Default SPI pins */
#define MCUJS_SPI0_SCK_PIN      18
#define MCUJS_SPI0_MOSI_PIN     19
#define MCUJS_SPI0_MISO_PIN     16
#define MCUJS_SPI0_CS_PIN       17

#define MCUJS_SPI1_SCK_PIN      10
#define MCUJS_SPI1_MOSI_PIN     11
#define MCUJS_SPI1_MISO_PIN     12
#define MCUJS_SPI1_CS_PIN       13

/* Default UART pins */
#define MCUJS_UART0_TX_PIN      0
#define MCUJS_UART0_RX_PIN      1
#define MCUJS_UART1_TX_PIN      8
#define MCUJS_UART1_RX_PIN      9

/* ADC pins */
#define MCUJS_ADC0_PIN          26
#define MCUJS_ADC1_PIN          27
#define MCUJS_ADC2_PIN          28
/* Note: ADC3/GPIO29 is used internally for CYW43 SPI on Pico 2 W */
#define MCUJS_ADC_VSYS_PIN      29

/* PWM configuration */
#define MCUJS_PWM_WRAP_DEFAULT  65535

/* USB configuration */
#define MCUJS_USB_VID           0x2E8A  /* Raspberry Pi VID */
#define MCUJS_USB_PID           0x000C  /* Custom PID for mcujs pico2_w */

#endif /* MCUJS_BOARD_CONFIG_H */
