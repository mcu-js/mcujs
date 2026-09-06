#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ESP_ROOT="$(realpath -e "${ROOT}/platform/esp32")"
IMAGE="${MCUJS_ESP32_DOCKER_IMAGE:-mcujs-esp32-builder:v5.3.2-amd64}"
BUILD_ABS="${ESP_ROOT}/build-docker"
PLATFORM=linux/amd64
VERSION="$(tr -d '[:space:]' < "${ROOT}/version.txt")"
UF2_NAME="mcujs-${VERSION}-seeed_xiao_esp32s3.uf2"
CAPABILITY_NAME="mcujs-${VERSION}-seeed_xiao_esp32s3.capabilities.json"
GIT_SHA="$(git -C "${ROOT}" rev-parse --short HEAD)"
if [[ ! "${GIT_SHA}" =~ ^[0-9a-f]{7,40}$ ]]; then
    printf 'Could not determine a valid source Git SHA for Docker build identity.\n' >&2
    exit 1
fi

usage() {
    cat <<'EOF'
Usage: platform/esp32/docker-build.sh [--prepare-image [--docker-network VALUE]]

Builds the ESP32-S3 application and application-only UF2 using the pinned AMD64
ESP-IDF image. The runtime container is networkless, mounts source read-only,
and writes final artifacts only to platform/esp32/build-docker/.

Environment:
  MCUJS_ESP32_DOCKER_IMAGE  Override the local image tag or immutable ID.
  MCUJS_DOCKER_NETWORK     Preparation network only; ignored during compilation.

--prepare-image (alias --rebuild-image) prepares the image only and may download
dependencies. Ordinary builds require an existing local image and never acquire it.
EOF
}

PREPARE_IMAGE=0
DOCKER_NETWORK="${MCUJS_DOCKER_NETWORK:-}"
NETWORK_OPTION=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h) usage; exit 0 ;;
        --prepare-image|--rebuild-image) PREPARE_IMAGE=1; shift ;;
        --docker-network)
            [[ $# -ge 2 ]] || { printf '%s\n' '--docker-network requires a value' >&2; exit 1; }
            DOCKER_NETWORK="$2"; NETWORK_OPTION=1; shift 2 ;;
        *) usage >&2; exit 1 ;;
    esac
done
if [[ "${NETWORK_OPTION}" -eq 1 && "${PREPARE_IMAGE}" -eq 0 ]]; then
    printf '%s\n' '--docker-network applies only to --prepare-image; firmware builds always use network none' >&2
    exit 1
fi
command -v docker >/dev/null 2>&1 || { printf 'Docker is not installed or not in PATH\n' >&2; exit 1; }
if [[ "${PREPARE_IMAGE}" -eq 1 ]]; then
    build_args=(--platform "${PLATFORM}" -f "${ROOT}/platform/esp32/Dockerfile" -t "${IMAGE}")
    [[ -z "${DOCKER_NETWORK}" ]] || build_args+=(--network "${DOCKER_NETWORK}")
    docker build "${build_args[@]}" "${ROOT}"
    exit 0
fi
if ! IMAGE_ID="$(docker image inspect --format '{{.Id}}' "${IMAGE}" 2>/dev/null)"; then
    printf 'Local builder %s is unavailable. Prepare it explicitly: ./build.sh seeed_xiao_esp32s3 --prepare-image\n' "${IMAGE}" >&2
    exit 1
fi
if [[ ! "${IMAGE_ID}" =~ ^sha256:[0-9a-f]{64}$ ]]; then
    printf 'Docker did not return a valid immutable image ID for %s\n' "${IMAGE}" >&2
    exit 1
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
    "${CAPABILITY_NAME}"
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
for artifact_path in "${BUILD_ABS}"/mcujs-*-seeed_xiao_esp32s3.capabilities.json; do
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

docker run --rm --init --pull never \
    --cap-drop ALL --security-opt no-new-privileges \
    --entrypoint /bin/bash \
    --platform "${PLATFORM}" \
    --network none \
    -u "$(id -u):$(id -g)" \
    -e HOME=/tmp/mcujs-home \
    -e MCUJS_BUILD_GIT_SHA="${GIT_SHA}" \
    -v "${ROOT}:/source:ro" \
    -v "${BUILD_ABS}:/output" \
    "${IMAGE_ID}" /source/platform/esp32/docker-entrypoint.sh build

for artifact in "${ARTIFACTS[@]}"; do
    artifact_path="${BUILD_ABS}/${artifact}"
    if [[ ! -f "${artifact_path}" || ! -s "${artifact_path}" || -L "${artifact_path}" ]]; then
        printf 'Docker build did not produce a regular artifact: %s\n' "${artifact_path}" >&2
        exit 1
    fi
done
cleanup_armed=0
trap - EXIT
