#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IDF_PATH="${IDF_PATH:-${HOME}/toolchains/esp-idf-5.3.2}"
JERRYSCRIPT_PATH="${JERRYSCRIPT_PATH:-${HOME}/toolchains/jerryscript-3.0.0}"
IDF_COMMIT="9d7f2d69f50d1288526d4f1027108e314e8c879f"
JERRYSCRIPT_COMMIT="50200152feb724a74a5f64e44d7885151537cfad"

verify_checkout() {
    local path="$1" expected="$2" label="$3" actual dirty
    actual="$(git -C "${path}" rev-parse HEAD 2>/dev/null || true)"
    [[ "${actual}" == "${expected}" ]] || {
        printf '%s must be at reviewed commit %s (found %s)\n' \
            "${label}" "${expected}" "${actual:-not a Git checkout}" >&2
        exit 1
    }
    dirty="$(git -C "${path}" status --porcelain --untracked-files=normal)"
    [[ -z "${dirty}" ]] || {
        printf '%s checkout is dirty; refusing a non-reproducible firmware build\n' "${label}" >&2
        exit 1
    }
}

[[ -f "${IDF_PATH}/export.sh" ]] || {
    printf 'ESP-IDF not found at %s\n' "${IDF_PATH}" >&2
    exit 1
}
[[ -f "${JERRYSCRIPT_PATH}/tools/build.py" ]] || {
    printf 'JerryScript not found at %s\n' "${JERRYSCRIPT_PATH}" >&2
    exit 1
}

verify_checkout "${IDF_PATH}" "${IDF_COMMIT}" "ESP-IDF"
verify_checkout "${JERRYSCRIPT_PATH}" "${JERRYSCRIPT_COMMIT}" "JerryScript"

for action in "$@"; do
    if [[ "${action}" == "flash" && "${MCUJS_ALLOW_DESTRUCTIVE_FULL_FLASH:-0}" != "1" ]]; then
        printf '%s\n' \
            'Refusing full flash: it replaces the TinyUF2-aware bootloader and partition metadata.' \
            'Use app-flash to preserve TinyUF2, or explicitly set MCUJS_ALLOW_DESTRUCTIVE_FULL_FLASH=1.' >&2
        exit 1
    fi
done

export IDF_PATH JERRYSCRIPT_PATH
# ESP-IDF's export script sets the pinned Python environment and tool paths.
# shellcheck disable=SC1090
source "${IDF_PATH}/export.sh" >/dev/null

exec idf.py -C "${SCRIPT_DIR}" -B "${SCRIPT_DIR}/build" "$@"
