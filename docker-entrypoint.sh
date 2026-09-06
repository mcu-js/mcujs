#!/bin/bash
set -euo pipefail

# Compile an ephemeral copy; the caller's checkout is never writable.
export HOME="${HOME:-/tmp/mcujs-home}"
ROOT=/tmp/mcujs-workspace
mkdir -p "${HOME}" "${ROOT}" /output
tar -C /source --exclude='./.git' --exclude='./build' \
    --exclude='./cmake-build-*' --exclude='./node_modules' \
    --exclude='./docs/node_modules' --exclude='./docs/build' \
    --exclude='./platform/esp32/build' --exclude='./platform/esp32/build-*' \
    --exclude='./platform/esp32/managed_components' \
    -cf - . | tar -C "${ROOT}" -xf -
source "${ROOT}/scripts/lib/boards.sh"
VERSION="$(tr -d '[:space:]' < "${ROOT}/version.txt")"

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
    mkdir -p "${ROOT}/cmake-build-${board}"
    cd "${ROOT}/cmake-build-${board}"

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

    # Publish only the completed artifacts, owned by the caller's numeric UID/GID.
    for ext in uf2 bin elf elf.map; do
        install -m 0644 "${ROOT}/cmake-build-${board}/mcujs-${VERSION}-${board}.${ext}" /output/
    done

    log_info "Build complete for ${board}"
}

# Ensure build output directory exists
mkdir -p "${ROOT}/build"

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
ls -la /output/*.uf2
