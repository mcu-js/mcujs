#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/scripts/lib/boards.sh"

FORCE=0

show_help() {
    cat <<'EOF'
mcujs release packager

Usage:
  scripts/package-release.sh [options]

Options:
  --force   Replace this version's existing dist package directory
  --help    Show this help text

The script expects build/mcujs-<version>-<board>.uf2 to exist for every
release board. It writes a deterministic manifest, SHA256SUMS, individual UF2
assets, and a source-date-stamped tarball under dist/.

Deterministic tarball creation requires GNU tar. On macOS, install GNU tar so
the `gtar` command is available, or run this script on Linux.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --force)
            FORCE=1
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

fail() {
    printf '[FAIL] %s\n' "$1" >&2
    exit 1
}

checksum_file() {
    local file="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "${file}" | awk '{print $1}'
        return
    fi
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "${file}" | awk '{print $1}'
        return
    fi
    fail 'sha256sum or shasum is required'
}

find_gnu_tar() {
    local candidate
    for candidate in "${TAR:-}" gtar tar; do
        [[ -n "${candidate}" ]] || continue
        if command -v "${candidate}" >/dev/null 2>&1 \
            && "${candidate}" --version 2>/dev/null | grep -qi 'GNU tar'; then
            printf '%s\n' "${candidate}"
            return
        fi
    done
    fail 'GNU tar is required for deterministic archives. Install gnu-tar or run this script on Linux.'
}

VERSION="$(tr -d '[:space:]' < "${ROOT_DIR}/version.txt")"
GIT_SHA="$(git -C "${ROOT_DIR}" rev-parse --short HEAD 2>/dev/null || printf 'unknown')"
GIT_EPOCH="$(git -C "${ROOT_DIR}" log -1 --format=%ct 2>/dev/null || date +%s)"
SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-${GIT_EPOCH}}"
RELEASE_NAME="mcujs-${VERSION}-${GIT_SHA}"
DIST_DIR="${ROOT_DIR}/dist"
PACKAGE_DIR="${DIST_DIR}/${RELEASE_NAME}"
MANIFEST="${PACKAGE_DIR}/RELEASE_MANIFEST.txt"
SUMS="${PACKAGE_DIR}/SHA256SUMS.txt"
TAR_BIN="$(find_gnu_tar)"

if [[ -e "${PACKAGE_DIR}" && "${FORCE}" -ne 1 ]]; then
    fail "${PACKAGE_DIR} already exists. Pass --force to replace it."
fi

rm -rf "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}"

{
    printf 'mcujs release manifest\n'
    printf 'version=%s\n' "${VERSION}"
    printf 'git_sha=%s\n' "${GIT_SHA}"
    printf 'source_date_epoch=%s\n' "${SOURCE_DATE_EPOCH}"
    printf 'boards=%s\n' "${#MCUJS_BOARDS[@]}"
    printf '\n'
    printf 'assets:\n'
} > "${MANIFEST}"
: > "${SUMS}"

for board in "${MCUJS_BOARDS[@]}"; do
    src="${ROOT_DIR}/build/mcujs-${VERSION}-${board}.uf2"
    asset="mcujs-${VERSION}-${board}.uf2"
    [[ -f "${src}" ]] || fail "Missing firmware artifact: ${src}"
    cp "${src}" "${PACKAGE_DIR}/${asset}"
    touch -d "@${SOURCE_DATE_EPOCH}" "${PACKAGE_DIR}/${asset}" 2>/dev/null || true
    printf '  - board=%s file=%s chip=%s flash=%s\n' \
        "${board}" \
        "${asset}" \
        "$(mcujs_board_chip "${board}")" \
        "$(mcujs_board_flash "${board}")" >> "${MANIFEST}"
    printf '%s  %s\n' "$(checksum_file "${PACKAGE_DIR}/${asset}")" "${asset}" >> "${SUMS}"
    cp "${PACKAGE_DIR}/${asset}" "${DIST_DIR}/${asset}"
done

touch -d "@${SOURCE_DATE_EPOCH}" "${MANIFEST}" "${SUMS}" 2>/dev/null || true
cp "${MANIFEST}" "${DIST_DIR}/${RELEASE_NAME}-manifest.txt"
cp "${SUMS}" "${DIST_DIR}/${RELEASE_NAME}-SHA256SUMS.txt"

TARBALL="${DIST_DIR}/${RELEASE_NAME}.tar.gz"
rm -f "${TARBALL}"
"${TAR_BIN}" \
    --sort=name \
    --mtime="@${SOURCE_DATE_EPOCH}" \
    --owner=0 \
    --group=0 \
    --numeric-owner \
    -czf "${TARBALL}" \
    -C "${DIST_DIR}" \
    "${RELEASE_NAME}"

printf 'Packaged release assets in %s\n' "${PACKAGE_DIR}"
printf 'Tarball: %s\n' "${TARBALL}"
