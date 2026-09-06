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
  --no-docker       Build RP boards with the local Pico toolchain
  --rebuild-image   Rebuild the RP Docker builder image before compiling
  --skip-build      Package existing RP and XIAO build artifacts
  --help            Show this help text

Default flow:
  1. Verify source release metadata.
  2. Build every RP board from a clean CMake directory.
  3. Freshly build the XIAO ESP32-S3 with its pinned Docker lane.
  4. Verify and package UF2 assets, checksums, and manifests under dist/.

--no-docker applies only to RP boards. The mandatory XIAO ESP32-S3 release
artifact always uses its pinned Docker lane. --skip-build skips both lanes;
package verification still rejects stale or relabelled XIAO firmware.
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
        "${ROOT_DIR}/build.sh" all --prepare-image
    fi
    "${ROOT_DIR}/build.sh" "${build_args[@]}"
    "${ROOT_DIR}/platform/esp32/docker-build.sh"
fi

"${ROOT_DIR}/scripts/package-release.sh" --force

printf '\nRelease package is ready in dist/.\n'
