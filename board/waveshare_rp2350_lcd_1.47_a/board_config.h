/*
 * mcujs - Waveshare RP2350-LCD-1.47-A Board Configuration
 * 
 * Board: Waveshare RP2350-LCD-1.47-A
 * Chip: RP2350
 * Flash: 16MB
 * RAM: 520KB
 * Display: ST7789V3 (172x320)
 */

#ifndef MCUJS_BOARD_CONFIG_H
#define MCUJS_BOARD_CONFIG_H

#include "../flash_config.h"

/* Board identification */
#define MCUJS_BOARD_NAME        "waveshare_rp2350_lcd_1.47_a"
#define MCUJS_BOARD_CHIP        "RP2350"
#define MCUJS_BOARD_CHIP_ID     0x2350
#define MCUJS_HAS_UF2           1

/* Memory configuration */
#define MCUJS_FLASH_SIZE        FLASH_SIZE_16MB
#define MCUJS_RAM_SIZE          (520 * 1024)

/* Clock configuration */
#define MCUJS_CPU_FREQ_HZ       (150 * 1000 * 1000)  /* 150 MHz */

/* Pin configuration */
#define MCUJS_LED_PIN           255      /* No discrete LED */
#define MCUJS_NEOPIXEL_PIN      22       /* WS2812B RGB LED */
#define MCUJS_HAS_NEOPIXEL      1
#define MCUJS_NEOPIXEL_LENGTH   1
#define MCUJS_NEOPIXEL_ORDER_GRB 1

/* Default I2C pins */
#define MCUJS_I2C0_SDA_PIN      4
#define MCUJS_I2C0_SCL_PIN      5
#define MCUJS_I2C1_SDA_PIN      6
#define MCUJS_I2C1_SCL_PIN      7

/* Default SPI pins */
/* SPI0 - LCD */
#define MCUJS_SPI0_SCK_PIN      18
#define MCUJS_SPI0_MOSI_PIN     19
#define MCUJS_SPI0_MISO_PIN     255  /* Not used for LCD */
#define MCUJS_SPI0_CS_PIN       17

/* SPI1 - SD Card */
#define MCUJS_SPI1_SCK_PIN      10
#define MCUJS_SPI1_MOSI_PIN     11
#define MCUJS_SPI1_MISO_PIN     12
#define MCUJS_SPI1_CS_PIN       15

/* LCD pins (ST7789V3) */
#define MCUJS_LCD_SPI_BUS       0
#define MCUJS_LCD_SCK_PIN       18
#define MCUJS_LCD_MOSI_PIN      19
#define MCUJS_LCD_MISO_PIN      255  /* Not used */
#define MCUJS_LCD_CS_PIN        17
#define MCUJS_LCD_DC_PIN        16
#define MCUJS_LCD_RST_PIN       20
#define MCUJS_LCD_BL_PIN        21

/* SD Card pins */
#define MCUJS_SD_SPI_BUS        1
#define MCUJS_SD_SCK_PIN        10
#define MCUJS_SD_MOSI_PIN       11
#define MCUJS_SD_MISO_PIN       12
#define MCUJS_SD_CS_PIN         15

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
#define MCUJS_USB_PID           0x000E  /* Custom PID for mcujs waveshare rp2350 lcd 1.47 a */

#endif /* MCUJS_BOARD_CONFIG_H */
