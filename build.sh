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
    echo "  all                                  Build for all supported boards (default)"
    echo ""
    echo "Options:"
    echo "  --clean          Clean build directories before building"
    echo "  --debug          Build with debug symbols"
    echo "  --no-docker      Build without Docker (requires local toolchain)"
    echo "  --docker-network VALUE"
    echo "                   Docker network mode for build/run (default: host on Linux)"
    echo "  --rebuild-image  Rebuild the Docker builder image before building"
    echo "  --help           Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./build.sh pico"
    echo "  ./build.sh all --clean"
    echo "  ./build.sh pico2 --debug"
}

BOARD="all"
BUILD_TYPE="Release"
CLEAN=0
USE_DOCKER=1
REBUILD_IMAGE=0
DOCKER_NETWORK="${MCUJS_DOCKER_NETWORK:-}"

if [[ -z "${DOCKER_NETWORK}" && "$(uname -s)" == "Linux" ]]; then
    DOCKER_NETWORK="host"
fi

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
            shift 2
            ;;
        --rebuild-image)
            REBUILD_IMAGE=1
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            if mcujs_is_board "$1"; then
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

VERSION="$(tr -d '[:space:]' < version.txt)"
log_info "mcujs version: ${VERSION}"
log_info "Target board: ${BOARD}"
log_info "Build type: ${BUILD_TYPE}"

# Create build output directory
mkdir -p build

# Always delete old UF2 files to ensure we're testing fresh builds
rm -f build/*.uf2

if [[ "${CLEAN}" -eq 1 ]]; then
    log_info "Cleaning build directories..."
    rm -rf cmake-build-*
    rm -f build/*.uf2
fi

if [[ "${USE_DOCKER}" -eq 1 ]]; then
    # Check if Docker is available
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed or not in PATH"
        log_info "Install Docker or use --no-docker for local build"
        exit 1
    fi

    IMAGE_NAME="mcujs-builder"

    # Build Docker image if it doesn't exist, or when explicitly requested.
    if [[ "${REBUILD_IMAGE}" -eq 1 ]] || ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
        log_info "Building Docker image..."
        docker_build_args=(-t "${IMAGE_NAME}")
        if [[ -n "${DOCKER_NETWORK}" ]]; then
            docker_build_args+=(--network "${DOCKER_NETWORK}")
        fi
        docker build "${docker_build_args[@]}" .
    else
        log_info "Using Docker image ${IMAGE_NAME}; pass --rebuild-image after dependency changes"
    fi

    log_info "Running build in Docker container..."
    docker_run_args=(--rm)
    if [[ -n "${DOCKER_NETWORK}" ]]; then
        docker_run_args+=(--network "${DOCKER_NETWORK}")
    fi
    docker run "${docker_run_args[@]}" \
        -v "${SCRIPT_DIR}:/workspace" \
        -u "$(id -u):$(id -g)" \
        "${IMAGE_NAME}" \
        "${BOARD}" "${BUILD_TYPE}"
else
    # Local build
    if [[ -z "${PICO_SDK_PATH:-}" ]]; then
        log_error "PICO_SDK_PATH environment variable is not set"
        exit 1
    fi

    build_board() {
        local board=$1
        log_info "Building for board: ${board}"

        mkdir -p "cmake-build-${board}"
        cd "cmake-build-${board}"

        cmake \
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

log_info "Build complete!"
echo ""
echo "Output files:"
ls -la build/*.uf2 2>/dev/null || log_warn "No UF2 files found"
