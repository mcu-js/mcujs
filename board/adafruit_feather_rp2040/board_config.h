/*
 * mcujs - Adafruit Feather RP2040 Board Configuration
 * 
 * Board: Adafruit Feather RP2040
 * Chip: RP2040
 * Flash: 8MB
 * RAM: 264KB
 * 
 * Features:
 * - RGB NeoPixel on GPIO 16
 * - Red LED on GPIO 13
 * - STEMMA QT I2C connector (I2C1: SDA=GP2, SCL=GP3)
 * - USB-C connector
 * - LiPoly battery charger
 * - BOOTSEL button connected to GPIO 4 (Rev D+)
 */

#ifndef MCUJS_BOARD_CONFIG_H
#define MCUJS_BOARD_CONFIG_H

#include "../flash_config.h"

/* Board identification */
#define MCUJS_BOARD_NAME        "adafruit_feather_rp2040"
#define MCUJS_BOARD_CHIP        "RP2040"
#define MCUJS_BOARD_CHIP_ID     0x2040
#define MCUJS_HAS_UF2           1

/* Memory configuration */
#define MCUJS_FLASH_SIZE        FLASH_SIZE_8MB
#define MCUJS_RAM_SIZE          (264 * 1024)

/* Clock configuration */
#define MCUJS_CPU_FREQ_HZ       (125 * 1000 * 1000)  /* 125 MHz */

/* Pin configuration */
#define MCUJS_LED_PIN           13      /* Red LED (D13) */
#define MCUJS_NEOPIXEL_PIN      16      /* RGB NeoPixel */
#define MCUJS_HAS_NEOPIXEL      1
#define MCUJS_NEOPIXEL_LENGTH   1
#define MCUJS_NEOPIXEL_ORDER_GRB 1

/* BOOTSEL button (Rev D+ boards have this connected to GPIO 4) */
#define MCUJS_BOOTSEL_PIN       4

/* Default I2C pins
 * I2C1 is the main I2C bus (STEMMA QT connector)
 * I2C0 is available on D24/D25
 */
#define MCUJS_I2C0_SDA_PIN      24      /* D24 */
#define MCUJS_I2C0_SCL_PIN      25      /* D25 */
#define MCUJS_I2C1_SDA_PIN      2       /* SDA (STEMMA QT) */
#define MCUJS_I2C1_SCL_PIN      3       /* SCL (STEMMA QT) */

/* Default SPI pins
 * SPI0 is the main SPI bus (exposed on header)
 */
#define MCUJS_SPI0_SCK_PIN      18      /* SCK */
#define MCUJS_SPI0_MOSI_PIN     19      /* MO (MOSI) */
#define MCUJS_SPI0_MISO_PIN     20      /* MI (MISO) */
#define MCUJS_SPI0_CS_PIN       1       /* RX (can be used as CS) */

#define MCUJS_SPI1_SCK_PIN      10      /* D10 */
#define MCUJS_SPI1_MOSI_PIN     11      /* D11 */
#define MCUJS_SPI1_MISO_PIN     12      /* D12 */
#define MCUJS_SPI1_CS_PIN       13      /* D13 (shared with LED) */

/* Default UART pins */
#define MCUJS_UART0_TX_PIN      0       /* TX */
#define MCUJS_UART0_RX_PIN      1       /* RX */
#define MCUJS_UART1_TX_PIN      8       /* D6 */
#define MCUJS_UART1_RX_PIN      9       /* D9 */

/* ADC pins (GPIO 26-29 on RP2040)
 * A0 = GP26, A1 = GP27, A2 = GP28, A3 = GP29
 */
#define MCUJS_ADC0_PIN          26      /* A0 */
#define MCUJS_ADC1_PIN          27      /* A1 */
#define MCUJS_ADC2_PIN          28      /* A2 */
#define MCUJS_ADC_VSYS_PIN      29      /* A3 (VSYS/3 measurement) */

/* PWM configuration */
#define MCUJS_PWM_WRAP_DEFAULT  65535

/* USB configuration */
#define MCUJS_USB_VID           0x2E8A  /* Raspberry Pi VID */
#define MCUJS_USB_PID           0x0011  /* Custom PID for mcujs adafruit feather rp2040 */

#endif /* MCUJS_BOARD_CONFIG_H */
