#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT=$(mktemp -d);trap 'rm -rf "$OUT"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/host/bindings" -I"$ROOT/src/filesystem" "$ROOT/tests/speaker_wav_test.c" "$ROOT/host/bindings/speaker_wav.c" -o "$OUT/wav"
"$OUT/wav"
python3 - "$OUT" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1])
for name in ['hardware/pio.h','hardware/dma.h','hardware/gpio.h','hardware/clocks.h','hardware/sync.h','pico/time.h','speaker_i2s.pio.h']:
 f=p/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('#include "speaker_hardware_stubs.h"\n')
PY
cc -std=gnu17 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
 -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_2_8=1 \
 -I"$OUT" -I"$ROOT/host" -I"$ROOT/host/bindings" -I"$ROOT/tests" -I"$ROOT/src/filesystem" \
 -I"$ROOT/board/waveshare_rp2350_touch_lcd_2.8" -I"$ROOT/platform/rp2/bindings" \
 -I"$ROOT/tests/native_stubs/rp2" -I"$JERRYSCRIPT_PATH/jerry-core/include" \
 "$ROOT/tests/speaker_native_test.c" "$ROOT/host/bindings/speaker_wav.c" \
 "$ROOT/host/bindings/validation.c" "$ROOT/host/bindings/bindings.c" "$ROOT/platform/rp2/bindings/speaker.c" \
 -Wl,--gc-sections "$JERRYSCRIPT_BUILD/lib/libjerry-core.a" "$JERRYSCRIPT_BUILD/lib/libjerry-port.a" -lm -o "$OUT/native"
"$OUT/native"
