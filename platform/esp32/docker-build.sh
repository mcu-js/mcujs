#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ESP_ROOT="$(realpath -e "${ROOT}/platform/esp32")"
IMAGE="${MCUJS_ESP32_DOCKER_IMAGE:-mcujs-esp32-builder:v5.3.2-amd64}"
BUILD_ABS="${ESP_ROOT}/build-docker"
PLATFORM=linux/amd64
VERSION="$(tr -d '[:space:]' < "${ROOT}/version.txt")"
UF2_NAME="mcujs-${VERSION}-seeed_xiao_esp32s3.uf2"

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
ARTIFACTS=(
    mcujs-esp32s3.bin
    "${UF2_NAME}"
    mcujs-esp32s3.elf
    mcujs-esp32s3.map
)
declare -A CLEANUP_SEEN=()
CLEANUP_PATHS=()
add_cleanup_path() {
    local path="$1"
    if [[ -z "${CLEANUP_SEEN[${path}]:-}" ]]; then
        CLEANUP_SEEN["${path}"]=1
        CLEANUP_PATHS+=("${path}")
    fi
}
for artifact in "${ARTIFACTS[@]}" mcujs-esp32s3.uf2; do
    add_cleanup_path "${BUILD_ABS}/${artifact}"
done
shopt -s nullglob
for artifact_path in "${BUILD_ABS}"/mcujs-*-seeed_xiao_esp32s3.uf2; do
    add_cleanup_path "${artifact_path}"
done
shopt -u nullglob

for artifact_path in "${CLEANUP_PATHS[@]}"; do
    if [[ -L "${artifact_path}" ]]; then
        printf 'Refusing symlinked Docker output artifact: %s\n' "${artifact_path}" >&2
        exit 1
    fi
    if [[ -e "${artifact_path}" && ! -f "${artifact_path}" ]]; then
        printf 'Refusing non-regular Docker output artifact: %s\n' "${artifact_path}" >&2
        exit 1
    fi
done

cleanup_outputs() {
    local path
    local failed=0
    for path in "${CLEANUP_PATHS[@]}"; do
        if [[ -e "${path}" || -L "${path}" ]]; then
            if ! unlink "${path}"; then
                printf 'Could not remove incomplete Docker artifact: %s\n' "${path}" >&2
                failed=1
            fi
        fi
    done
    return "${failed}"
}

cleanup_armed=1
cleanup_on_exit() {
    local status=$?
    trap - EXIT
    if [[ "${cleanup_armed}" -eq 1 ]] && ! cleanup_outputs; then
        status=1
    fi
    exit "${status}"
}
trap cleanup_on_exit EXIT
cleanup_outputs

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

for artifact in "${ARTIFACTS[@]}"; do
    artifact_path="${BUILD_ABS}/${artifact}"
    if [[ ! -f "${artifact_path}" || -L "${artifact_path}" ]]; then
        printf 'Docker build did not produce a regular artifact: %s\n' "${artifact_path}" >&2
        exit 1
    fi
done
cleanup_armed=0
trap - EXIT
