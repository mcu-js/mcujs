#!/usr/bin/env bash
# Run inside the pinned SDK/Jerry Docker image; no hardware or network required.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${JERRYSCRIPT_PATH:?point at the external Jerry source}"
: "${JERRYSCRIPT_BUILD:?point at its external native build}"
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT
python3 - "$OUT" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1])
for name in ['driver/gpio.h','driver/i2c.h','driver/i2s_std.h','esp_timer.h','freertos/FreeRTOS.h','freertos/task.h']:
    p=root/name;p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text('#include "microphone_native_sdk.h"\n')
PY
cc -std=gnu17 -Wall -Wextra -Werror -pthread -g \
 -DMCUJS_BOARD_WAVESHARE_ESP32S3_EPAPER_1_54_V2=1 \
 -I"$OUT" -I"$ROOT/tests" -I"$JERRYSCRIPT_PATH/jerry-core/include" \
 "$ROOT/tests/microphone_native_test.c" "$ROOT/tests/microphone_native_sdk.c" \
 "$ROOT/platform/esp32/main/bindings/microphone.c" \
 "$JERRYSCRIPT_BUILD/lib/libjerry-core.a" "$JERRYSCRIPT_BUILD/lib/libjerry-port.a" \
 -Wl,--wrap=malloc -lm -o "$OUT/native"
"$OUT/native"
