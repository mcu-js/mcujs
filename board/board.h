/*
 * mcujs - Board Abstraction Interface
 * 
 * Common interface for all supported boards
 */

#ifndef MCUJS_BOARD_H
#define MCUJS_BOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 
 * Note: board_config.h is included via compiler include path
 * from the board-specific directory (e.g., board/pico/board_config.h)
 */

/*
 * Board information structure
 */
typedef struct {
    const char *name;           /* Board name (e.g., "pico", "pico2") */
    const char *chip;           /* Chip name (e.g., "RP2040", "RP2350") */
    uint32_t flash_size;        /* Total flash size in bytes */
    uint32_t ram_size;          /* Total RAM size in bytes */
    uint32_t cpu_freq;          /* CPU frequency in Hz */
    uint8_t led_pin;            /* Onboard LED pin (255 if none) */
} board_info_t;

/*
 * Initialize board hardware
 * - Sets up clocks
 * - Initializes GPIO
 * - Configures any board-specific peripherals
 * Note: Named mcujs_board_init to avoid conflict with TinyUSB's board_init
 */
void mcujs_board_init(void);

/*
 * Get board information
 */
const board_info_t *board_get_info(void);

/*
 * Get filesystem boundaries from linker symbols
 */
uint32_t board_get_fs_start(void);
uint32_t board_get_fs_end(void);
uint32_t board_get_fs_size(void);

/*
 * LED control (for boards with onboard LED)
 */
void board_led_init(void);
void board_led_set(bool on);
void board_led_toggle(void);

/*
 * Delay functions
 */
void board_delay_ms(uint32_t ms);
void board_delay_us(uint32_t us);

/*
 * Get system tick (milliseconds since boot)
 */
uint32_t board_get_ticks_ms(void);

/*
 * Get unique board ID (from flash chip)
 */
void board_get_unique_id(uint8_t *id, size_t len);

/*
 * Enter UF2 bootloader mode (if supported)
 * Returns true if supported and triggered.
 */
bool board_enter_uf2(void);

#endif /* MCUJS_BOARD_H */
