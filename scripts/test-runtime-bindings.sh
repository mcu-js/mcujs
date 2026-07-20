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
    shift 2
    cc -std=gnu17 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections \
        "$@" \
        -I"${ROOT}/host" \
        -I"${ROOT}/host/bindings" \
        -I"${ROOT}/src/filesystem" \
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
        "${ROOT}/host/bindings/require.c" \
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
for factory in gpio i2c neopixel pwm; do
    nm -g "${FULL}" | grep -Eq " T js_create_${factory}_module$"
done
"${FULL}"

compile_binding_test "${CONSTRAINED_RP}" platform/rp2 \
    -DMCUJS_USE_PRODUCTION_BINDING_HELPERS=1 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_WAVESHARE_RP2350_LCD_1_47_A=1 \
    -I"${ROOT}/tests/native_stubs/rp2"
for factory in gpio i2c neopixel pwm; do
    nm -g "${CONSTRAINED_RP}" | grep -Eq " T js_create_${factory}_module$"
done
"${CONSTRAINED_RP}"

compile_binding_test "${CONSTRAINED}" platform/esp32/main \
    -DMCUJS_USE_PRODUCTION_BINDING_HELPERS=1 \
    -DMCUJS_PLATFORM_ESP32=1 -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 \
    -I"${ROOT}/tests/native_stubs/esp32" \
    -I"${ROOT}/platform/esp32/main/bindings"
for factory in gpio i2c neopixel pwm; do
    nm -g "${CONSTRAINED}" | grep -Eq " T js_create_${factory}_module$"
done
"${CONSTRAINED}"
