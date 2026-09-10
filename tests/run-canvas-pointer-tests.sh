#!/usr/bin/env bash
# SDK fakes only. Use the separately built native Jerry tree; never modify it.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
JERRY_ROOT="${JERRYSCRIPT_PATH:-/opt/jerryscript}"
JERRY_BUILD="${JERRYSCRIPT_BUILD:-$JERRY_ROOT/build}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
python3 - "$ROOT" "$TMP" <<'PY'
from pathlib import Path
import sys
root,out=map(Path,sys.argv[1:])
for name,source in [('events','events'),('devices','devices'),('button','button'),('canvas','canvas'),('st7789','displays/st7789')]:
 data=(root/('lib/'+source+'.js')).read_bytes()
 (out/(name+'_source.h')).write_text('static const jerry_char_t '+name+'_source[]={'+','.join(map(str,data))+'};\n')
PY
cc -std=gnu17 -Wall -Wextra -Werror -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
 -DMCUJS_PLATFORM_RP2 -DMCUJS_EXPERIMENTAL_CANVAS -DMCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_1_69 \
 -DMCUJS_CANVAS_DEFAULT_ST7789 -DMCUJS_CANVAS_DEFAULT_ST7789_1_69 \
 -I"$TMP" -I"$ROOT/host" -I"$ROOT/host/bindings" -I"$ROOT/platform/rp2/bindings" \
 -I"$ROOT/tests/native_stubs/rp2" -I"$JERRY_ROOT/jerry-core/include" \
 "$ROOT/tests/canvas_pointer_jerry_test.c" "$ROOT/host/bindings/builtin_modules.c" \
 -isystem "${CTX_PATH:-/opt/ctx}" "$ROOT/host/bindings/canvas_renderer.c" \
 "$ROOT/host/bindings/canvas_native.c" "$ROOT/host/bindings/validation.c" \
 "$ROOT/host/bindings/i2c_options.c" "$ROOT/host/runtime_registry.c" \
 "$ROOT/platform/rp2/bindings/i2c.c" "$ROOT/platform/rp2/bindings/pin_policy.c" \
 -Wl,--gc-sections "$JERRY_BUILD/lib/libjerry-core.a" "$JERRY_BUILD/lib/libjerry-port.a" -lm -o "$TMP/pointer"
"$TMP/pointer"
