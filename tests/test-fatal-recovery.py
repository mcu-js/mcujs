#!/usr/bin/env python3
"""Bounded host test: real port C, fake SDK reset. Never touches a device."""
from pathlib import Path
import os, subprocess, tempfile
ROOT = Path(__file__).resolve().parents[1]
JERRY = Path(os.environ.get('JERRYSCRIPT_PATH', '/opt/jerryscript'))
HEADERS = {
    'test_reset.h': '''#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { volatile uint32_t scratch[8]; } watchdog_hw_t;
extern watchdog_hw_t test_watchdog;
typedef struct { struct { volatile uint32_t ctrl; } io[6]; } test_qspi_t;
typedef struct { uint32_t gpio_hi_in; } test_sio_t;
extern test_qspi_t test_qspi;
extern test_sio_t test_sio;
#define ioqspi_hw (&test_qspi)
#define sio_hw (&test_sio)
#define GPIO_OVERRIDE_LOW 2u
#define IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB 12u
#define IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS (3u << 12u)
#define SIO_GPIO_HI_IN_QSPI_CSN_BITS 0x08000000u
uint32_t save_and_disable_interrupts(void);
void restore_interrupts(uint32_t);
void hw_write_masked(volatile uint32_t *, uint32_t, uint32_t);
#define watchdog_hw (&test_watchdog)
void watchdog_reboot(uint32_t, uint32_t, uint32_t);
bool watchdog_caused_reboot(void);
''',
    'hardware/watchdog.h': '#pragma once\n#include "test_reset.h"\n',
    'hardware/structs/watchdog.h': '#pragma once\n#include "test_reset.h"\n',
    'hardware/timer.h': '#pragma once\n',
    'esp_attr.h': '#pragma once\n#define RTC_NOINIT_ATTR __attribute__((section("test_rtc")))\n',
    'esp_system.h': '#pragma once\n#define ESP_RST_POWERON 0\n#define ESP_RST_SW 4\ntypedef int esp_reset_reason_t;\nvoid esp_restart(void);\nesp_reset_reason_t esp_reset_reason(void);\n',
    'esp_log.h': '#pragma once\n#include <stdio.h>\n#define ESP_LOGE(tag, ...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)\n#define ESP_LOGI ESP_LOGE\n',
    'esp_timer.h': '#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n',
    'freertos/FreeRTOS.h': '#pragma once\n#include <stdint.h>\ntypedef uint32_t TickType_t;\n#define pdMS_TO_TICKS(n) (n)\n',
    'freertos/task.h': '#pragma once\n#include "FreeRTOS.h"\nvoid vTaskDelay(TickType_t);\n',
    'pico/stdlib.h': '#pragma once\n#include_next "pico/stdlib.h"\n#define __no_inline_not_in_flash_func(n) n\n',
    'usb_cdc.h': '#pragma once\n#include <stdbool.h>\nbool usb_cdc_connected(void);\nvoid usb_cdc_puts(const char *);\n',
    'esp_err.h': '#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\nconst char *esp_err_to_name(esp_err_t);\n',
    'nvs.h': '#pragma once\n#include <stdint.h>\n#include "esp_err.h"\ntypedef int nvs_handle_t;\n#define NVS_READWRITE 1\n#define ESP_ERR_NVS_NOT_FOUND 0x1102\nesp_err_t nvs_open(const char *, int, nvs_handle_t *);\nesp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *);\nesp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t);\nesp_err_t nvs_commit(nvs_handle_t);\n',
    'nvs_flash.h': '#pragma once\n#include "esp_err.h"\nesp_err_t nvs_flash_init(void);\n',
}
HEADERS['esp_system.h'] += '#define ESP_RST_TASK_WDT 1\n#define ESP_RST_INT_WDT 2\n#define ESP_RST_WDT 3\n'
for name in ('hardware/gpio.h', 'hardware/sync.h', 'hardware/structs/ioqspi.h', 'hardware/structs/sio.h', 'hardware/regs/sio.h'):
    HEADERS[name] = '#pragma once\n#include "test_reset.h"\n'
JERRY_BUILD = Path(os.environ['JERRYSCRIPT_BUILD'])
with tempfile.TemporaryDirectory(prefix='mcujs-fatal-') as temp:
    temp = Path(temp)
    for name, text in HEADERS.items():
        path = temp/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    failed = False
    for backend in ('rp2040', 'rp2350', 'esp32'):
        rp = backend.startswith('rp')
        folder = 'platform/rp2' if rp else 'platform/esp32/main'
        binary = temp/backend
        cmd = [os.environ.get('CC', 'cc'), '-std=gnu17', '-Wall', '-Wextra', '-Werror',
               '-ffunction-sections', '-fdata-sections',
               '-DMCUJS_PLATFORM_RP2=1' if rp else '-DMCUJS_PLATFORM_ESP32=1',
               '-DPICO_RP2350='+str(int(backend == 'rp2350')),
               '-I'+str(temp), '-I'+str(ROOT/'host'), '-I'+str(JERRY/'jerry-core/include'),
               '-I'+str(ROOT/folder), '-I'+str(ROOT/'src'), '-I'+str(ROOT/'src/filesystem'),
               '-I'+str(ROOT/'tests/native_stubs/rp2'),
               str(ROOT/'tests/fatal_recovery_test.c'), str(ROOT/folder/'jerry_port.c'),
               str(ROOT/folder/'boot.c'),
               '-Wl,--gc-sections', '-Wl,--wrap=abort',
               str(JERRY_BUILD/'lib/libjerry-core.a'), str(JERRY_BUILD/'lib/libjerry-port.a'),
               '-lm', '-o', str(binary)]
        subprocess.run(cmd, check=True)
        for trigger in ('fatal-early', 'fatal-late'):
            state = temp/(backend+'-'+trigger+'.state')
            for mode, expected in ((trigger,77),('recover',0),('normal',0)):
                result = subprocess.run([str(binary),mode,str(state)],text=True,capture_output=True,timeout=3)
                ok = result.returncode == expected
                print(f'{backend}/{trigger}/{mode}: {"PASS" if ok else "FAIL"}; exit={result.returncode}; {(result.stdout+result.stderr).strip()}',flush=True)
                failed |= not ok
                if not ok:
                    break
        state=temp/(backend+'-cold.state')
        for mode,expected in (('fatal-late',77),('cold',0)):
            result=subprocess.run([str(binary),mode,str(state)],text=True,capture_output=True,timeout=3)
            ok=result.returncode==expected
            print(f'{backend}/cold/{mode}: {"PASS" if ok else "FAIL"}; exit={result.returncode}; {(result.stdout+result.stderr).strip()}',flush=True)
            failed |= not ok
    raise SystemExit(1 if failed else 0)
