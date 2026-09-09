#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${MCUJS_ESP32_BUILD_DIR:-${SCRIPT_DIR}/build}"
if [[ "${BUILD_DIR}" != /* ]]; then
    BUILD_DIR="${SCRIPT_DIR}/${BUILD_DIR}"
fi
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

if [[ -n "${SERIAL_TOOL_EXTRA_ARGS:-}" ]]; then
    printf '%s\n' \
        'Refusing SERIAL_TOOL_EXTRA_ARGS: app-flash permits no arguments outside the validated response file.' >&2
    exit 1
fi

for action in "$@"; do
    case "${action}" in
      @*)
        printf '%s\n' \
            "Refusing ESP-IDF argument file '${action}': recursive arguments bypass project flash validation." >&2
        exit 1
        ;;
      --force|--extra-args|--extra-args=*)
        printf '%s\n' \
            "Refusing ESP-IDF flash option '${action}': app-flash permits no unvalidated esptool arguments." >&2
        exit 1
        ;;
      app-flash)
        destructive=0
        ;;
      *flash*|*erase*)
        destructive=1
        ;;
      *) destructive=0 ;;
    esac
    if [[ "${destructive}" == "1" && "${MCUJS_ALLOW_DESTRUCTIVE_FULL_FLASH:-0}" != "1" ]]; then
        printf '%s\n' \
            "Refusing destructive ESP-IDF target '${action}': it can replace or erase protected flash regions." \
            'Use an application-only UF2, or explicitly set MCUJS_ALLOW_DESTRUCTIVE_FULL_FLASH=1 for recovery engineering.' >&2
        exit 1
    fi
done

export IDF_PATH JERRYSCRIPT_PATH
export IDF_COMPONENT_STRICT_CHECKSUM=1
# ESP-IDF's export script sets the pinned Python environment and tool paths.
# shellcheck disable=SC1090
source "${IDF_PATH}/export.sh" >/dev/null

# Export ESP-IDF's otherwise implicit host default so the component project
# include can normalize the compiler-tool path in reproducible debug metadata.
IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-${HOME}/.espressif}"
[[ -d "${IDF_TOOLS_PATH}" ]] || {
    printf 'ESP-IDF tools directory is missing: %s\n' "${IDF_TOOLS_PATH}" >&2
    exit 1
}
export IDF_TOOLS_PATH

validate_runtime_config() {
    local config="${BUILD_DIR}/sdkconfig" required line found
    [[ -f "${config}" ]] || {
        printf 'Generated sdkconfig is missing: %s\n' "${config}" >&2
        return 1
    }

    for required in \
        'CONFIG_ESP_CONSOLE_UART_DEFAULT=y' \
        '# CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG is not set' \
        'CONFIG_ESP_CONSOLE_SECONDARY_NONE=y' \
        'CONFIG_TINYUSB_DESC_USE_ESPRESSIF_VID=y' \
        'CONFIG_TINYUSB_DESC_USE_DEFAULT_PID=y' \
        'CONFIG_TINYUSB_MSC_ENABLED=y' \
        'CONFIG_TINYUSB_MSC_BUFSIZE=4096' \
        'CONFIG_TINYUSB_CDC_ENABLED=y' \
        'CONFIG_TINYUSB_CDC_COUNT=1' \
        'CONFIG_TINYUSB_NO_DEFAULT_TASK=y' \
        'CONFIG_ESP_TASK_WDT_EN=y' \
        'CONFIG_ESP_TASK_WDT_INIT=y' \
        'CONFIG_ESP_TASK_WDT_PANIC=y' \
        'CONFIG_ESP_TASK_WDT_TIMEOUT_S=10' \
        'CONFIG_APP_REPRODUCIBLE_BUILD=y' \
        'CONFIG_COMPILER_HIDE_PATHS_MACROS=y' \
        'CONFIG_FATFS_LFN_HEAP=y' \
        'CONFIG_FATFS_MAX_LFN=255' \
        'CONFIG_FATFS_API_ENCODING_UTF_8=y' \
        'CONFIG_FATFS_USE_LABEL=y'; do
        if [[ "${MCUJS_BOARD:-}" == "seeed_reterminal_sticky" && "${required}" == CONFIG_TINYUSB_* ]]; then
            continue
        fi
        found=0
        while IFS= read -r line; do
            if [[ "${line}" == "${required}" ]]; then
                found=1
                break
            fi
        done < "${config}"
        [[ "${found}" == "1" ]] || {
            printf 'Generated sdkconfig violates MCU.js runtime policy: missing %s\n' \
                "${required}" >&2
            return 1
        }
    done
    if [[ "${MCUJS_BOARD:-}" == "seeed_reterminal_sticky" ]]; then
        python - "${config}" <<'PYCONFIG'
import sys
from pathlib import Path
lines = set(Path(sys.argv[1]).read_text().splitlines())
required = {'CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y', 'CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240', 'CONFIG_SPIRAM=y',
 'CONFIG_SPIRAM_MODE_OCT=y', 'CONFIG_SPIRAM_BOOT_INIT=y', 'CONFIG_SPIRAM_USE_MALLOC=y',
 '# CONFIG_TINYUSB_CDC_ENABLED is not set', '# CONFIG_TINYUSB_MSC_ENABLED is not set'}
if not required <= lines:
    raise SystemExit(f'Sticky config missing: {sorted(required - lines)}')
PYCONFIG
    fi
}

validate_app_flash_metadata() {
    local build_dir="${BUILD_DIR}"
    python - "${build_dir}" <<'PY'
import json
import shlex
import sys
from pathlib import Path

build_dir = Path(sys.argv[1]).resolve()
args_path = build_dir / "app-flash_args"
json_path = build_dir / "flasher_args.json"
expected_image = "mcujs-esp32s3.bin"
expected_offset = 0x10000
import os
sticky = os.environ.get("MCUJS_BOARD") == "seeed_reterminal_sticky"
if sticky:
    expected_offset = 0x690000
    ota_end = 0xc90000
else:
    ota_end = 0x400000 if os.environ.get("MCUJS_BOARD") == "waveshare_esp32s3_epaper_1.54_v2" else 0x410000

if not args_path.is_file() or not json_path.is_file():
    raise SystemExit("Generated app-flash metadata is missing")

tokens = shlex.split(args_path.read_text())
expected_tokens = [
    "--flash_mode", "dio",
    "--flash_freq", "80m",
    "--flash_size", "32MB" if sticky else "8MB",
    hex(expected_offset), expected_image,
]
if tokens != expected_tokens:
    raise SystemExit(
        f"Unsafe app-flash arguments: expected {expected_tokens}, found {tokens}"
    )

metadata = json.loads(json_path.read_text())
app = metadata.get("app", {})
if int(str(app.get("offset", "-1")), 0) != expected_offset or app.get("file") != expected_image:
    raise SystemExit(f"Unsafe generated app metadata: {app}")

image = (build_dir / expected_image).resolve()
if image.parent != build_dir or not image.is_file():
    raise SystemExit(f"Generated application image is missing: {image}")
image_size = image.stat().st_size
if image_size <= 0 or expected_offset + image_size > ota_end:
    raise SystemExit(
        f"Application image range 0x{expected_offset:x}..0x{expected_offset + image_size:x} "
        f"exceeds ota_0 end 0x{ota_end:x}"
    )

print(
    f"Validated app-only flash metadata: 0x{expected_offset:x} {expected_image} "
    f"({image_size} bytes)"
)
PY
}

idf_args=("$@")
app_flash=0
for action in "${idf_args[@]}"; do
    [[ "${action}" == "app-flash" ]] && app_flash=1
done

if [[ "${app_flash}" == "1" ]]; then
    preflight_args=()
    for action in "${idf_args[@]}"; do
        if [[ "${action}" == "app-flash" ]]; then
            preflight_args+=(build)
        else
            preflight_args+=("${action}")
        fi
    done
    idf.py -C "${SCRIPT_DIR}" -B "${BUILD_DIR}" "${preflight_args[@]}"
    validate_runtime_config
    validate_app_flash_metadata
    exec idf.py -C "${SCRIPT_DIR}" -B "${BUILD_DIR}" "${idf_args[@]}"
fi

idf.py -C "${SCRIPT_DIR}" -B "${BUILD_DIR}" "${idf_args[@]}"
if [[ -f "${BUILD_DIR}/sdkconfig" ]]; then
    validate_runtime_config
fi
if [[ -f "${BUILD_DIR}/app-flash_args" ]]; then
    validate_app_flash_metadata
fi
