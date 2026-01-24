#!/bin/bash
set -e

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
    mkdir -p /workspace/cmake-build-${board}
    cd /workspace/cmake-build-${board}
    
    # Configure
    cmake \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DBOARD=${board} \
        -DPICO_SDK_PATH=${PICO_SDK_PATH} \
        ..
    
    # Build
    make -j$(nproc)
    
    # Copy UF2 to build directory
    cp /workspace/cmake-build-${board}/*.uf2 /workspace/build/ 2>/dev/null || true
    
    log_info "Build complete for ${board}"
}

# Ensure build output directory exists
mkdir -p /workspace/build

# Build requested boards
case "${BOARDS}" in
    "all")
        build_board "pico"
        build_board "pico2"
        build_board "pico2_w"
        build_board "waveshare_rp2040_zero"
        build_board "waveshare_rp2040_touch_lcd_1.28"
        ;;
    "pico"|"pico2"|"pico2_w"|"waveshare_rp2040_zero"|"waveshare_rp2040_touch_lcd_1.28")
        build_board "${BOARDS}"
        ;;
    *)
        log_error "Unknown board: ${BOARDS}"
        log_info "Available boards: pico, pico2, pico2_w, waveshare_rp2040_zero, waveshare_rp2040_touch_lcd_1.28, all"
        exit 1
        ;;
esac

log_info "All builds complete!"
ls -la /workspace/build/*.uf2 2>/dev/null || log_warn "No UF2 files found in build directory"
