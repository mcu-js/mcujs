/*
 * mcujs - Waveshare RP2040-PiZero Board Configuration
 * 
 * Board: Waveshare RP2040-PiZero
 * Chip: RP2040
 * Flash: 16MB
 * RAM: 264KB
 * 
 * Features:
 * - Raspberry Pi Zero form factor with 40-pin GPIO header
 * - DVI output (directly drives HDMI-compatible displays)
 * - MicroSD card slot
 * - PIO-USB port (secondary USB for host/device)
 * - Lithium battery charging circuit
 * 
 * Reference: https://www.waveshare.com/wiki/RP2040-PiZero
 */

#ifndef MCUJS_BOARD_CONFIG_H
#define MCUJS_BOARD_CONFIG_H

#include "../flash_config.h"

/* Board identification */
#define MCUJS_BOARD_NAME        "waveshare_rp2040_pizero"
#define MCUJS_BOARD_CHIP        "RP2040"
#define MCUJS_BOARD_CHIP_ID     0x2040
#define MCUJS_HAS_UF2           1

/* Memory configuration */
#define MCUJS_FLASH_SIZE        FLASH_SIZE_16MB
#define MCUJS_RAM_SIZE          (264 * 1024)

/* Clock configuration - DVI requires 252 MHz for TMDS encoding */
#define MCUJS_CPU_FREQ_HZ       (252 * 1000 * 1000)  /* 252 MHz for DVI */

/* Pin configuration */
#define MCUJS_LED_PIN           255     /* No onboard LED */
#define MCUJS_NEOPIXEL_PIN      255     /* No onboard NeoPixel */
#define MCUJS_HAS_NEOPIXEL      0
#define MCUJS_NEOPIXEL_LENGTH   0
#define MCUJS_NEOPIXEL_ORDER_GRB 1

/*
 * Default I2C pins (Pi Zero compatible header)
 * I2C0: GPIO 0/1 (Pin 27/28 on 40-pin header - ID EEPROM)
 * I2C1: GPIO 2/3 (Pin 3/5 on 40-pin header - standard Pi I2C)
 */
#define MCUJS_I2C0_SDA_PIN      0
#define MCUJS_I2C0_SCL_PIN      1
#define MCUJS_I2C1_SDA_PIN      2
#define MCUJS_I2C1_SCL_PIN      3

/*
 * Default SPI pins (Pi Zero compatible header)
 * SPI0: Standard Pi SPI0 pins
 * SPI1: Directly used by SD card (directly controlled by board)
 */
#define MCUJS_SPI0_SCK_PIN      18
#define MCUJS_SPI0_MOSI_PIN     19
#define MCUJS_SPI0_MISO_PIN     16
#define MCUJS_SPI0_CS_PIN       17

#define MCUJS_SPI1_SCK_PIN      10
#define MCUJS_SPI1_MOSI_PIN     11
#define MCUJS_SPI1_MISO_PIN     12
#define MCUJS_SPI1_CS_PIN       13

/*
 * Default UART pins (Pi Zero compatible header)
 * UART0: GPIO 0/1 (shared with I2C0)
 * UART1: GPIO 4/5
 */
#define MCUJS_UART0_TX_PIN      0
#define MCUJS_UART0_RX_PIN      1
#define MCUJS_UART1_TX_PIN      4
#define MCUJS_UART1_RX_PIN      5

/* ADC pins (GPIO 26-29 on RP2040) */
#define MCUJS_ADC0_PIN          26
#define MCUJS_ADC1_PIN          27
#define MCUJS_ADC2_PIN          28
#define MCUJS_ADC_VSYS_PIN      29  /* VSYS/3 measurement */

/* PWM configuration */
#define MCUJS_PWM_WRAP_DEFAULT  65535

/* USB configuration */
#define MCUJS_USB_VID           0x2E8A  /* Raspberry Pi VID */
#define MCUJS_USB_PID           0x0010  /* Custom PID for mcujs waveshare rp2040 pizero */

/*
 * ============================================================================
 * DVI Output Configuration (directly drives HDMI display)
 * 
 * The RP2040-PiZero uses bit-banged DVI via PIO state machines.
 * Pin assignments match the PicoDVI waveshare_rp2040_pizero configuration.
 * 
 * PicoDVI uses differential pair emulation - each TMDS lane uses 2 GPIO pins:
 * - Positive pin (P) drives the signal
 * - Negative pin (N) = P + 1, drives inverted signal
 * ============================================================================
 */

/* DVI TMDS Data Lanes (PicoDVI style - positive pin, negative is +1) 
 * These match the waveshare_rp2040_pizero config in PicoDVI:
 * pins_tmds = {26, 24, 22} for D0/Blue, D1/Green, D2/Red
 */
#define MCUJS_DVI_D0_PIN        26      /* TMDS Data 0 (Blue + HSync/VSync), uses GPIO 26/27 */
#define MCUJS_DVI_D1_PIN        24      /* TMDS Data 1 (Green), uses GPIO 24/25 */
#define MCUJS_DVI_D2_PIN        22      /* TMDS Data 2 (Red), uses GPIO 22/23 */

/* DVI TMDS Clock Lane 
 * pins_clk = 28, uses GPIO 28/29 
 */
#define MCUJS_DVI_CLK_PIN       28      /* TMDS Clock, uses GPIO 28/29 */

/* PIO instance for DVI (PicoDVI uses pio0 by default) */
#define MCUJS_DVI_PIO           pio0

/* DVI feature flag */
#define MCUJS_HAS_DVI           1

/*
 * ============================================================================
 * MicroSD Card Configuration
 * 
 * NOTE: MicroSD support is planned for Phase 2.
 * The SD card uses SPI mode on dedicated pins.
 * ============================================================================
 */

#define MCUJS_SD_SPI_BUS        1       /* SD card on SPI1 */
#define MCUJS_SD_SCK_PIN        10
#define MCUJS_SD_MOSI_PIN       11
#define MCUJS_SD_MISO_PIN       12
#define MCUJS_SD_CS_PIN         9       /* SD card chip select */

/* SD feature flag (disabled until Phase 2 implementation) */
#define MCUJS_HAS_SD            0

/*
 * ============================================================================
 * PIO-USB Configuration (secondary USB port)
 * 
 * NOTE: PIO-USB support is planned for Phase 3.
 * This allows the board to act as USB host or device on the secondary port.
 * ============================================================================
 */

#define MCUJS_PIO_USB_DP_PIN    24      /* USB D+ */
#define MCUJS_PIO_USB_DM_PIN    25      /* USB D- */

/* PIO-USB feature flag (disabled until Phase 3 implementation) */
#define MCUJS_HAS_PIO_USB       0

#endif /* MCUJS_BOARD_CONFIG_H */
