/* Explicit driver fixtures: independent of generated registry/board headers.
 * Wiring provenance and transport limits: tests/rp2-sd-wiring.md. */
#ifndef SD_TEST_BOARD_CONFIG_H
#define SD_TEST_BOARD_CONFIG_H
#ifndef MCUJS_HAS_SD
#define MCUJS_HAS_SD 1
#endif
#if SD_TEST_BOARD == 147
#define MCUJS_SD_SPI_BUS 1
#define MCUJS_SD_SCK_PIN 10
#define MCUJS_SD_MOSI_PIN 11
#define MCUJS_SD_MISO_PIN 12
#define MCUJS_SD_CS_PIN 15
#elif SD_TEST_BOARD == 2040
#define MCUJS_SD_SPI_BUS 0
#define MCUJS_SD_SCK_PIN 18
#define MCUJS_SD_MOSI_PIN 19
#define MCUJS_SD_MISO_PIN 20
#define MCUJS_SD_CS_PIN 21
#elif SD_TEST_BOARD == 28
#define MCUJS_SD_SPI_BUS -1
#define MCUJS_SD_SCK_PIN 19
#define MCUJS_SD_MOSI_PIN 20
#define MCUJS_SD_MISO_PIN 21
#define MCUJS_SD_CS_PIN 24
#else
#error "Select an explicit SD test board"
#endif
#ifndef MCUJS_SD_READONLY
#define MCUJS_SD_READONLY 0
#endif
#define MCUJS_SD_POWER_PIN -1
#define MCUJS_SD_POWER_ACTIVE_HIGH 0
#define MCUJS_SD_CARD_DETECT_PIN -1
#define MCUJS_USB_SD_MSC 1
#endif
