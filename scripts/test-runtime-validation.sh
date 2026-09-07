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
    if [[ "${MCUJS_VALIDATION_TEST_IN_DOCKER:-0}" != 1 ]] && \
       command -v docker >/dev/null 2>&1 && \
       docker image inspect mcujs-builder >/dev/null 2>&1; then
        exec docker run --rm \
            --entrypoint bash \
            -e MCUJS_VALIDATION_TEST_IN_DOCKER=1 \
            -v "${ROOT}:/workspace:ro" \
            -w /workspace \
            mcujs-builder \
            scripts/test-runtime-validation.sh
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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/mcujs-runtime-validation.XXXXXX")"
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
    --mem-heap=64 \
    >/dev/null

compile_validation_test() {
    local output="$1"
    cc -std=gnu17 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections \
        -I"${ROOT}/host/bindings" \
        -I"${JERRY_ROOT}/jerry-core/include" \
        "${ROOT}/tests/runtime_validation_test.c" \
        "${ROOT}/host/bindings/validation.c" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" \
        -lm \
        -o "${output}"
}

compile_pwm_policy_test() {
    local output="$1"
    cc -std=gnu17 -Wall -Wextra -Werror \
        -I"${ROOT}/host/bindings" \
        "${ROOT}/tests/pwm_policy_test.c" \
        "${ROOT}/host/bindings/pwm_policy.c" \
        -lm \
        -o "${output}"
}

assert_global_text_symbol() {
    local binary="$1"
    local symbol="$2"
    local symbols="${binary}.nm"
    nm -g "${binary}" >"${symbols}"
    grep -Eq " T ${symbol}$" "${symbols}"
}

compile_backend_test() {
    local output="$1"
    local backend="$2"
    shift 2
    cc -std=gnu17 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections \
        "$@" \
        -I"${ROOT}/host" \
        -I"${ROOT}/host/bindings" \
        -I"${ROOT}/tests" \
        -I"${JERRY_ROOT}/jerry-core/include" \
        "${ROOT}/tests/runtime_validation_backend_test.c" \
        "${ROOT}/tests/runtime_validation_backend_stubs.c" \
        "${ROOT}/host/bindings/validation.c" \
        "${ROOT}/host/bindings/i2c_options.c" \
        "${ROOT}/host/bindings/spi_options.c" \
        "${ROOT}/host/bindings/neopixel_options.c" \
        "${ROOT}/host/bindings/pwm_policy.c" \
        "${ROOT}/${backend}/bindings/gpio.c" \
        "${ROOT}/${backend}/bindings/i2c.c" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" \
        -lm \
        -o "${output}"
}

compile_i2c_options_board_test() {
    local output="$1"
    cc -std=gnu17 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections \
        -DMCUJS_PLATFORM_RP2=1 \
        -DMCUJS_BOARD_WAVESHARE_RP2040_TOUCH_LCD_1_28=1 \
        -I"${ROOT}/host" \
        -I"${ROOT}/host/bindings" \
        -I"${ROOT}/board/waveshare_rp2040_touch_lcd_1.28" \
        -I"${ROOT}/platform/rp2/bindings" \
        -I"${ROOT}/tests" \
        -I"${ROOT}/tests/native_stubs/rp2" \
        -I"${JERRY_ROOT}/jerry-core/include" \
        "${ROOT}/tests/i2c_options_board_test.c" \
        "${ROOT}/tests/runtime_validation_backend_stubs.c" \
        "${ROOT}/host/bindings/validation.c" \
        "${ROOT}/host/bindings/i2c_options.c" \
        "${ROOT}/platform/rp2/bindings/pin_policy.c" \
        "${ROOT}/platform/rp2/bindings/i2c.c" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" \
        -lm \
        -o "${output}"
}

compile_pwm_resource_backend_test() {
    local output="$1"
    local backend="$2"
    shift 2
    cc -std=gnu17 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections \
        "$@" \
        -I"${ROOT}/host" \
        -I"${ROOT}/host/bindings" \
        -I"${ROOT}/tests" \
        -I"${JERRY_ROOT}/jerry-core/include" \
        "${ROOT}/tests/pwm_resource_backend_test.c" \
        "${ROOT}/tests/runtime_validation_backend_stubs.c" \
        "${ROOT}/host/bindings/validation.c" \
        "${ROOT}/host/bindings/pwm_policy.c" \
        "${ROOT}/${backend}/bindings/gpio.c" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" \
        -lm \
        -o "${output}"
}

compile_adc_backend_test() {
    local output="$1"
    local backend="$2"
    shift 2
    cc -std=gnu17 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections \
        "$@" \
        -I"${ROOT}/host" \
        -I"${ROOT}/host/bindings" \
        -I"${ROOT}/tests" \
        -I"${JERRY_ROOT}/jerry-core/include" \
        "${ROOT}/tests/adc_backend_test.c" \
        "${ROOT}/tests/runtime_validation_backend_stubs.c" \
        "${ROOT}/host/bindings/validation.c" \
        "${ROOT}/${backend}/bindings/pin_policy.c" \
        "${ROOT}/${backend}/bindings/adc.c" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" \
        -lm \
        -o "${output}"
}

compile_spi_backend_test() {
    local output="$1"
    local backend="$2"
    shift 2
    cc -std=gnu17 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections \
        "$@" \
        -I"${ROOT}/host" \
        -I"${ROOT}/host/bindings" \
        -I"${ROOT}/tests" \
        -I"${JERRY_ROOT}/jerry-core/include" \
        "${ROOT}/tests/spi_backend_test.c" \
        "${ROOT}/tests/runtime_validation_backend_stubs.c" \
        "${ROOT}/host/bindings/validation.c" \
        "${ROOT}/host/bindings/spi_options.c" \
        "${ROOT}/${backend}/bindings/pin_policy.c" \
        "${ROOT}/${backend}/bindings/spi.c" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" \
        -lm \
        -o "${output}"
}

compile_neopixel_backend_test() {
    local output="$1"
    local backend="$2"
    shift 2
    cc -std=gnu17 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections \
        "$@" \
        -I"${ROOT}/host" \
        -I"${ROOT}/host/bindings" \
        -I"${ROOT}/tests" \
        -I"${JERRY_ROOT}/jerry-core/include" \
        "${ROOT}/tests/neopixel_backend_test.c" \
        "${ROOT}/tests/runtime_validation_backend_stubs.c" \
        "${ROOT}/host/bindings/validation.c" \
        "${ROOT}/host/bindings/neopixel_options.c" \
        "${ROOT}/${backend}/bindings/pin_policy.c" \
        "${ROOT}/${backend}/bindings/neopixel.c" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" \
        -lm \
        -o "${output}"
}

VALIDATION_TEST="${TMP_ROOT}/runtime-validation-shared"
compile_validation_test "${VALIDATION_TEST}"
"${VALIDATION_TEST}"

PWM_POLICY_TEST="${TMP_ROOT}/pwm-policy"
compile_pwm_policy_test "${PWM_POLICY_TEST}"
"${PWM_POLICY_TEST}"

RP2_BACKEND_TEST="${TMP_ROOT}/runtime-validation-rp2-backend"
compile_backend_test "${RP2_BACKEND_TEST}" platform/rp2 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_PICO=1 \
    -I"${ROOT}/board/pico" \
    -I"${ROOT}/platform/rp2/bindings" \
    -I"${ROOT}/tests/native_stubs/rp2" \
    "${ROOT}/platform/rp2/bindings/pin_policy.c" \
    "${ROOT}/platform/rp2/bindings/pwm.c" \
    "${ROOT}/platform/rp2/bindings/spi.c" \
    "${ROOT}/platform/rp2/bindings/adc.c" \
    "${ROOT}/platform/rp2/bindings/neopixel.c" \
    "${ROOT}/platform/rp2/bindings/onboard_led.c"
for factory in gpio i2c pwm spi adc neopixel; do
    assert_global_text_symbol "${RP2_BACKEND_TEST}" "js_create_${factory}_module"
done
"${RP2_BACKEND_TEST}"

I2C_OPTIONS_DEFAULT_BUS_1_TEST="${TMP_ROOT}/i2c-options-default-bus-1"
compile_i2c_options_board_test "${I2C_OPTIONS_DEFAULT_BUS_1_TEST}"
assert_global_text_symbol "${I2C_OPTIONS_DEFAULT_BUS_1_TEST}" \
    "js_create_i2c_module"
"${I2C_OPTIONS_DEFAULT_BUS_1_TEST}"

RP2_PWM_RESOURCE_TEST="${TMP_ROOT}/pwm-resource-rp2"
compile_pwm_resource_backend_test "${RP2_PWM_RESOURCE_TEST}" platform/rp2 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_PICO=1 \
    -I"${ROOT}/board/pico" \
    -I"${ROOT}/platform/rp2/bindings" \
    -I"${ROOT}/tests/native_stubs/rp2" \
    "${ROOT}/platform/rp2/bindings/pin_policy.c" \
    "${ROOT}/platform/rp2/bindings/pwm.c"
"${RP2_PWM_RESOURCE_TEST}"

RP2_ADC_TEST="${TMP_ROOT}/adc-rp2"
compile_adc_backend_test "${RP2_ADC_TEST}" platform/rp2 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_PICO=1 \
    -I"${ROOT}/board/pico" \
    -I"${ROOT}/platform/rp2/bindings" \
    -I"${ROOT}/tests/native_stubs/rp2"
assert_global_text_symbol "${RP2_ADC_TEST}" "js_create_adc_module"
"${RP2_ADC_TEST}"

RP2_SPI_TEST="${TMP_ROOT}/spi-rp2"
compile_spi_backend_test "${RP2_SPI_TEST}" platform/rp2 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_PICO=1 \
    -I"${ROOT}/board/pico" \
    -I"${ROOT}/platform/rp2/bindings" \
    -I"${ROOT}/tests/native_stubs/rp2"
assert_global_text_symbol "${RP2_SPI_TEST}" "js_create_spi_module"
"${RP2_SPI_TEST}"

RP2_SPI_DEFAULT_BUS_1_TEST="${TMP_ROOT}/spi-rp2-default-bus-1"
compile_spi_backend_test "${RP2_SPI_DEFAULT_BUS_1_TEST}" platform/rp2 \
    -DMCUJS_PLATFORM_RP2=1 \
    -DMCUJS_BOARD_WAVESHARE_RP2040_TOUCH_LCD_1_28=1 \
    -I"${ROOT}/board/waveshare_rp2040_touch_lcd_1.28" \
    -I"${ROOT}/platform/rp2/bindings" \
    -I"${ROOT}/tests/native_stubs/rp2"
assert_global_text_symbol "${RP2_SPI_DEFAULT_BUS_1_TEST}" \
    "js_create_spi_module"
"${RP2_SPI_DEFAULT_BUS_1_TEST}"

RP2_NEOPIXEL_TEST="${TMP_ROOT}/neopixel-rp2"
compile_neopixel_backend_test "${RP2_NEOPIXEL_TEST}" platform/rp2 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_WAVESHARE_RP2040_PIZERO=1 \
    -I"${ROOT}/board/waveshare_rp2040_pizero" \
    -I"${ROOT}/platform/rp2/bindings" \
    -I"${ROOT}/tests/native_stubs/rp2"
assert_global_text_symbol "${RP2_NEOPIXEL_TEST}" "js_create_neopixel_module"
"${RP2_NEOPIXEL_TEST}"

RP2350_BACKEND_TEST="${TMP_ROOT}/runtime-validation-rp2350-backend"
compile_backend_test "${RP2350_BACKEND_TEST}" platform/rp2 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_PICO2=1 \
    -I"${ROOT}/board/pico2" \
    -I"${ROOT}/platform/rp2/bindings" \
    -I"${ROOT}/tests/native_stubs/rp2" \
    "${ROOT}/platform/rp2/bindings/pin_policy.c" \
    "${ROOT}/platform/rp2/bindings/pwm.c" \
    "${ROOT}/platform/rp2/bindings/spi.c" \
    "${ROOT}/platform/rp2/bindings/adc.c" \
    "${ROOT}/platform/rp2/bindings/neopixel.c" \
    "${ROOT}/platform/rp2/bindings/onboard_led.c"
for factory in gpio i2c pwm spi adc neopixel; do
    assert_global_text_symbol "${RP2350_BACKEND_TEST}" "js_create_${factory}_module"
done
"${RP2350_BACKEND_TEST}"

RP2350_PWM_RESOURCE_TEST="${TMP_ROOT}/pwm-resource-rp2350"
compile_pwm_resource_backend_test "${RP2350_PWM_RESOURCE_TEST}" platform/rp2 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_PICO2=1 \
    -I"${ROOT}/board/pico2" \
    -I"${ROOT}/platform/rp2/bindings" \
    -I"${ROOT}/tests/native_stubs/rp2" \
    "${ROOT}/platform/rp2/bindings/pin_policy.c" \
    "${ROOT}/platform/rp2/bindings/pwm.c"
"${RP2350_PWM_RESOURCE_TEST}"

RP2350_ADC_TEST="${TMP_ROOT}/adc-rp2350"
compile_adc_backend_test "${RP2350_ADC_TEST}" platform/rp2 \
    -DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_PICO2=1 \
    -I"${ROOT}/board/pico2" \
    -I"${ROOT}/platform/rp2/bindings" \
    -I"${ROOT}/tests/native_stubs/rp2"
assert_global_text_symbol "${RP2350_ADC_TEST}" "js_create_adc_module"
"${RP2350_ADC_TEST}"

compile_rp_board_surface_test() {
    local output="$1"
    local board_id="$2"
    local board_macro="$3"
    shift 3
    cc -std=gnu17 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections \
        -DMCUJS_PLATFORM_RP2=1 -D"${board_macro}"=1 \
        -DMCUJS_VERSION='"0.1.0"' \
        "$@" \
        -I"${ROOT}/host" \
        -I"${ROOT}/host/bindings" \
        -I"${ROOT}/board" \
        -I"${ROOT}/board/${board_id}" \
        -I"${ROOT}/src" \
        -I"${ROOT}/src/filesystem" \
        -I"${ROOT}/platform/rp2/bindings" \
        -I"${ROOT}/tests" \
        -I"${ROOT}/tests/native_stubs/rp2" \
        -I"${JERRY_ROOT}/jerry-core/include" \
        "${ROOT}/tests/rp2_board_surface_test.c" \
        "${ROOT}/tests/runtime_validation_backend_stubs.c" \
        "${ROOT}/host/bindings/validation.c" \
        "${ROOT}/host/bindings/neopixel_options.c" \
        "${ROOT}/platform/rp2/bindings/pin_policy.c" \
        "${ROOT}/platform/rp2/bindings/gpio.c" \
        "${ROOT}/platform/rp2/bindings/adc.c" \
        "${ROOT}/platform/rp2/bindings/neopixel.c" \
        "${ROOT}/platform/rp2/bindings/onboard_led.c" \
        "${ROOT}/platform/rp2/bindings/board.c" \
        -Wl,--gc-sections \
        "${JERRY_BUILD}/lib/libjerry-core.a" \
        "${JERRY_BUILD}/lib/libjerry-port.a" \
        -lm \
        -o "${output}"
}

RP2_BUTTON_SURFACE_TEST="${TMP_ROOT}/runtime-rp2-button-surface"
compile_rp_board_surface_test "${RP2_BUTTON_SURFACE_TEST}" \
    pico MCUJS_BOARD_PICO \
    -DMCUJS_TEST_EXPECT_LED_PIN=1 -DMCUJS_TEST_LED_PIN_STRING='"25"' \
    -DMCUJS_TEST_EXPECT_LED_METHOD=1 -DMCUJS_TEST_EXPECT_BUTTON=1 \
    -DMCUJS_TEST_EXPECT_VSYS=1 -DMCUJS_TEST_EXPECT_NEOPIXEL=0 \
    -DMCUJS_TEST_EXPECT_ADC_CHANNEL3=1
"${RP2_BUTTON_SURFACE_TEST}"

RP2_NO_LED_SURFACE_TEST="${TMP_ROOT}/runtime-rp2-no-led-surface"
compile_rp_board_surface_test "${RP2_NO_LED_SURFACE_TEST}" \
    waveshare_rp2040_touch_lcd_1.28 MCUJS_BOARD_WAVESHARE_RP2040_TOUCH_LCD_1_28 \
    -DMCUJS_TEST_EXPECT_LED_PIN=0 -DMCUJS_TEST_EXPECT_LED_METHOD=0 \
    -DMCUJS_TEST_EXPECT_VSYS=0 -DMCUJS_TEST_EXPECT_NEOPIXEL=0 \
    -DMCUJS_TEST_EXPECT_ADC_CHANNEL3=0 \
    -DMCUJS_TEST_INPUT_ONLY_PIN_STRING='"21"'
"${RP2_NO_LED_SURFACE_TEST}"

RP2_INPUT_ONLY_SURFACE_TEST="${TMP_ROOT}/runtime-rp2-input-only-surface"
compile_rp_board_surface_test "${RP2_INPUT_ONLY_SURFACE_TEST}" \
    waveshare_rp2350_touch_lcd_1.69 MCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_1_69 \
    -DMCUJS_TEST_EXPECT_LED_PIN=0 -DMCUJS_TEST_EXPECT_LED_METHOD=0 \
    -DMCUJS_TEST_EXPECT_VSYS=0 -DMCUJS_TEST_EXPECT_NEOPIXEL=0 \
    -DMCUJS_TEST_EXPECT_ADC_CHANNEL3=0 \
    -DMCUJS_TEST_INPUT_ONLY_PIN_STRING='"23"'
"${RP2_INPUT_ONLY_SURFACE_TEST}"

RP2_CYW43_SURFACE_TEST="${TMP_ROOT}/runtime-rp2-cyw43-surface"
compile_rp_board_surface_test "${RP2_CYW43_SURFACE_TEST}" \
    pico2_w MCUJS_BOARD_PICO2_W -DMCUJS_HAS_CYW43=1 \
    -DMCUJS_TEST_EXPECT_LED_PIN=0 -DMCUJS_TEST_EXPECT_LED_METHOD=1 \
    -DMCUJS_TEST_EXPECT_VSYS=0 -DMCUJS_TEST_EXPECT_NEOPIXEL=0 \
    -DMCUJS_TEST_EXPECT_ADC_CHANNEL3=0
"${RP2_CYW43_SURFACE_TEST}"

RP2_NEOPIXEL_SURFACE_TEST="${TMP_ROOT}/runtime-rp2-neopixel-surface"
compile_rp_board_surface_test "${RP2_NEOPIXEL_SURFACE_TEST}" \
    adafruit_feather_rp2040 MCUJS_BOARD_ADAFRUIT_FEATHER_RP2040 \
    -DMCUJS_TEST_EXPECT_LED_PIN=1 -DMCUJS_TEST_LED_PIN_STRING='"13"' \
    -DMCUJS_TEST_EXPECT_LED_METHOD=1 -DMCUJS_TEST_EXPECT_VSYS=0 \
    -DMCUJS_TEST_EXPECT_NEOPIXEL=1 -DMCUJS_TEST_EXPECT_ADC_CHANNEL3=1
"${RP2_NEOPIXEL_SURFACE_TEST}"

RP2_RGB_NEOPIXEL_SURFACE_TEST="${TMP_ROOT}/runtime-rp2-rgb-neopixel-surface"
compile_rp_board_surface_test "${RP2_RGB_NEOPIXEL_SURFACE_TEST}" \
    waveshare_rp2040_zero MCUJS_BOARD_WAVESHARE_RP2040_ZERO \
    -DMCUJS_TEST_EXPECT_LED_PIN=0 -DMCUJS_TEST_EXPECT_LED_METHOD=0 \
    -DMCUJS_TEST_EXPECT_VSYS=0 -DMCUJS_TEST_EXPECT_NEOPIXEL=1 \
    -DMCUJS_TEST_EXPECT_ADC_CHANNEL3=1
"${RP2_RGB_NEOPIXEL_SURFACE_TEST}"

ESP32_BACKEND_TEST="${TMP_ROOT}/runtime-validation-esp32-backend"
compile_backend_test "${ESP32_BACKEND_TEST}" platform/esp32/main \
    -DMCUJS_PLATFORM_ESP32=1 -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 \
    -I"${ROOT}/tests/native_stubs/esp32" \
    -I"${ROOT}/platform/esp32/main/bindings" \
    "${ROOT}/platform/esp32/main/bindings/pin_policy.c" \
    "${ROOT}/platform/esp32/main/bindings/pwm.c"
for factory in gpio i2c pwm; do
    assert_global_text_symbol "${ESP32_BACKEND_TEST}" "js_create_${factory}_module"
done
"${ESP32_BACKEND_TEST}"

ESP32_SPI_TEST="${TMP_ROOT}/spi-esp32"
compile_spi_backend_test "${ESP32_SPI_TEST}" platform/esp32/main \
    -DMCUJS_PLATFORM_ESP32=1 -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 \
    -I"${ROOT}/tests/native_stubs/esp32" \
    -I"${ROOT}/platform/esp32/main/bindings"
assert_global_text_symbol "${ESP32_SPI_TEST}" "js_create_spi_module"
"${ESP32_SPI_TEST}"

ESP32_NEOPIXEL_TEST="${TMP_ROOT}/neopixel-esp32"
compile_neopixel_backend_test "${ESP32_NEOPIXEL_TEST}" platform/esp32/main \
    -DMCUJS_PLATFORM_ESP32=1 -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 \
    -I"${ROOT}/tests/native_stubs/esp32" \
    -I"${ROOT}/platform/esp32/main/bindings"
assert_global_text_symbol "${ESP32_NEOPIXEL_TEST}" "js_create_neopixel_module"
"${ESP32_NEOPIXEL_TEST}"

ESP32_PWM_RESOURCE_TEST="${TMP_ROOT}/pwm-resource-esp32"
compile_pwm_resource_backend_test "${ESP32_PWM_RESOURCE_TEST}" platform/esp32/main \
    -DMCUJS_PLATFORM_ESP32=1 -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 \
    -I"${ROOT}/tests/native_stubs/esp32" \
    -I"${ROOT}/platform/esp32/main/bindings" \
    "${ROOT}/platform/esp32/main/bindings/pin_policy.c" \
    "${ROOT}/platform/esp32/main/bindings/pwm.c"
"${ESP32_PWM_RESOURCE_TEST}"

ESP32_ADC_TEST="${TMP_ROOT}/adc-esp32"
compile_adc_backend_test "${ESP32_ADC_TEST}" platform/esp32/main \
    -DMCUJS_PLATFORM_ESP32=1 -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 \
    -I"${ROOT}/tests/native_stubs/esp32" \
    -I"${ROOT}/platform/esp32/main/bindings"
assert_global_text_symbol "${ESP32_ADC_TEST}" "js_create_adc_module"
"${ESP32_ADC_TEST}"
