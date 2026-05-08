#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

NO_DOCKER=0
REBUILD_IMAGE=0
SKIP_BUILD=0

show_help() {
    cat <<'EOF'
mcujs deterministic release builder

Usage:
  scripts/release.sh [options]

Options:
  --no-docker       Build with the local Pico toolchain instead of Docker
  --rebuild-image   Rebuild the Docker builder image before compiling
  --skip-build      Package existing build/*.uf2 artifacts
  --help            Show this help text

Default flow:
  1. Verify source release metadata.
  2. Build every supported board from a clean CMake directory.
  3. Package UF2 assets, checksums, and a release manifest under dist/.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-docker)
            NO_DOCKER=1
            shift
            ;;
        --rebuild-image)
            REBUILD_IMAGE=1
            shift
            ;;
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$1" >&2
            show_help >&2
            exit 1
            ;;
    esac
done

"${ROOT_DIR}/scripts/verify-release.sh"

if [[ "${SKIP_BUILD}" -eq 0 ]]; then
    build_args=(all --clean)
    if [[ "${NO_DOCKER}" -eq 1 ]]; then
        build_args+=(--no-docker)
    fi
    if [[ "${REBUILD_IMAGE}" -eq 1 ]]; then
        build_args+=(--rebuild-image)
    fi
    "${ROOT_DIR}/build.sh" "${build_args[@]}"
fi

"${ROOT_DIR}/scripts/package-release.sh" --force

printf '\nRelease package is ready in dist/.\n'
