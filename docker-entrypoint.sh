#!/bin/bash
set -euo pipefail

source /workspace/scripts/lib/boards.sh

BOARDS="${1:-all}"
BUILD_TYPE="${2:-Release}"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

build_board() {
    local board=$1
    log_info "Building mcujs for board: ${board}"

    # Create build directory
    mkdir -p "/workspace/cmake-build-${board}"
    cd "/workspace/cmake-build-${board}"

    cmake_args=(
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
        -DBOARD="${board}"
        -DPICO_SDK_PATH="${PICO_SDK_PATH}"
    )

    if [[ -d "/opt/picotool/picotool" ]]; then
        cmake_args+=(-Dpicotool_DIR=/opt/picotool/picotool)
    fi

    # Configure
    cmake "${cmake_args[@]}" ..

    # Build
    make -j"$(nproc)"

    # Copy UF2 to build directory
    cp "/workspace/cmake-build-${board}"/*.uf2 /workspace/build/ 2>/dev/null || true

    log_info "Build complete for ${board}"
}

# Ensure build output directory exists
mkdir -p /workspace/build

# Build requested boards
if [[ "${BOARDS}" == "all" ]]; then
    for board in "${MCUJS_BOARDS[@]}"; do
        build_board "${board}"
    done
else
    if ! mcujs_is_board "${BOARDS}"; then
        log_error "Unknown board: ${BOARDS}"
        log_info "Available boards:"
        mcujs_print_board_list
        log_info "  all"
        exit 1
    fi
    build_board "${BOARDS}"
fi

log_info "All builds complete!"
ls -la /workspace/build/*.uf2 2>/dev/null || log_warn "No UF2 files found in build directory"
