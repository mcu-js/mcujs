#!/usr/bin/env bash
# Read-only external Jerry dependency: no downloads, SDK setup or Jerry rebuild.
# --board-smoke verifies the real board/timer fixture before button integration.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JERRY_ROOT="${JERRYSCRIPT_PATH:-/opt/jerryscript}"
JERRY_BUILD="${JERRYSCRIPT_BUILD:-${JERRY_ROOT}/build}"
CC="${CC:-cc}"
mode="${1:-full}"
if [[ $# -gt 1 || ( "$mode" != full && "$mode" != --board-smoke ) ]]; then
    printf 'Usage: %s [--board-smoke]\n' "$0" >&2
    exit 2
fi
for file in "${JERRY_ROOT}/jerry-core/include/jerryscript.h" \
            "${JERRY_BUILD}/lib/libjerry-core.a" "${JERRY_BUILD}/lib/libjerry-port.a"; do
    if [[ ! -f "$file" ]]; then
        printf 'Missing %s. Set JERRYSCRIPT_PATH and JERRYSCRIPT_BUILD to an external native Jerry 3.0.0 build (64 KiB, memory stats ON); this test never builds Jerry.\n' "$file" >&2
        exit 1
    fi
done
# The heap size and memory-stat support are checked against the linked runtime,
# not inferred from a possibly stale CMakeCache.txt.
if [[ "$mode" == full && ! -f "${ROOT}/lib/button.js" ]]; then
    printf 'Missing production lib/button.js; integrate button factory/registry changes first (or use --board-smoke for the board/timer fixture only).\n' >&2
    exit 1
fi
TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/mcujs-buttons-tests.XXXXXX")"
trap 'rm -rf "${TMP_ROOT}"' EXIT
python3 - "$ROOT" "$TMP_ROOT" "$mode" <<'PY'
from pathlib import Path
import sys
root, out = map(Path, sys.argv[1:3])
for name in ('devices', 'events', 'button'):
    data = (root / 'lib' / (name + '.js')).read_bytes()
    # Production builtin_modules.c passes sizeof(array), without a trailing NUL.
    (out / (name + '_source.h')).write_text(
        'static const jerry_char_t ' + name + '_source[] = {' +
        ','.join(map(str, data)) + '};\n')
# Extend rather than copy the existing fake SDK fixtures. These are declaration
# gaps needed only by board.c and timers.c; implementations live in the harness.
headers = {
    'hardware/timer.h': '#pragma once\n',
    'esp_timer.h': '#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n',
    'esp_heap_caps.h': '#pragma once\n#include <stddef.h>\n#define MALLOC_CAP_8BIT 1\nsize_t heap_caps_get_free_size(unsigned caps);\n',
    'esp_mac.h': '#pragma once\n#include <stdint.h>\n#include "esp_err.h"\nesp_err_t esp_efuse_mac_get_default(uint8_t *mac);\n',
    'esp_system.h': '#pragma once\ntypedef int esp_reset_reason_t;\nvoid esp_restart(void);\nesp_reset_reason_t esp_reset_reason(void);\n',
    'esp_private/system_internal.h': '#pragma once\n#include "esp_system.h"\nvoid esp_reset_reason_set_hint(esp_reset_reason_t hint);\n',
    'freertos/task.h': '#pragma once\n#include <stdint.h>\ntypedef uint32_t TickType_t;\nvoid vTaskDelay(TickType_t ticks);\n',
    'driver/gpio.h': '''#pragma once
#include_next "driver/gpio.h"
#include <stdint.h>
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
typedef struct {
    uint64_t pin_bit_mask;
    int mode, pull_up_en, pull_down_en, intr_type;
} gpio_config_t;
esp_err_t gpio_config(const gpio_config_t *config);
''',
}
for name, text in headers.items():
    path = out / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
PY

compile_target() {
    local target="$1" backend="$2" board_macro="$3"
    shift 3
    local flags=(
        -std=gnu17 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
        -DMCUJS_USE_PRODUCTION_BINDING_HELPERS=1 "-D${board_macro}=1"
        '-DMCUJS_VERSION="0.1.0"'
        "-I${TMP_ROOT}" "-I${ROOT}/host" "-I${ROOT}/host/bindings"
        "-I${ROOT}/tests" "-I${JERRY_ROOT}/jerry-core/include"
        "$@"
    )
    if [[ "$mode" == --board-smoke ]]; then flags+=(-DMCUJS_BUTTONS_BOARD_SMOKE=1); fi
    # Reuse the existing backend fixture verbatim. Rename only its RP clock
    # definitions so this harness can advance SDK time without fake JS timers.
    "$CC" "${flags[@]}" \
        -Dget_absolute_time=mcujs_unused_fixed_time \
        -Dto_ms_since_boot=mcujs_unused_fixed_ms \
        -Dusb_cdc_reset_usb=mcujs_unused_usb_reset \
        -Dboard_enter_uf2=mcujs_unused_enter_uf2 \
        -c "${ROOT}/tests/runtime_validation_backend_stubs.c" \
        -o "${TMP_ROOT}/${target}-sdk.o"
    local extra=()
    if [[ "$target" == pico ]]; then
        extra+=("${ROOT}/platform/rp2/bindings/onboard_led.c"
                "${ROOT}/platform/rp2/bindings/gpio.c"
                "${ROOT}/platform/rp2/bindings/pin_policy.c")
    else
        extra+=(-Wl,--wrap=gpio_get_level)
    fi
    "$CC" "${flags[@]}" \
        "${ROOT}/tests/buttons_jerry_test.c" \
        "${TMP_ROOT}/${target}-sdk.o" \
        "${ROOT}/host/runtime_registry.c" \
        "${ROOT}/host/bindings/bindings.c" \
        "${ROOT}/host/bindings/board_registry.c" \
        "${ROOT}/host/bindings/builtin_modules.c" \
        "${ROOT}/host/bindings/validation.c" \
        "${ROOT}/${backend}/bindings/board.c" \
        "${ROOT}/${backend}/bindings/timers.c" \
        "${extra[@]}" -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" -lm \
        -o "${TMP_ROOT}/${target}"
}
compile_target pico platform/rp2 MCUJS_BOARD_PICO \
    -DMCUJS_PLATFORM_RP2=1 \
    "-I${ROOT}/board/pico" "-I${ROOT}/board" "-I${ROOT}/src" \
    "-I${ROOT}/src/filesystem" "-I${ROOT}/platform/rp2/bindings" \
    "-I${ROOT}/tests/native_stubs/rp2"
compile_target xiao platform/esp32/main MCUJS_BOARD_SEEED_XIAO_ESP32S3 \
    -DMCUJS_PLATFORM_ESP32=1 \
    "-I${ROOT}/platform/esp32/main" "-I${ROOT}/src/filesystem" \
    "-I${ROOT}/platform/esp32/main/bindings" \
    "-I${ROOT}/tests/native_stubs/esp32"
# Bounded even if a regression accidentally introduces an infinite JS loop.
for target in pico xiao; do timeout 30s "${TMP_ROOT}/${target}"; done
