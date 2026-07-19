#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ESP_DIR="${ROOT}/platform/esp32"
HOST_BUILD="${ESP_DIR}/build-host-repro"
DOCKER_BUILD="${ESP_DIR}/build-docker"
VERSION="$(tr -d '[:space:]' < "${ROOT}/version.txt")"
UF2_NAME="mcujs-${VERSION}-seeed_xiao_esp32s3.uf2"
CAPABILITY_NAME="mcujs-${VERSION}-seeed_xiao_esp32s3.capabilities.json"

MCUJS_ESP32_BUILD_DIR="${HOST_BUILD}" "${ESP_DIR}/build.sh" fullclean
MCUJS_ESP32_BUILD_DIR="${HOST_BUILD}" "${ESP_DIR}/build.sh" build
python3 "${ESP_DIR}/make-uf2.py" \
    --input "${HOST_BUILD}/mcujs-esp32s3.bin" \
    --output "${HOST_BUILD}/${UF2_NAME}"
install -m 0644 \
    "${ROOT}/runtime/manifests/seeed_xiao_esp32s3.json" \
    "${HOST_BUILD}/${CAPABILITY_NAME}"

"${ESP_DIR}/docker-build.sh"

for artifact in mcujs-esp32s3.elf mcujs-esp32s3.bin "${UF2_NAME}" "${CAPABILITY_NAME}"; do
    if ! cmp -s "${HOST_BUILD}/${artifact}" "${DOCKER_BUILD}/${artifact}"; then
        printf 'Host and Docker artifacts differ: %s\n' "${artifact}" >&2
        sha256sum "${HOST_BUILD}/${artifact}" "${DOCKER_BUILD}/${artifact}" >&2
        exit 1
    fi
    printf 'byte-identical: %s\n' "${artifact}"
done

sha256sum \
    "${HOST_BUILD}/mcujs-esp32s3.bin" \
    "${HOST_BUILD}/${UF2_NAME}" \
    "${HOST_BUILD}/${CAPABILITY_NAME}"
