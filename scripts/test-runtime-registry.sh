#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FULL="$(mktemp "${TMPDIR:-/tmp}/mcujs-registry-full.XXXXXX")"
DVI="$(mktemp "${TMPDIR:-/tmp}/mcujs-registry-dvi.XXXXXX")"
CONSTRAINED_RP="$(mktemp "${TMPDIR:-/tmp}/mcujs-registry-constrained-rp.XXXXXX")"
CONSTRAINED="$(mktemp "${TMPDIR:-/tmp}/mcujs-registry-constrained.XXXXXX")"
trap 'rm -f "${FULL}" "${DVI}" "${CONSTRAINED_RP}" "${CONSTRAINED}"' EXIT

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

compile_registry_test "${DVI}" \
    -DMCUJS_BOARD_WAVESHARE_RP2040_PIZERO=1 \
    -DMCUJS_EXPECTED_BOARD='"waveshare_rp2040_pizero"' \
    -DMCUJS_EXPECTED_MANIFEST_NAME='"\"name\":\"waveshare_rp2040_pizero\""'
"${DVI}"

compile_registry_test "${CONSTRAINED_RP}" \
    -DMCUJS_BOARD_WAVESHARE_RP2350_LCD_1_47_A=1 \
    -DMCUJS_EXPECTED_BOARD='"waveshare_rp2350_lcd_1.47_a"' \
    -DMCUJS_EXPECTED_MANIFEST_NAME='"\"name\":\"waveshare_rp2350_lcd_1.47_a\""'
"${CONSTRAINED_RP}"

compile_registry_test "${CONSTRAINED}" \
    -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 \
    -DMCUJS_EXPECTED_BOARD='"seeed_xiao_esp32s3"' \
    -DMCUJS_EXPECTED_MANIFEST_NAME='"\"name\":\"seeed_xiao_esp32s3\""'
"${CONSTRAINED}"

compile_registry_test "${CONSTRAINED_RP}" \
    -DMCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_2_8=1 \
    -DMCUJS_EXPECTED_BOARD='"waveshare_rp2350_touch_lcd_2.8"' \
    -DMCUJS_EXPECTED_MANIFEST_NAME='"\"name\":\"waveshare_rp2350_touch_lcd_2.8\""'
"${CONSTRAINED_RP}"
