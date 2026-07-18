#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY_RP="$(mktemp "${TMPDIR:-/tmp}/mcujs-repl-rp-test.XXXXXX")"
BINARY_ESP="$(mktemp "${TMPDIR:-/tmp}/mcujs-repl-esp-test.XXXXXX")"
trap 'rm -f "${BINARY_RP}" "${BINARY_ESP}"' EXIT

compile_repl_test() {
    local output="$1"
    shift
    cc -std=gnu17 -Wall -Wextra -Werror \
        -DMCUJS_BUILD_ID='"test"' \
        "$@" \
        -I"${ROOT}/src" \
        -I"${ROOT}/src/usb" \
        -I"${ROOT}/src/filesystem" \
        -I"${ROOT}/host" \
        -I"${ROOT}/board" \
        -I"${ROOT}/platform/esp32/main" \
        "${ROOT}/tests/repl_input_test.c" \
        "${ROOT}/src/repl.c" \
        -o "${output}"
}

compile_repl_test "${BINARY_RP}"
"${BINARY_RP}"

compile_repl_test "${BINARY_ESP}" \
    -DMCUJS_PLATFORM_ESP32=1 \
    -DMCUJS_FEATURE_IMAGE=0 \
    -DMCUJS_FEATURE_KEYBOARD=0 \
    -DMCUJS_FEATURE_MOUSE=0 \
    -DMCUJS_FEATURE_GRAPHICS=0 \
    -DMCUJS_FEATURE_SCREEN=0
"${BINARY_ESP}"
