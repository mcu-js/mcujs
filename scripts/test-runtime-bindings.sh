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
# The real PWM/buzzer factory needs SDK alarm declarations in the host lane.
for name, text in {
 'pico/time.h': '#pragma once\n#include <stdint.h>\n#include <stdbool.h>\ntypedef int32_t alarm_id_t;\ntypedef int64_t (*alarm_callback_t)(alarm_id_t,void*);\nalarm_id_t add_alarm_in_ms(uint32_t,alarm_callback_t,void*,bool);\nbool cancel_alarm(alarm_id_t);\n',
 'hardware/sync.h': '#pragma once\n#include <stdint.h>\nstatic inline uint32_t save_and_disable_interrupts(void){return 0;}\nstatic inline void restore_interrupts(uint32_t n){(void)n;}\n',
}.items():
 path = out/name; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(text)
for name in ['events', 'devices', 'button', 'buzzer', 'speaker', 'microphone']:
 source = (root/('lib/'+name+'.js')).read_bytes()
 (out/(name+'_source.h')).write_text('static const jerry_char_t '+name+'_source[] = {' + ','.join(str(b) for b in source) + '};\n')
tests = (root/'tests/events.test.js').read_text()
script = (root/'tests/fs_binary_native.js').read_bytes() + b'\0'
(out/'fs_binary_test_source.h').write_text('static const char fs_binary_test_source[] = {' + ','.join(str(b) for b in script) + '};\n')
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
    local modules="$3"
    local display_sources=() peripheral_sources=()
    shift 3
    if [[ "${backend}" == "platform/rp2" ]]; then
        display_sources=(-Wno-type-limits)
        for name in graphics screen; do
            if [[ " ${modules} " == *" ${name} "* ]]; then
                display_sources+=("${ROOT}/host/bindings/${name}.c")
            fi
        done
    fi
    # Like firmware CMake, do not compile peripheral implementations that this
    # profile omits. Disabled route tables deliberately have no SDK arguments.
    for name in gpio i2c neopixel pwm; do
        if [[ " ${modules} " == *" ${name} "* ]]; then
            peripheral_sources+=("${ROOT}/${backend}/bindings/${name}.c")
            case "${name}" in
                i2c|neopixel) peripheral_sources+=("${ROOT}/host/bindings/${name}_options.c") ;;
            esac
        fi
    done
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
        "${ROOT}/host/bindings/pwm_policy.c" \
        "${ROOT}/host/bindings/board_registry.c" \
        "${ROOT}/host/bindings/fs.c" \
        "${ROOT}/host/bindings/require.c" \
        "${ROOT}/host/bindings/builtin_modules.c" \
        "${ROOT}/host/bindings/console.c" \
        "${display_sources[@]}" \
        "${ROOT}/${backend}/bindings/pin_policy.c" \
        "${peripheral_sources[@]}" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" \
        -lm \
        -o "${output}"
}

# Exercise the production loader/registry on every default configuration, not
# just representative Pico/RP2350/XIAO profiles. Physical drivers remain stubbed;
# configured-display opt-ins are exercised by the separate native tests below.
source "${ROOT}/scripts/lib/boards.sh"
BINDING_BOARDS=("${MCUJS_RELEASE_BOARDS[@]}" waveshare_esp32s3_epaper_1.54_v2 seeed_reterminal_sticky)
for board in "${BINDING_BOARDS[@]}"; do
    define="${board^^}"
    define="${define//./_}"
    binary="${TMP_ROOT}/runtime-bindings-${board}"
    flags=(-DMCUJS_USE_PRODUCTION_BINDING_HELPERS=1 "-DMCUJS_BOARD_${define}=1")
    case "${board}" in
        seeed_xiao_esp32s3|waveshare_esp32s3_epaper_1.54_v2|seeed_reterminal_sticky)
            backend=platform/esp32/main
            flags+=(-DMCUJS_PLATFORM_ESP32=1 -I"${ROOT}/tests/native_stubs/esp32" -I"${ROOT}/platform/esp32/main/bindings") ;;
        *)
            backend=platform/rp2
            flags+=(-DMCUJS_PLATFORM_RP2=1 -I"${ROOT}/tests/native_stubs/rp2" -I"${ROOT}/board/${board}") ;;
    esac
    modules="$(node -e 'console.log(require(process.argv[1]).boardDescriptors[process.argv[2]].modules.join(" "))' "${ROOT}/runtime/board-registry.js" "${board}")"
    compile_binding_test "${binary}" "${backend}" "${modules}" "${flags[@]}"
    nm -g "${binary}" > "${binary}.nm"
    for factory in gpio i2c neopixel pwm keyboard mouse graphics screen; do
        if [[ " ${modules} " == *" ${factory} "* ]]; then
            grep -Eq " T js_create_${factory}_module$" "${binary}.nm"
        elif grep -Eq " T js_create_${factory}_module$" "${binary}.nm"; then
            printf 'Unavailable factory was linked for %s: %s\n' "${board}" "${factory}" >&2
            exit 1
        fi
    done
    "${binary}"
done

JERRYSCRIPT_PATH="${JERRY_ROOT}" JERRYSCRIPT_BUILD="${JERRY_BUILD}" \
    bash "${ROOT}/tests/run-devices-display-tests.sh"
JERRYSCRIPT_PATH="${JERRY_ROOT}" JERRYSCRIPT_BUILD="${JERRY_BUILD}" \
    bash "${ROOT}/tests/run-buttons-tests.sh"
JERRYSCRIPT_PATH="${JERRY_ROOT}" JERRYSCRIPT_BUILD="${JERRY_BUILD}" \
    bash "${ROOT}/tests/run-esp-safe-mode-tests.sh"
JERRYSCRIPT_PATH="${JERRY_ROOT}" JERRYSCRIPT_BUILD="${JERRY_BUILD}" \
    bash "${ROOT}/tests/run-canvas-pointer-tests.sh"
JERRYSCRIPT_PATH="${JERRY_ROOT}" JERRYSCRIPT_BUILD="${JERRY_BUILD}" \
    bash "${ROOT}/tests/run-buzzer-tests.sh"
