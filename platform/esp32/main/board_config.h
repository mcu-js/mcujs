#ifndef MCUJS_ESP32S3_BOARD_CONFIG_H
#define MCUJS_ESP32S3_BOARD_CONFIG_H

#define MCUJS_BOARD_NAME "seeed_xiao_esp32s3"
#define MCUJS_BOARD_CHIP "ESP32-S3"
#define MCUJS_FLASH_SIZE (8u * 1024u * 1024u)
#define MCUJS_RAM_SIZE (512u * 1024u)
#define MCUJS_CPU_FREQ_HZ (240u * 1000u * 1000u)
#define MCUJS_LED_PIN 21
#define MCUJS_BUTTON_PIN 0 /* BOOT: reserved strapping pin, managed input only */
#define MCUJS_UART_TX_PIN 43 /* XIAO D6 */
#define MCUJS_UART_RX_PIN 44 /* XIAO D7 */
#define MCUJS_UART_BAUD 115200
#define MCUJS_HAS_DVI 0
#define MCUJS_HAS_NEOPIXEL 0

#endif /* MCUJS_ESP32S3_BOARD_CONFIG_H */
