#!/usr/bin/env bash
set -euo pipefail

SOURCE_ROOT=/source
ROOT=/tmp/mcujs-workspace
ESP_DIR="${ROOT}/platform/esp32"
BUILD_DIR=/tmp/mcujs-build
OUTPUT_DIR=/output
VERSION="$(tr -d '[:space:]' < "${SOURCE_ROOT}/version.txt")"
export MCUJS_BOARD="${MCUJS_BOARD:-seeed_xiao_esp32s3}"
UF2_NAME="mcujs-${VERSION}-${MCUJS_BOARD}.uf2"
CAPABILITY_NAME="mcujs-${VERSION}-${MCUJS_BOARD}.capabilities.json"

if [[ "${1:-build}" != "build" || $# -gt 1 ]]; then
    printf 'The ESP32 Docker lane supports only a non-flashing build action.\n' >&2
    exit 1
fi

export HOME="${HOME:-/tmp/mcujs-home}"
export IDF_PATH=/opt/esp/idf
export IDF_TOOLS_PATH=/opt/esp
export JERRYSCRIPT_PATH=/opt/jerryscript
export TINYUF2_PATH=/opt/tinyuf2
export MCUJS_ESP32_BUILD_DIR="${BUILD_DIR}"
mkdir -p "${HOME}" "${ROOT}" "${BUILD_DIR}" "${OUTPUT_DIR}"

# Build from an ephemeral snapshot. Source is mounted read-only and host build
# outputs/managed components are excluded.
tar -C "${SOURCE_ROOT}" \
    --exclude='./build' \
    --exclude='./cmake-build-*' \
    --exclude='./docs/build' \
    --exclude='./docs/node_modules' \
    --exclude='./platform/esp32/build' \
    --exclude='./platform/esp32/build-*' \
    --exclude='./platform/esp32/managed_components' \
    -cf - . | tar --no-same-owner -C "${ROOT}" -xf -
cp -R /opt/mcujs-managed-components "${ESP_DIR}/managed_components"

git config --global --add safe.directory "${ROOT}"
git config --global --add safe.directory "${IDF_PATH}"
git config --global --add safe.directory "${JERRYSCRIPT_PATH}"
git config --global --add safe.directory "${TINYUF2_PATH}"
git config --global --add safe.directory "${TINYUF2_PATH}/lib/uf2"

python3 "${ESP_DIR}/verify-component-lock.py" \
    --lock "${ESP_DIR}/dependencies.lock" \
    --components "${ESP_DIR}/managed_components"
"${ESP_DIR}/build.sh" build
if [[ "${MCUJS_BOARD}" == "waveshare_esp32s3_epaper_1.54_v2" || "${MCUJS_BOARD}" == "seeed_reterminal_sticky" ]]; then
 install -m 0644 "${BUILD_DIR}/mcujs-esp32s3.bin" "${BUILD_DIR}/mcujs-esp32s3.elf" "${BUILD_DIR}/mcujs-esp32s3.map" "${BUILD_DIR}/bootloader/bootloader.bin" "${BUILD_DIR}/partition_table/partition-table.bin" "${BUILD_DIR}/flasher_args.json" "${OUTPUT_DIR}/"
 install -m 0644 "${BUILD_DIR}/${CAPABILITY_NAME}" "${OUTPUT_DIR}/${CAPABILITY_NAME}"
 exit 0
fi
python3 "${ESP_DIR}/make-uf2.py" \
    --input "${BUILD_DIR}/mcujs-esp32s3.bin" \
    --output "${BUILD_DIR}/${UF2_NAME}"

install -m 0644 \
    "${BUILD_DIR}/mcujs-esp32s3.bin" \
    "${BUILD_DIR}/${UF2_NAME}" \
    "${BUILD_DIR}/mcujs-esp32s3.elf" \
    "${BUILD_DIR}/mcujs-esp32s3.map" \
    "${OUTPUT_DIR}/"
install -m 0644 \
    "${BUILD_DIR}/${CAPABILITY_NAME}" \
    "${OUTPUT_DIR}/${CAPABILITY_NAME}"

sha256sum \
    "${OUTPUT_DIR}/mcujs-esp32s3.bin" \
    "${OUTPUT_DIR}/${UF2_NAME}" \
    "${OUTPUT_DIR}/${CAPABILITY_NAME}"
