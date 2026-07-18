/* Shared board-service ABI used by the full MCU.js REPL on ESP32-S3. */

#include "board.h"
#include "board_config.h"

#include "esp_private/system_internal.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const board_info_t s_board_info = {
    .name = MCUJS_BOARD_NAME,
    .chip = MCUJS_BOARD_CHIP,
    .flash_size = MCUJS_FLASH_SIZE,
    .ram_size = MCUJS_RAM_SIZE,
    .cpu_freq = MCUJS_CPU_FREQ_HZ,
    .led_pin = MCUJS_LED_PIN,
    .neopixel_pin = 255,
};

const board_info_t *board_get_info(void) {
    return &s_board_info;
}

void board_delay_ms(uint32_t delay_ms) {
    TickType_t ticks = pdMS_TO_TICKS(delay_ms);
    if (delay_ms > 0 && ticks == 0) {
        ticks = 1;
    }
    vTaskDelay(ticks);
}

bool board_enter_uf2(void) {
    enum { APP_REQUEST_UF2_RESET_HINT = 0x11F2 };
    /* TinyUF2 requires this reference so IDF links the hint implementation. */
    (void)esp_reset_reason();
    esp_reset_reason_set_hint((esp_reset_reason_t)APP_REQUEST_UF2_RESET_HINT);
    vTaskDelay(1);
    esp_restart();
    return true;
}
