/*
 * mcujs - Waveshare RP2040 Touch LCD 1.28 Board Configuration
 * 
 * Board: Waveshare RP2040 Touch LCD 1.28
 * Chip: RP2040
 * Flash: 4MB
 * RAM: 264KB
 */

#ifndef MCUJS_BOARD_CONFIG_H
#define MCUJS_BOARD_CONFIG_H

#include "../flash_config.h"

/* Board identification */
#define MCUJS_BOARD_NAME        "waveshare_rp2040_touch_lcd_1.28"
#define MCUJS_BOARD_CHIP        "RP2040"
#define MCUJS_BOARD_CHIP_ID     0x2040
#define MCUJS_HAS_UF2           1

/* Memory configuration */
#define MCUJS_FLASH_SIZE        FLASH_SIZE_4MB
#define MCUJS_RAM_SIZE          (264 * 1024)

/* Clock configuration */
#define MCUJS_CPU_FREQ_HZ       (125 * 1000 * 1000)  /* 125 MHz */

/* Pin configuration */
#define MCUJS_LED_PIN           255      /* No discrete LED */
#define MCUJS_NEOPIXEL_PIN      255      /* No onboard NeoPixel */
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
#define MCUJS_SPI1_MISO_PIN      2
#define MCUJS_SPI1_CS_PIN       9

/* LCD pins (GC9A01A) */
#define MCUJS_LCD_SPI_BUS       1
#define MCUJS_LCD_SCK_PIN       10
#define MCUJS_LCD_MOSI_PIN      11
#define MCUJS_LCD_MISO_PIN      2
#define MCUJS_LCD_CS_PIN        9
#define MCUJS_LCD_DC_PIN        8
#define MCUJS_LCD_RST_PIN       12
#define MCUJS_LCD_BL_PIN        25

/* Touch controller pins */
#define MCUJS_TOUCH_I2C_BUS     1
#define MCUJS_TOUCH_SDA_PIN     6
#define MCUJS_TOUCH_SCL_PIN     7
#define MCUJS_TOUCH_INT_PIN     21
#define MCUJS_TOUCH_RST_PIN     22

/* IMU sensor pins */
#define MCUJS_IMU_I2C_BUS      1
#define MCUJS_IMU_SDA_PIN      6
#define MCUJS_IMU_SCL_PIN      7
#define MCUJS_IMU_INT1_PIN     23
#define MCUJS_IMU_INT2_PIN     24

/* Default UART pins */
#define MCUJS_UART0_TX_PIN      0
#define MCUJS_UART0_RX_PIN      1
#define MCUJS_UART1_TX_PIN      8
#define MCUJS_UART1_RX_PIN      9

/* ADC pins */
#define MCUJS_ADC0_PIN          26
#define MCUJS_ADC1_PIN          27
#define MCUJS_ADC2_PIN          28
#define MCUJS_ADC_VSYS_PIN      29

/* PWM configuration */
#define MCUJS_PWM_WRAP_DEFAULT  65535

/* USB configuration */
#define MCUJS_USB_VID           0x2E8A  /* Raspberry Pi VID */
#define MCUJS_USB_PID           0x000D  /* Custom PID for mcujs waveshare rp2040 touch lcd 1.28 */

#endif /* MCUJS_BOARD_CONFIG_H */
