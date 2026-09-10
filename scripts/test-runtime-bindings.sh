#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JERRY_COMMIT="50200152feb724a74a5f64e44d7885151537cfad"

find_jerryscript() {
    local candidate
    for candidate in "${JERRYSCRIPT_PATH:-}" /opt/jerryscript; do
        if [[ -n "${candidate}" && -f "${candidate}/tools/build.py" ]]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done
    return 1
}

if ! JERRY_ROOT="$(find_jerryscript)"; then
    if [[ "${MCUJS_BINDING_TEST_IN_DOCKER:-0}" != 1 ]] && \
       command -v docker >/dev/null 2>&1 && \
       docker image inspect mcujs-builder >/dev/null 2>&1; then
        exec docker run --rm \
            --entrypoint bash \
            -e MCUJS_BINDING_TEST_IN_DOCKER=1 \
            -v "${ROOT}:/workspace:ro" \
            -w /workspace \
            mcujs-builder \
            scripts/test-runtime-bindings.sh
    fi
    printf 'JerryScript v3.0.0 source is required. Set JERRYSCRIPT_PATH or build the mcujs-builder image.\n' >&2
    exit 1
fi

actual_jerry_commit="$(git -C "${JERRY_ROOT}" rev-parse HEAD 2>/dev/null || true)"
if [[ "${actual_jerry_commit}" != "${JERRY_COMMIT}" ]]; then
    printf 'JerryScript must be pinned to %s, found %s\n' \
        "${JERRY_COMMIT}" "${actual_jerry_commit:-unknown}" >&2
    exit 1
fi
if ! git -C "${JERRY_ROOT}" diff --quiet --ignore-submodules HEAD --; then
    printf 'JerryScript checkout must be clean: %s\n' "${JERRY_ROOT}" >&2
    exit 1
fi

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/mcujs-runtime-bindings.XXXXXX")"
trap 'rm -rf "${TMP_ROOT}"' EXIT
JERRY_BUILD="${TMP_ROOT}/jerry"
python3 - "$ROOT" "$TMP_ROOT" <<'PY'
from pathlib import Path
import sys
root, out = map(Path, sys.argv[1:])
for name in ['events', 'devices', 'button']:
 source = (root/('lib/'+name+'.js')).read_bytes()
 (out/(name+'_source.h')).write_text('static const jerry_char_t '+name+'_source[] = {' + ','.join(str(b) for b in source) + '};\n')
tests = (root/'tests/events.test.js').read_text()
imports = "const test = require('node:test');\nconst assert = require('node:assert/strict');\n"
assert tests.startswith(imports)
tests = tests[len(imports):].replace("require('../lib/events.js')", "require('events')")
script = '(function(){\n' + (root/'tests/events_native_harness.js').read_text() + tests + '\nassert.equal(completedEventTests, 12);\n})();'
(out/'events_test_source.h').write_text('static const char events_test_source[] = {' + ','.join(str(b) for b in script.encode()+b'\0') + '};\n')
PY

python3 "${JERRY_ROOT}/tools/build.py" \
    --builddir="${JERRY_BUILD}" \
    --jerry-cmdline=OFF \
    --jerry-ext=OFF \
    --jerry-math=OFF \
    --jerry-port=ON \
    --lto=OFF \
    --strip=OFF \
    --cpointer-32bit=OFF \
    --error-messages=ON \
    --cmake-param=-DJERRY_MEM_STATS=ON \
    --mem-heap=64 \
    >/dev/null

compile_binding_test() {
    local output="$1"
    local backend="$2"
    local display_sources=()
    shift 2
    if [[ "${backend}" == "platform/rp2" ]]; then
        display_sources=(
            -Wno-type-limits
            "${ROOT}/host/bindings/graphics.c"
            "${ROOT}/host/bindings/screen.c"
        )
    fi
    cc -std=gnu17 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections \
        "$@" \
        -I"${TMP_ROOT}" \
        -I"${ROOT}/host" \
        -I"${ROOT}/host/bindings" \
        -I"${ROOT}/src/filesystem" \
        -I"${ROOT}/src/usb" \
        -I"${ROOT}/tests" \
        -I"${JERRY_ROOT}/jerry-core/include" \
        "${ROOT}/tests/runtime_bindings_test.c" \
        "${ROOT}/tests/runtime_validation_backend_stubs.c" \
        "${ROOT}/host/runtime_registry.c" \
        "${ROOT}/host/bindings/bindings.c" \
        "${ROOT}/host/bindings/validation.c" \
        "${ROOT}/host/bindings/i2c_options.c" \
        "${ROOT}/host/bindings/neopixel_options.c" \
        "${ROOT}/host/bindings/pwm_policy.c" \
        "${ROOT}/host/bindings/board_registry.c" \
        "${ROOT}/host/bindings/fs.c" \
        "${ROOT}/host/bindings/require.c" \
        "${ROOT}/host/bindings/builtin_modules.c" \
        "${ROOT}/host/bindings/console.c" \
        "${display_sources[@]}" \
        "${ROOT}/${backend}/bindings/pin_policy.c" \
        "${ROOT}/${backend}/bindings/gpio.c" \
        "${ROOT}/${backend}/bindings/i2c.c" \
        "${ROOT}/${backend}/bindings/neopixel.c" \
        "${ROOT}/${backend}/bindings/pwm.c" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" \
        -lm \
        -o "${output}"
}

FULL="${TMP_ROOT}/runtime-bindings-full"
CONSTRAINED_RP="${TMP_ROOT}/runtime-bindings-constrained-rp"
CONSTRAINED="${TMP_ROOT}/runtime-bindings-constrained"

compile_binding_test "${FULL}" platform/rp2 \
    -DMCUJS_USE_PRODUCTION_BINDING_HELPERS=1 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_PICO=1 \
    -I"${ROOT}/tests/native_stubs/rp2"
for factory in gpio graphics i2c neopixel pwm screen; do
    nm -g "${FULL}" | grep -E " T js_create_${factory}_module$" >/dev/null
done
for factory in keyboard mouse; do
    nm -g "${FULL}" | grep -E " T js_create_${factory}_module$" >/dev/null
done
"${FULL}"

compile_binding_test "${CONSTRAINED_RP}" platform/rp2 \
    -DMCUJS_USE_PRODUCTION_BINDING_HELPERS=1 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_WAVESHARE_RP2350_LCD_1_47_A=1 \
    -I"${ROOT}/tests/native_stubs/rp2"
for factory in gpio graphics i2c neopixel pwm screen; do
    nm -g "${CONSTRAINED_RP}" | grep -E " T js_create_${factory}_module$" >/dev/null
done
for factory in keyboard mouse; do
    nm -g "${CONSTRAINED_RP}" | grep -E " T js_create_${factory}_module$" >/dev/null
done
"${CONSTRAINED_RP}"

compile_binding_test "${CONSTRAINED}" platform/esp32/main \
    -DMCUJS_USE_PRODUCTION_BINDING_HELPERS=1 \
    -DMCUJS_PLATFORM_ESP32=1 -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 \
    -I"${ROOT}/tests/native_stubs/esp32" \
    -I"${ROOT}/platform/esp32/main/bindings"
for factory in gpio i2c neopixel pwm; do
    nm -g "${CONSTRAINED}" | grep -E " T js_create_${factory}_module$" >/dev/null
done
for factory in keyboard mouse; do
    if nm -g "${CONSTRAINED}" | grep -E " T js_create_${factory}_module$" >/dev/null; then
        printf 'Unavailable ESP32 HID factory was linked: %s\n' "${factory}" >&2
        exit 1
    fi
done
for factory in graphics screen; do
    if nm -g "${CONSTRAINED}" | grep -E " T js_create_${factory}_module$" >/dev/null; then
        printf 'Unavailable ESP32 display factory was linked: %s\n' "${factory}" >&2
        exit 1
    fi
done
"${CONSTRAINED}"

JERRYSCRIPT_PATH="${JERRY_ROOT}" JERRYSCRIPT_BUILD="${JERRY_BUILD}" \
    bash "${ROOT}/tests/run-devices-display-tests.sh"
JERRYSCRIPT_PATH="${JERRY_ROOT}" JERRYSCRIPT_BUILD="${JERRY_BUILD}" \
    bash "${ROOT}/tests/run-buttons-tests.sh"
