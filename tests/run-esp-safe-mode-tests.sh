#!/usr/bin/env bash
# Native board/boot fixture. No downloads or Jerry rebuilds.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JERRY_ROOT="${JERRYSCRIPT_PATH:?Set JERRYSCRIPT_PATH}"
JERRY_BUILD="${JERRYSCRIPT_BUILD:?Set JERRYSCRIPT_BUILD}"
TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT
python3 - "$ROOT" "$TMP_ROOT" <<'PY'
from pathlib import Path
import ast, sys
root, out = map(Path, sys.argv[1:])
# Reuse the board fixture's declaration-only SDK extensions.
s = (root / 'tests/run-buttons-tests.sh').read_text()
start = s.index('headers = {')
end = s.index('\nfor name, text', start)
headers = ast.literal_eval(s[start:end].split('=', 1)[1].strip())
headers.update({
'usb_cdc.h': '#pragma once\nvoid usb_cdc_puts(const char *);\n',
'esp_system.h': headers['esp_system.h'] + '#define ESP_RST_TASK_WDT 1\n#define ESP_RST_INT_WDT 2\n#define ESP_RST_WDT 3\n',
'esp_log.h': '#pragma once\n#define ESP_LOGE(tag, ...) ((void)(tag))\n#define ESP_LOGI(tag, ...) ((void)(tag))\n',
'nvs.h': '#pragma once\n#include <stdint.h>\n#include "esp_err.h"\ntypedef int nvs_handle_t;\n#define NVS_READWRITE 1\n#define ESP_ERR_NVS_NOT_FOUND 0x1102\nesp_err_t nvs_open(const char *, int, nvs_handle_t *);\nesp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *);\nesp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t);\nesp_err_t nvs_commit(nvs_handle_t);\n',
'nvs_flash.h': '#pragma once\n#include "esp_err.h"\nesp_err_t nvs_flash_init(void);\n',
})
for name, text in headers.items():
    path = out / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
PY
flags=(-std=gnu17 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
 -DMCUJS_USE_PRODUCTION_BINDING_HELPERS=1 -DMCUJS_PLATFORM_ESP32=1
 -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 '-DMCUJS_VERSION="0.1.0"'
 "-I$TMP_ROOT" "-I$ROOT/host" "-I$ROOT/host/bindings" "-I$ROOT/tests"
 "-I$JERRY_ROOT/jerry-core/include" "-I$ROOT/platform/esp32/main"
 "-I$ROOT/src/filesystem" "-I$ROOT/platform/esp32/main/bindings"
 "-I$ROOT/tests/native_stubs/esp32")
"${CC:-cc}" "${flags[@]}" "$ROOT/tests/esp_safe_mode_test.c" \
 "$ROOT/tests/runtime_validation_backend_stubs.c" \
 "$ROOT/host/runtime_registry.c" "$ROOT/host/bindings/bindings.c" \
 "$ROOT/host/bindings/board_registry.c" "$ROOT/host/bindings/validation.c" \
 "$ROOT/platform/esp32/main/bindings/board.c" "$ROOT/platform/esp32/main/boot.c" \
 -Wl,--gc-sections "$JERRY_BUILD/lib/libjerry-core.a" "$JERRY_BUILD/lib/libjerry-port.a" -lm \
 -o "$TMP_ROOT/esp-safe-mode"
timeout 10s "$TMP_ROOT/esp-safe-mode"
