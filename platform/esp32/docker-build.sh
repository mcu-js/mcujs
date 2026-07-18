#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ESP_ROOT="$(realpath -e "${ROOT}/platform/esp32")"
IMAGE="${MCUJS_ESP32_DOCKER_IMAGE:-mcujs-esp32-builder:v5.3.2-amd64}"
BUILD_ABS="${ESP_ROOT}/build-docker"
PLATFORM=linux/amd64

usage() {
    cat <<'EOF'
Usage: platform/esp32/docker-build.sh

Builds the ESP32-S3 application and application-only UF2 using the pinned AMD64
ESP-IDF image. The runtime container is networkless, mounts source read-only,
and writes final artifacts only to platform/esp32/build-docker/.

Environment:
  MCUJS_ESP32_DOCKER_IMAGE  Override the local image tag.
EOF
}

if [[ $# -gt 0 ]]; then
    case "$1" in
        --help|-h) usage; exit 0 ;;
        *) usage >&2; exit 1 ;;
    esac
fi

if [[ -L "${BUILD_ABS}" ]]; then
    printf 'Refusing symlinked Docker output directory: %s\n' "${BUILD_ABS}" >&2
    exit 1
fi
mkdir -p "${BUILD_ABS}"
if [[ "$(realpath -e "${BUILD_ABS}")" != "${BUILD_ABS}" ]]; then
    printf 'Docker output directory escaped its fixed path: %s\n' "${BUILD_ABS}" >&2
    exit 1
fi
for artifact in mcujs-esp32s3.bin mcujs-esp32s3.uf2 \
                mcujs-esp32s3.elf mcujs-esp32s3.map; do
    if [[ -L "${BUILD_ABS}/${artifact}" ]]; then
        printf 'Refusing symlinked Docker output artifact: %s\n' "${BUILD_ABS}/${artifact}" >&2
        exit 1
    fi
done

docker build --platform "${PLATFORM}" \
    -f "${ROOT}/platform/esp32/Dockerfile" \
    -t "${IMAGE}" "${ROOT}"
docker run --rm --init \
    --platform "${PLATFORM}" \
    --network none \
    -u "$(id -u):$(id -g)" \
    -e HOME=/tmp/mcujs-home \
    -v "${ROOT}:/source:ro" \
    -v "${BUILD_ABS}:/output" \
    "${IMAGE}" build
