/*
 * mcujs - Board Implementation
 * 
 * Implements the board abstraction interface defined in board/board.h
 */

#include "board.h"
#include "board_config.h"

#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

/* Static board info structure */
static const board_info_t s_board_info = {
    .name = MCUJS_BOARD_NAME,
    .chip = MCUJS_BOARD_CHIP,
    .flash_size = MCUJS_FLASH_SIZE,
    .ram_size = MCUJS_RAM_SIZE,
    .cpu_freq = MCUJS_CPU_FREQ_HZ,
    .led_pin = MCUJS_LED_PIN
};

/* External linker symbols for filesystem region */
extern char _FS_start[];
extern char _FS_end[];

void mcujs_board_init(void) {
    /* Most initialization is done by pico_stdlib */
    /* Any board-specific initialization goes here */
}

const board_info_t *board_get_info(void) {
    return &s_board_info;
}

uint32_t board_get_fs_start(void) {
    return (uint32_t)(uintptr_t)_FS_start;
}

uint32_t board_get_fs_end(void) {
    return (uint32_t)(uintptr_t)_FS_end;
}

uint32_t board_get_fs_size(void) {
    return (uint32_t)(_FS_end - _FS_start);
}

void board_led_init(void) {
    gpio_init(MCUJS_LED_PIN);
    gpio_set_dir(MCUJS_LED_PIN, GPIO_OUT);
    gpio_put(MCUJS_LED_PIN, 0);
}

void board_led_set(bool on) {
    gpio_put(MCUJS_LED_PIN, on ? 1 : 0);
}

void board_led_toggle(void) {
    gpio_put(MCUJS_LED_PIN, !gpio_get(MCUJS_LED_PIN));
}

void board_delay_ms(uint32_t ms) {
    sleep_ms(ms);
}

void board_delay_us(uint32_t us) {
    sleep_us(us);
}

uint32_t board_get_ticks_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

void board_get_unique_id(uint8_t *id, size_t len) {
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    
    size_t copy_len = len < PICO_UNIQUE_BOARD_ID_SIZE_BYTES ? 
                      len : PICO_UNIQUE_BOARD_ID_SIZE_BYTES;
    
    for (size_t i = 0; i < copy_len; i++) {
        id[i] = board_id.id[i];
    }
    
    /* Zero-fill remaining bytes if requested length is larger */
    for (size_t i = copy_len; i < len; i++) {
        id[i] = 0;
    }
}
