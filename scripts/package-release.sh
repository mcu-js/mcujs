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
  --force   Replace this version's existing package and top-level dist assets
  --help    Show this help text

The script expects each RP UF2 under build/, plus the XIAO ESP32-S3 binary, UF2,
and capability manifest under platform/esp32/build-docker/. It verifies the
XIAO build ID, exact UF2 payload, and generated capability manifest before
staging a deterministic manifest, SHA256SUMS, individual UF2/capability assets,
and a source-date-stamped tarball under dist/. The complete staged release is
published with same-filesystem renames only after every input and output passes.

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
TARBALL_NAME="${RELEASE_NAME}.tar.gz"
TAR_BIN="$(find_gnu_tar)"

node "${ROOT_DIR}/scripts/generate-runtime-registry.js" --check
node "${ROOT_DIR}/scripts/verify-esp32-release-artifacts.js"

asset_names=()
capability_names=()
asset_sources=()
capability_sources=()

# Preflight the complete input set before creating or replacing any release output.
for board in "${MCUJS_RELEASE_BOARDS[@]}"; do
    asset="mcujs-${VERSION}-${board}.uf2"
    capability_asset="mcujs-${VERSION}-${board}.capabilities.json"
    if [[ "${board}" == "seeed_xiao_esp32s3" ]]; then
        src="${ROOT_DIR}/platform/esp32/build-docker/${asset}"
        capability_src="${ROOT_DIR}/platform/esp32/build-docker/${capability_asset}"
    else
        src="${ROOT_DIR}/build/${asset}"
        capability_src="${ROOT_DIR}/runtime/manifests/${board}.json"
    fi
    generated_capability="${ROOT_DIR}/runtime/manifests/${board}.json"
    [[ -f "${src}" && ! -L "${src}" ]] || fail "Missing or non-regular firmware artifact: ${src}"
    [[ -f "${capability_src}" && ! -L "${capability_src}" ]] \
        || fail "Missing or non-regular capability manifest: ${capability_src}"
    [[ -f "${generated_capability}" && ! -L "${generated_capability}" ]] \
        || fail "Missing or non-regular generated capability manifest: ${generated_capability}"
    cmp -s "${capability_src}" "${generated_capability}" \
        || fail "Capability manifest does not match generated registry: ${capability_src}"
    mcujs_board_chip "${board}" >/dev/null \
        || fail "Missing chip metadata for release board: ${board}"
    mcujs_board_flash "${board}" >/dev/null \
        || fail "Missing flash metadata for release board: ${board}"
    asset_names+=("${asset}")
    capability_names+=("${capability_asset}")
    asset_sources+=("${src}")
    capability_sources+=("${capability_src}")
done

# Preflight the complete output set before creating temporary directories or
# replacing any authoritative release asset. A partial previous publication is
# still authoritative and requires an explicit --force replacement.
top_level_names=()
for index in "${!asset_names[@]}"; do
    top_level_names+=("${asset_names[${index}]}" "${capability_names[${index}]}")
done
top_level_names+=(
    "${RELEASE_NAME}-manifest.txt"
    "${RELEASE_NAME}-SHA256SUMS.txt"
    "${TARBALL_NAME}"
)
target_outputs=("${PACKAGE_DIR}")
for name in "${top_level_names[@]}"; do
    target_outputs+=("${DIST_DIR}/${name}")
done
if [[ "${FORCE}" -ne 1 ]]; then
    for target in "${target_outputs[@]}"; do
        if [[ -e "${target}" || -L "${target}" ]]; then
            fail "${target} already exists. Pass --force to replace it."
        fi
    done
fi

mkdir -p "${DIST_DIR}"
STAGE_ROOT="$(mktemp -d "${DIST_DIR}/.mcujs-package-stage.XXXXXX")"
if ! BACKUP_ROOT="$(mktemp -d "${DIST_DIR}/.mcujs-package-backup.XXXXXX")"; then
    rm -rf -- "${STAGE_ROOT}"
    fail "Unable to create release backup directory under ${DIST_DIR}"
fi
PACKAGE_STAGE="${STAGE_ROOT}/${RELEASE_NAME}"
TOP_LEVEL_STAGE="${STAGE_ROOT}/top-level"
MANIFEST="${PACKAGE_STAGE}/RELEASE_MANIFEST.txt"
SUMS="${PACKAGE_STAGE}/SHA256SUMS.txt"
PUBLISHING=0
COMMITTED=0
published_targets=()
backed_up_targets=()

cleanup() {
    status=$?
    trap - EXIT
    trap '' HUP INT TERM
    set +e
    if [[ "${PUBLISHING}" -eq 1 && "${COMMITTED}" -ne 1 ]]; then
        for target in "${published_targets[@]}"; do
            rm -rf -- "${target}"
        done
        for target in "${backed_up_targets[@]}"; do
            backup="${BACKUP_ROOT}/$(basename "${target}")"
            if [[ -e "${backup}" || -L "${backup}" ]]; then
                mv -- "${backup}" "${target}"
            fi
        done
    fi
    rm -rf -- "${STAGE_ROOT}" "${BACKUP_ROOT}"
    exit "${status}"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

mkdir -p "${PACKAGE_STAGE}" "${TOP_LEVEL_STAGE}"

{
    printf 'mcujs release manifest\n'
    printf 'version=%s\n' "${VERSION}"
    printf 'git_sha=%s\n' "${GIT_SHA}"
    printf 'source_date_epoch=%s\n' "${SOURCE_DATE_EPOCH}"
    printf 'boards=%s\n' "${#MCUJS_RELEASE_BOARDS[@]}"
    printf '\n'
    printf 'assets:\n'
} > "${MANIFEST}"
: > "${SUMS}"

for index in "${!MCUJS_RELEASE_BOARDS[@]}"; do
    board="${MCUJS_RELEASE_BOARDS[${index}]}"
    asset="${asset_names[${index}]}"
    capability_asset="${capability_names[${index}]}"
    src="${asset_sources[${index}]}"
    capability_src="${capability_sources[${index}]}"
    cp "${src}" "${PACKAGE_STAGE}/${asset}"
    cp "${capability_src}" "${PACKAGE_STAGE}/${capability_asset}"
    touch -d "@${SOURCE_DATE_EPOCH}" "${PACKAGE_STAGE}/${asset}" 2>/dev/null || true
    touch -d "@${SOURCE_DATE_EPOCH}" "${PACKAGE_STAGE}/${capability_asset}" 2>/dev/null || true
    printf '  - board=%s file=%s capabilities=%s chip=%s flash=%s\n' \
        "${board}" \
        "${asset}" \
        "${capability_asset}" \
        "$(mcujs_board_chip "${board}")" \
        "$(mcujs_board_flash "${board}")" >> "${MANIFEST}"
    printf '%s  %s\n' "$(checksum_file "${PACKAGE_STAGE}/${asset}")" "${asset}" >> "${SUMS}"
    printf '%s  %s\n' "$(checksum_file "${PACKAGE_STAGE}/${capability_asset}")" "${capability_asset}" >> "${SUMS}"
    cp "${PACKAGE_STAGE}/${asset}" "${TOP_LEVEL_STAGE}/${asset}"
    cp "${PACKAGE_STAGE}/${capability_asset}" "${TOP_LEVEL_STAGE}/${capability_asset}"
    cmp -s "${PACKAGE_STAGE}/${asset}" "${TOP_LEVEL_STAGE}/${asset}" \
        || fail "Staged top-level firmware copy differs: ${asset}"
    cmp -s "${PACKAGE_STAGE}/${capability_asset}" "${TOP_LEVEL_STAGE}/${capability_asset}" \
        || fail "Staged top-level capability copy differs: ${capability_asset}"
done

touch -d "@${SOURCE_DATE_EPOCH}" "${MANIFEST}" "${SUMS}" 2>/dev/null || true
cp "${MANIFEST}" "${TOP_LEVEL_STAGE}/${RELEASE_NAME}-manifest.txt"
cp "${SUMS}" "${TOP_LEVEL_STAGE}/${RELEASE_NAME}-SHA256SUMS.txt"
cmp -s "${MANIFEST}" "${TOP_LEVEL_STAGE}/${RELEASE_NAME}-manifest.txt" \
    || fail "Staged top-level release manifest differs"
cmp -s "${SUMS}" "${TOP_LEVEL_STAGE}/${RELEASE_NAME}-SHA256SUMS.txt" \
    || fail "Staged top-level checksum manifest differs"

"${TAR_BIN}" \
    --sort=name \
    --mtime="@${SOURCE_DATE_EPOCH}" \
    --owner=0 \
    --group=0 \
    --numeric-owner \
    -czf "${TOP_LEVEL_STAGE}/${TARBALL_NAME}" \
    -C "${STAGE_ROOT}" \
    "${RELEASE_NAME}"
"${TAR_BIN}" -tzf "${TOP_LEVEL_STAGE}/${TARBALL_NAME}" >/dev/null

# All failure-prone checks are complete. Publish each output with an atomic
# same-filesystem rename; the EXIT trap restores every previous target if a
# rename itself fails before the full set is committed.
staged_outputs=("${PACKAGE_STAGE}")
for name in "${top_level_names[@]}"; do
    staged="${TOP_LEVEL_STAGE}/${name}"
    [[ -f "${staged}" && ! -L "${staged}" ]] \
        || fail "Missing or non-regular staged release output: ${staged}"
    staged_outputs+=("${staged}")
done

PUBLISHING=1
for index in "${!staged_outputs[@]}"; do
    staged="${staged_outputs[${index}]}"
    target="${target_outputs[${index}]}"
    if [[ -e "${target}" || -L "${target}" ]]; then
        backup="${BACKUP_ROOT}/$(basename "${target}")"
        backed_up_targets+=("${target}")
        mv -- "${target}" "${backup}"
    fi
    published_targets+=("${target}")
    mv -- "${staged}" "${target}"
done
COMMITTED=1

printf 'Packaged release assets in %s\n' "${PACKAGE_DIR}"
printf 'Tarball: %s\n' "${DIST_DIR}/${TARBALL_NAME}"
