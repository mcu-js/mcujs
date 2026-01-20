#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

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

show_help() {
    echo -e "${CYAN}mcujs build script${NC}"
    echo ""
    echo "Usage: ./build.sh [board] [options]"
    echo ""
    echo "Boards:"
    echo "  pico                 Build for Raspberry Pi Pico (RP2040)"
    echo "  pico2                Build for Raspberry Pi Pico 2 (RP2350)"
    echo "  waveshare_rp2040_zero Build for Waveshare RP2040-Zero (RP2040)"
    echo "  all                  Build for all supported boards (default)"
    echo ""
    echo "Options:"
    echo "  --clean       Clean build directories before building"
    echo "  --debug       Build with debug symbols"
    echo "  --no-docker   Build without Docker (requires local toolchain)"
    echo "  --help        Show this help message"
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

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        pico|pico2|waveshare_rp2040_zero|all)
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
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

VERSION=$(cat version.txt | tr -d '[:space:]')
log_info "mcujs version: ${VERSION}"
log_info "Target board: ${BOARD}"
log_info "Build type: ${BUILD_TYPE}"

# Create build output directory
mkdir -p build

# Always delete old UF2 files to ensure we're testing fresh builds
rm -f build/*.uf2

if [[ $CLEAN -eq 1 ]]; then
    log_info "Cleaning build directories..."
    rm -rf cmake-build-*
    rm -rf build/*.uf2
fi

if [[ $USE_DOCKER -eq 1 ]]; then
    # Check if Docker is available
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed or not in PATH"
        log_info "Install Docker or use --no-docker for local build"
        exit 1
    fi
    
    IMAGE_NAME="mcujs-builder"
    
    # Build Docker image if it doesn't exist or Dockerfile changed
    if [[ "$(docker images -q ${IMAGE_NAME} 2> /dev/null)" == "" ]] || \
       [[ "Dockerfile" -nt "$(docker inspect -f '{{.Created}}' ${IMAGE_NAME} 2>/dev/null || echo '1970-01-01')" ]]; then
        log_info "Building Docker image..."
        docker build -t ${IMAGE_NAME} .
    fi
    
    log_info "Running build in Docker container..."
    docker run --rm \
        -v "${SCRIPT_DIR}:/workspace" \
        -u "$(id -u):$(id -g)" \
        ${IMAGE_NAME} \
        "${BOARD}" "${BUILD_TYPE}"
else
    # Local build
    if [[ -z "${PICO_SDK_PATH}" ]]; then
        log_error "PICO_SDK_PATH environment variable is not set"
        exit 1
    fi
    
    build_board() {
        local board=$1
        log_info "Building for board: ${board}"
        
        mkdir -p "cmake-build-${board}"
        cd "cmake-build-${board}"
        
        cmake \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
            -DBOARD=${board} \
            ..
        
        make -j$(nproc)
        cd ..
    }
    
    case "${BOARD}" in
        "all")
            build_board "pico"
            build_board "pico2"
            build_board "waveshare_rp2040_zero"
            ;;
        *)
            build_board "${BOARD}"
            ;;
    esac
fi

log_info "Build complete!"
echo ""
echo "Output files:"
ls -la build/*.uf2 2>/dev/null || log_warn "No UF2 files found"
