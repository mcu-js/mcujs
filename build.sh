#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"
source "${SCRIPT_DIR}/scripts/lib/boards.sh"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

job_count() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
        return
    fi
    getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1\n'
}

show_help() {
    echo -e "${CYAN}mcujs build script${NC}"
    echo ""
    echo "Usage: ./build.sh [board] [options]"
    echo ""
    echo "Boards:"
    mcujs_print_board_list
    echo "  seeed_xiao_esp32s3                    Seeed Studio XIAO ESP32-S3"
    echo "  all                                  Build all RP boards (default; excludes ESP32)"
    echo ""
    echo "Options:"
    echo "  --clean          Clean local RP build directories (Docker builds are always fresh)"
    echo "  --debug          Build RP firmware with debug symbols"
    echo "  --no-docker      Build RP firmware with the local toolchain"
    echo "  --prepare-image  Prepare the selected SDK image only; may download dependencies"
    echo "  --docker-network VALUE"
    echo "                   Preparation network only (default: Docker default)"
    echo "  --rebuild-image  Alias for --prepare-image; does not compile firmware"
    echo "  --help           Show this help message"
    echo ""
    echo "Images: MCUJS_RP_DOCKER_IMAGE (default: mcujs-builder)"
    echo "        MCUJS_ESP32_DOCKER_IMAGE (default: mcujs-esp32-builder:v5.3.2-amd64)"
    echo "Normal Docker builds require a local image and always use network none."
    echo "MCUJS_DOCKER_NETWORK affects preparation only."
    echo ""
    echo "Examples:"
    echo "  ./build.sh pico"
    echo "  ./build.sh seeed_xiao_esp32s3 --prepare-image"
    echo "  ./build.sh seeed_xiao_esp32s3"
    echo "  ./build.sh all --clean"
    echo "  ./build.sh pico2 --debug"
}

BOARD="all"
BUILD_TYPE="Release"
CLEAN=0
USE_DOCKER=1
PREPARE_IMAGE=0
DOCKER_NETWORK="${MCUJS_DOCKER_NETWORK:-}"
NETWORK_OPTION=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        all)
            BOARD="$1"
            shift
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --no-docker)
            USE_DOCKER=0
            shift
            ;;
        --docker-network)
            if [[ $# -lt 2 ]]; then
                log_error "--docker-network requires a value"
                exit 1
            fi
            DOCKER_NETWORK="$2"
            NETWORK_OPTION=1
            shift 2
            ;;
        --prepare-image|--rebuild-image)
            PREPARE_IMAGE=1
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            if mcujs_is_board "$1" || [[ "$1" == "seeed_xiao_esp32s3" ]]; then
                BOARD="$1"
                shift
            else
                log_error "Unknown option or board: $1"
                show_help
                exit 1
            fi
            ;;
    esac
done

if [[ "${NETWORK_OPTION}" -eq 1 && "${PREPARE_IMAGE}" -eq 0 ]]; then
    log_error "--docker-network applies only to --prepare-image; firmware builds always use network none"
    exit 1
fi
if [[ "${USE_DOCKER}" -eq 0 && "${PREPARE_IMAGE}" -eq 1 ]]; then
    log_error "--prepare-image cannot be combined with --no-docker"
    exit 1
fi
if [[ "${BOARD}" == "seeed_xiao_esp32s3" ]]; then
    if [[ "${USE_DOCKER}" -eq 0 || "${BUILD_TYPE}" != "Release" ]]; then
        log_error "XIAO supports the Docker release configuration here; use platform/esp32/build.sh for local engineering builds"
        exit 1
    fi
    esp_args=()
    if [[ "${PREPARE_IMAGE}" -eq 1 ]]; then
        esp_args+=(--prepare-image)
        if [[ -n "${DOCKER_NETWORK}" ]]; then
            esp_args+=(--docker-network "${DOCKER_NETWORK}")
        fi
    fi
    exec "${SCRIPT_DIR}/platform/esp32/docker-build.sh" "${esp_args[@]}"
fi

# Inspect locally before touching outputs. Never acquire an image during a build.
if [[ "${USE_DOCKER}" -eq 1 ]]; then
    command -v docker >/dev/null 2>&1 || { log_error "Docker is not installed or not in PATH"; exit 1; }
    IMAGE_NAME="${MCUJS_RP_DOCKER_IMAGE:-mcujs-builder}"
    if [[ "${PREPARE_IMAGE}" -eq 1 ]]; then
        docker_build_args=(-t "${IMAGE_NAME}")
        if [[ -n "${DOCKER_NETWORK}" ]]; then
            docker_build_args+=(--network "${DOCKER_NETWORK}")
        fi
        docker build "${docker_build_args[@]}" "${SCRIPT_DIR}"
        exit 0
    fi
fi

VERSION="$(tr -d '[:space:]' < version.txt)"
if ! GIT_SHA="$(git -C "${SCRIPT_DIR}" rev-parse --short HEAD 2>/dev/null)"; then
    log_error "Could not determine source Git SHA for build identity"
    exit 1
fi
if [[ ! "${GIT_SHA}" =~ ^[0-9a-f]{7,40}$ ]]; then
    log_error "Source Git SHA for build identity is invalid: ${GIT_SHA}"
    exit 1
fi
if ! command -v strings >/dev/null 2>&1; then
    log_error "strings is required to verify ELF build identity"
    exit 1
fi
if [[ "${USE_DOCKER}" -eq 1 ]]; then
    if ! IMAGE_ID="$(docker image inspect --format '{{.Id}}' "${IMAGE_NAME}" 2>/dev/null)"; then
        log_error "Local builder ${IMAGE_NAME} is unavailable. Prepare it explicitly: ./build.sh ${BOARD} --prepare-image"
        exit 1
    fi
    if [[ ! "${IMAGE_ID}" =~ ^sha256:[0-9a-f]{64}$ ]]; then
        log_error "Docker did not return a valid immutable image ID for ${IMAGE_NAME}"
        exit 1
    fi
fi
log_info "mcujs version: ${VERSION}"
log_info "Build identity: ${VERSION}+${GIT_SHA}"
log_info "Target board: ${BOARD}"
log_info "Build type: ${BUILD_TYPE}"

# Refuse redirected outputs before deleting or mounting anything.
if [[ -L build || ( -e build && ! -d build ) ]]; then
    log_error "Refusing non-directory or symlinked build output"
    exit 1
fi
mkdir -p build
OUTPUT_DIR="${SCRIPT_DIR}/build"

if [[ "${CLEAN}" -eq 1 && "${USE_DOCKER}" -eq 0 ]]; then
    log_info "Cleaning build directories..."
    rm -rf cmake-build-*
    rm -f build/*.uf2
fi

if [[ "${USE_DOCKER}" -eq 1 ]]; then
    boards=("${BOARD}")
    [[ "${BOARD}" == all ]] && boards=("${MCUJS_BOARDS[@]}")
    outputs=()
    shopt -s nullglob
    for board in "${boards[@]}"; do
        for ext in uf2 bin elf elf.map; do
            outputs+=("${OUTPUT_DIR}/mcujs-${VERSION}-${board}.${ext}")
            outputs+=("${OUTPUT_DIR}"/mcujs-*-${board}.${ext})
        done
    done
    shopt -u nullglob
    for output in "${outputs[@]}"; do
        if [[ -L "${output}" || ( -e "${output}" && ! -f "${output}" ) ]]; then
            log_error "Refusing non-regular output: ${output}"
            exit 1
        fi
    done
    # Failure must not leave partial or stale artifacts for the selected boards.
    trap 'rm -f -- "${outputs[@]}"' EXIT
    rm -f -- "${outputs[@]}"
    log_info "Running networkless build with ${IMAGE_ID}"
    docker run --rm --init --pull never --network none \
        --cap-drop ALL --security-opt no-new-privileges \
        --entrypoint /bin/bash --workdir /tmp \
        -v "${SCRIPT_DIR}:/source:ro" \
        -v "${OUTPUT_DIR}:/output" \
        -u "$(id -u):$(id -g)" \
        -e HOME=/tmp/mcujs-home \
        -e MCUJS_BUILD_GIT_SHA="${GIT_SHA}" \
        "${IMAGE_ID}" /source/docker-entrypoint.sh \
        "${BOARD}" "${BUILD_TYPE}"
    for board in "${boards[@]}"; do
        for ext in uf2 bin elf elf.map; do
            output="${OUTPUT_DIR}/mcujs-${VERSION}-${board}.${ext}"
            if [[ ! -s "${output}" || -L "${output}" || ! -f "${output}" ]]; then
                log_error "Build did not produce a nonempty regular artifact: ${output}"
                exit 1
            fi
        done
    done
else
    # Local build
    rm -f build/*.uf2
    if [[ -z "${PICO_SDK_PATH:-}" ]]; then
        log_error "PICO_SDK_PATH environment variable is not set"
        exit 1
    fi

    build_board() {
        local board=$1
        log_info "Building for board: ${board}"

        mkdir -p "cmake-build-${board}"
        cd "cmake-build-${board}"

        MCUJS_BUILD_GIT_SHA="${GIT_SHA}" cmake \
            -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
            -DBOARD="${board}" \
            ..

        make -j"$(job_count)"
        cd ..
    }

    if [[ "${BOARD}" == "all" ]]; then
        for board in "${MCUJS_BOARDS[@]}"; do
            build_board "${board}"
        done
    else
        build_board "${BOARD}"
    fi
fi

verify_board_build_identity() {
    local board="$1"
    local elf="cmake-build-${board}/mcujs-${VERSION}-${board}.elf"
    if [[ "${USE_DOCKER}" -eq 1 ]]; then
        elf="build/mcujs-${VERSION}-${board}.elf"
    fi
    local expected="${VERSION}+${GIT_SHA}"

    if [[ ! -f "${elf}" ]]; then
        log_error "Build did not produce expected ELF: ${elf}"
        exit 1
    fi
    if ! LC_ALL=C strings "${elf}" | grep -Fx "${expected}" >/dev/null; then
        log_error "ELF build identity mismatch for ${board}; expected ${expected}"
        exit 1
    fi
    log_info "Verified ELF build identity for ${board}: ${expected}"
}

if [[ "${BOARD}" == "all" ]]; then
    for board in "${MCUJS_BOARDS[@]}"; do
        verify_board_build_identity "${board}"
    done
else
    verify_board_build_identity "${BOARD}"
fi

trap - EXIT
log_info "Build complete!"
echo ""
echo "Output files:"
ls -la build/*.uf2 2>/dev/null || log_warn "No UF2 files found"
