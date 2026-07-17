#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    printf '[FAIL] %s\n' "$1" >&2
    exit 1
}

pass() {
    printf '[ OK ] %s\n' "$1"
}

require_file() {
    [[ -f "$1" ]] || fail "Missing required file: $1"
}

PLATFORM_FILE="${ROOT_DIR}/platform/rp2/platform.cmake"
require_file "${PLATFORM_FILE}"
require_file "${ROOT_DIR}/platform/README.md"
require_file "${ROOT_DIR}/platform/rp2/pico_sdk_import.cmake"
require_file "${ROOT_DIR}/platform/rp2/usb/tusb_config.h"

required_variables=(
    MCUJS_PLATFORM_MAIN_SOURCE
    MCUJS_PLATFORM_JERRY_PORT_SOURCE
    MCUJS_PLATFORM_USB_SOURCES
    MCUJS_PLATFORM_STORAGE_SOURCES
    MCUJS_PLATFORM_BOARD_SOURCE
    MCUJS_PLATFORM_BOOT_SOURCE
    MCUJS_PLATFORM_HARDWARE_BINDING_SOURCES
    MCUJS_PLATFORM_USB_BINDING_SOURCES
    MCUJS_PLATFORM_OPTIONAL_BINDING_SOURCES
    MCUJS_PLATFORM_INCLUDE_DIRS
)

required_hooks=(
    mcujs_platform_sdk_init
    mcujs_platform_configure_jerry
    mcujs_platform_configure_host
    mcujs_platform_configure_core
    mcujs_platform_configure_bindings
    mcujs_platform_configure_executable
)

for name in "${required_variables[@]}"; do
    grep -Fq "set(${name}" "${PLATFORM_FILE}" ||
        fail "RP2 platform does not declare ${name}"
done

for name in "${required_hooks[@]}"; do
    grep -Eq "^(macro|function)\(${name}([ )])" "${PLATFORM_FILE}" ||
        fail "RP2 platform does not implement ${name}"
done

pass 'RP2 backend declares the complete build contract'

legacy_sources=(
    src/main.c
    src/board.c
    src/boot.c
    src/filesystem/diskio.c
    src/filesystem/flash_ops.c
    src/usb/tusb_config.h
    host/jerry_port.c
    host/bindings/gpio.c
    host/bindings/timers.c
    host/bindings/pwm.c
    host/bindings/i2c.c
    host/bindings/spi.c
    host/bindings/adc.c
    host/bindings/neopixel.c
    host/bindings/board.c
    host/bindings/keyboard.c
    host/bindings/mouse.c
    host/bindings/dvi.c
    pico_sdk_import.cmake
)

for path in "${legacy_sources[@]}"; do
    [[ ! -e "${ROOT_DIR}/${path}" ]] ||
        fail "Platform implementation leaked into legacy path: ${path}"
done

pass 'legacy RP2 implementation paths are empty'

mapfile -t sdk_leaks < <(
    grep -R -n -E '#include[[:space:]]*[<"](pico|hardware)/' \
        --include='*.c' --include='*.h' \
        "${ROOT_DIR}/src" "${ROOT_DIR}/host" "${ROOT_DIR}/board" || true
)

if (( ${#sdk_leaks[@]} > 0 )); then
    printf '%s\n' "${sdk_leaks[@]}" >&2
    fail 'Pico SDK headers are included by shared runtime sources'
fi

pass 'shared runtime sources contain no Pico SDK includes'

mapfile -t cmake_leaks < <(
    grep -n -E 'pico_sdk_|pico_stdlib|pico_unique_id|hardware_|tinyusb_' \
        "${ROOT_DIR}/CMakeLists.txt" \
        "${ROOT_DIR}/src/CMakeLists.txt" \
        "${ROOT_DIR}/host/CMakeLists.txt" \
        "${ROOT_DIR}/host/bindings/CMakeLists.txt" \
        "${ROOT_DIR}/javascript/CMakeLists.txt" || true
)

if (( ${#cmake_leaks[@]} > 0 )); then
    printf '%s\n' "${cmake_leaks[@]}" >&2
    fail 'Pico SDK targets are referenced by shared CMake files'
fi

pass 'shared CMake files contain no Pico SDK targets'

if ! grep -Fq 'Unsupported MCUJS_PLATFORM' "${ROOT_DIR}/CMakeLists.txt"; then
    fail 'top-level CMake does not reject unsupported platforms'
fi

pass 'unknown platforms fail closed'
printf '\nPlatform boundary checks passed.\n'
