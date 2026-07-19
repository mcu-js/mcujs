#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FULL="$(mktemp "${TMPDIR:-/tmp}/mcujs-registry-full.XXXXXX")"
CONSTRAINED="$(mktemp "${TMPDIR:-/tmp}/mcujs-registry-constrained.XXXXXX")"
trap 'rm -f "${FULL}" "${CONSTRAINED}"' EXIT

compile_registry_test() {
    local output="$1"
    shift
    cc -std=gnu17 -Wall -Wextra -Werror \
        "$@" \
        -I"${ROOT}/host" \
        "${ROOT}/tests/runtime_registry_test.c" \
        "${ROOT}/host/runtime_registry.c" \
        -o "${output}"
}

compile_registry_test "${FULL}" \
    -DMCUJS_BOARD_PICO=1 \
    -DMCUJS_EXPECTED_BOARD='"pico"' \
    -DMCUJS_EXPECTED_MANIFEST_NAME='"\"name\":\"pico\""'
"${FULL}"

compile_registry_test "${CONSTRAINED}" \
    -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 \
    -DMCUJS_EXPECTED_BOARD='"seeed_xiao_esp32s3"' \
    -DMCUJS_EXPECTED_MANIFEST_NAME='"\"name\":\"seeed_xiao_esp32s3\""'
"${CONSTRAINED}"
