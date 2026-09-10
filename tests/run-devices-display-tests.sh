#!/usr/bin/env bash
# No downloads, SDK setup or Jerry rebuilds. Point at a separately staged native
# JerryScript 3.0.0 tree built with --mem-heap=64 and JERRY_MEM_STATS=ON.
# This script only reads that tree, and builds both adapters in a temporary dir.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JERRY_ROOT="${JERRYSCRIPT_PATH:-/opt/jerryscript}"
CC="${CC:-cc}"
JERRY_BUILD="${JERRYSCRIPT_BUILD:-${JERRY_ROOT}/build}"
CTX_PATH="${CTX_PATH:-/opt/ctx}"
printf '%s  %s\n' bf2b18bcea31bf191f7e82c9beef98bd717bd5ccd635908f07ab8a8cf1698bb3 "${CTX_PATH}/ctx.h" | sha256sum -c - >/dev/null
for file in "${JERRY_ROOT}/jerry-core/include/jerryscript.h" "${JERRY_BUILD}/lib/libjerry-core.a" "${JERRY_BUILD}/lib/libjerry-port.a"; do
    if [[ ! -f "${file}" ]]; then
        printf 'Missing %s/%s; set JERRYSCRIPT_PATH to an externally built native Jerry tree (64 KiB heap, memory stats ON).\n' "$JERRY_ROOT" "$file" >&2
        exit 1
    fi
done
if [[ ! -f "${ROOT}/lib/devices.js" ]]; then
    printf 'Missing production lib/devices.js; integrate the configured-display implementation before running this test.\n' >&2
    exit 1
fi
TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/mcujs-devices-display.XXXXXX")"
trap 'rm -rf "${TMP_ROOT}"' EXIT
python3 - "$ROOT" "$TMP_ROOT" <<'PY'
from pathlib import Path
import sys
root, out = map(Path, sys.argv[1:])
for name, source in (
    ('events', 'lib/events.js'),
    ('canvas', 'lib/canvas.js'),
    ('devices', 'lib/devices.js'),
    ('button', 'lib/button.js'),
    ('st7789', 'lib/displays/st7789.js'),
):
    data = (root / source).read_bytes()
    # builtin_modules.c passes sizeof(array); do NOT add a trailing NUL.
    (out / (name + '_source.h')).write_text(
        'static const jerry_char_t ' + name + '_source[] = {' +
        ','.join(map(str, data)) + '};\n')
data=(root/'examples/portable/device-display/draw.js').read_bytes()+b'\0'
(out/'portable_draw_source.h').write_text('static const char portable_draw_source[] = {'+','.join(map(str,data))+'};\n')
PY

compile_and_run() {
    local name="$1" adapter="$2" board="$3"
    local flags=(
        -std=gnu17 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
        -DMCUJS_PLATFORM_ESP32=1 -DMCUJS_EXPERIMENTAL_CANVAS=1
        "-D${adapter}=1" "-D${board}=1"
        "-I${TMP_ROOT}" "-I${ROOT}/host" "-I${ROOT}/host/bindings"
        "-I${ROOT}/tests" "-I${ROOT}/tests/canvas_epaper_stubs"
        "-I${JERRY_ROOT}/jerry-core/include" -isystem "${CTX_PATH}"
    )
    # Redirect only the native Canvas record allocator, never the Jerry heap or
    # production driver. The existing fixture tracks PSRAM at the IDF boundary.
    "$CC" "${flags[@]}" \
        -Dcalloc=mcujs_test_canvas_calloc -Dfree=mcujs_test_canvas_free \
        -c "${ROOT}/host/bindings/canvas_native.c" -o "${TMP_ROOT}/${name}-native.o"
    "$CC" "${flags[@]}" \
        "${ROOT}/tests/devices_display_jerry_test.c" \
        "${ROOT}/host/runtime_registry.c" \
        "${ROOT}/host/bindings/bindings.c" \
        "${ROOT}/host/bindings/board_registry.c" \
        "${ROOT}/host/bindings/builtin_modules.c" \
        "${ROOT}/host/bindings/canvas_renderer.c" \
        "${ROOT}/host/bindings/validation.c" \
        "${TMP_ROOT}/${name}-native.o" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" -lm \
        -o "${TMP_ROOT}/${name}"
    "${TMP_ROOT}/${name}"
}
compile_and_run sticky MCUJS_CANVAS_STICKY MCUJS_BOARD_SEEED_RETERMINAL_STICKY
compile_and_run epaper154 MCUJS_CANVAS_EPAPER154 MCUJS_BOARD_WAVESHARE_ESP32S3_EPAPER_1_54_V2
