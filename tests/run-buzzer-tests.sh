#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT=$(mktemp -d);trap 'rm -rf "$OUT"' EXIT
python3 - "$OUT" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1])
for name,text in {
'pico/time.h': '#pragma once\n#include <stdint.h>\n#include <stdbool.h>\ntypedef int32_t alarm_id_t;\ntypedef int64_t (*alarm_callback_t)(alarm_id_t,void*);\nalarm_id_t add_alarm_in_ms(uint32_t,alarm_callback_t,void*,bool);\nbool cancel_alarm(alarm_id_t);\n',
'hardware/sync.h':'#pragma once\n#include <stdint.h>\nstatic inline uint32_t save_and_disable_interrupts(void){return 0;}\nstatic inline void restore_interrupts(uint32_t n){(void)n;}\n',
'hardware/timer.h':'#pragma once\n'}.items():
 f=p/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(text)
PY
cc -std=gnu17 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
 -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_1_69=1 \
 -I"$OUT" -I"$ROOT/host" -I"$ROOT/host/bindings" -I"$ROOT/tests" \
 -I"$ROOT/board/waveshare_rp2350_touch_lcd_1.69" -I"$ROOT/platform/rp2/bindings" \
 -I"$ROOT/tests/native_stubs/rp2" -I"$JERRYSCRIPT_PATH/jerry-core/include" \
 "$ROOT/tests/buzzer_native_test.c" "$ROOT/tests/runtime_validation_backend_stubs.c" \
 "$ROOT/host/bindings/validation.c" "$ROOT/host/bindings/pwm_policy.c" \
 "$ROOT/platform/rp2/bindings/gpio.c" "$ROOT/platform/rp2/bindings/pin_policy.c" \
 "$ROOT/platform/rp2/bindings/pwm.c" "$ROOT/platform/rp2/bindings/timers.c" \
 -Wl,--gc-sections "$JERRYSCRIPT_BUILD/lib/libjerry-core.a" "$JERRYSCRIPT_BUILD/lib/libjerry-port.a" -lm -o "$OUT/test"
"$OUT/test"
