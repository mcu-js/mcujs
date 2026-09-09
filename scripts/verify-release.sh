#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/scripts/lib/boards.sh"

ALLOW_DIRTY=0
RUN_DOCS=0

show_help() {
    cat <<'EOF'
mcujs release source verifier

Usage:
  scripts/verify-release.sh [options]

Options:
  --allow-dirty   Do not fail when tracked or untracked files are changed
  --docs          Also run docs typecheck and production build
  --help          Show this help text

Checks:
  - release version syntax
  - board registry matches board/ configuration directories
  - every board has required CMake/header files
  - public docs mention every release board ID
  - contributor and release docs share the development/release policy
  - MCU.js 0.2 portable API schema and docs remain consistent
  - generated runtime registry, manifests, native maps, and Jerry bindings agree
  - RP Docker builds preserve source identity from normal and linked worktrees
  - shell entrypoints parse cleanly
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --allow-dirty)
            ALLOW_DIRTY=1
            shift
            ;;
        --docs)
            RUN_DOCS=1
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

pass() {
    printf '[ OK ] %s\n' "$1"
}

require_file() {
    [[ -f "$1" ]] || fail "Missing required file: $1"
}

check_clean_tree() {
    if [[ "${ALLOW_DIRTY}" -eq 1 ]]; then
        pass 'git tree cleanliness skipped by --allow-dirty'
        return
    fi

    if ! git -C "${ROOT_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        pass 'not a git worktree; cleanliness skipped'
        return
    fi

    local status
    status="$(git -C "${ROOT_DIR}" status --short)"
    [[ -z "${status}" ]] || fail "Git worktree is dirty. Commit, stash, or pass --allow-dirty."
    pass 'git worktree is clean'
}

check_version() {
    require_file "${ROOT_DIR}/version.txt"
    local version
    version="$(tr -d '[:space:]' < "${ROOT_DIR}/version.txt")"
    [[ "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+([-.][0-9A-Za-z]+)*$ ]] || fail "version.txt must be SemVer-like, found '${version}'"
    pass "version.txt is ${version}"
}

check_board_registry() {
    local board
    local actual_file
    local actual_boards=()
    local expected_file
    local actual_joined
    local expected_joined

    for board in "${MCUJS_BOARDS[@]}"; do
        require_file "${ROOT_DIR}/board/${board}/board_config.h"
        require_file "${ROOT_DIR}/board/${board}/board_config.cmake"
    done

    while IFS= read -r actual_file; do
        actual_boards+=("$(basename "$(dirname "${actual_file}")")")
    done < <(find "${ROOT_DIR}/board" -mindepth 2 -maxdepth 2 -name board_config.cmake -print | LC_ALL=C sort)

    expected_file="$(mktemp)"
    actual_file="$(mktemp)"

    # Profiles now include ESP memory policy and experimental targets, not
    # just the RP release lane. Compare against the authoritative full registry.
    node -e 'Object.keys(require(process.argv[1]).boardDescriptors).sort().forEach(id => console.log(id))' \
        "${ROOT_DIR}/runtime/board-registry.js" > "${expected_file}"
    printf '%s\n' "${actual_boards[@]}" | LC_ALL=C sort > "${actual_file}"

    if ! diff -u "${expected_file}" "${actual_file}" >/dev/null; then
        expected_joined="$(tr '\n' ' ' < "${expected_file}")"
        actual_joined="$(tr '\n' ' ' < "${actual_file}")"
        rm -f "${expected_file}" "${actual_file}"
        fail "Board registry drift. Expected: ${expected_joined} Actual: ${actual_joined}"
    fi

    rm -f "${expected_file}" "${actual_file}"
    pass 'board profiles match the complete runtime registry'
}

check_docs_board_coverage() {
    local doc="${ROOT_DIR}/docs/docs/hardware-boards.md"
    local board
    require_file "${doc}"
    for board in "${MCUJS_RELEASE_BOARDS[@]}"; do
        if ! grep -Fq "\`${board}\`" "${doc}"; then
            fail "hardware board docs do not mention ${board}"
        fi
    done
    pass 'hardware board docs cover every release board ID'
}

check_development_policy() {
    local doc
    local phrase
    local docs=(
        "${ROOT_DIR}/CONTRIBUTING.md"
        "${ROOT_DIR}/RELEASING.md"
        "${ROOT_DIR}/docs/docs/contributing.md"
        "${ROOT_DIR}/docs/docs/releasing.md"
    )
    local required_phrases=(
        '`main` is the protected release branch (not `master`) and is release-only.'
        'Feature branches start from the latest approved `development` tip.'
        'Each implementation requires independent review before integration.'
        'Release candidates are immutable.'
        'Physical QA gates the `development` to `main` pull request.'
        'The pull request must precede the merge.'
        'Release tags and publishing happen only after the merge.'
    )

    require_file "${ROOT_DIR}/docs/docs/development/mcujs-0.2-portable-api.md"

    for doc in "${docs[@]}"; do
        require_file "${doc}"
        for phrase in "${required_phrases[@]}"; do
            grep -Fq "${phrase}" "${doc}" || fail "Development/release policy missing from ${doc}: ${phrase}"
        done
    done

    pass 'development and release policy is consistent across contributor docs'
}

check_shell_syntax() {
    local script
    local scripts=(
        "${ROOT_DIR}/build.sh"
        "${ROOT_DIR}/docker-entrypoint.sh"
        "${ROOT_DIR}/scripts/boards.sh"
        "${ROOT_DIR}/scripts/configure-github-domain-verification.sh"
        "${ROOT_DIR}/scripts/configure-pages-dns.sh"
        "${ROOT_DIR}/scripts/finalize-pages-cutover.sh"
        "${ROOT_DIR}/scripts/package-release.sh"
        "${ROOT_DIR}/scripts/release.sh"
        "${ROOT_DIR}/scripts/test-repl.sh"
        "${ROOT_DIR}/scripts/test-portable-conformance.sh"
        "${ROOT_DIR}/scripts/test-rp-build-identity.sh"
        "${ROOT_DIR}/scripts/test-runtime-bindings.sh"
        "${ROOT_DIR}/scripts/test-runtime-registry.sh"
        "${ROOT_DIR}/scripts/test-runtime-validation.sh"
        "${ROOT_DIR}/scripts/verify-platform-boundaries.sh"
        "${ROOT_DIR}/scripts/verify-release.sh"
        "${ROOT_DIR}/platform/esp32/build.sh"
        "${ROOT_DIR}/platform/esp32/docker-build.sh"
        "${ROOT_DIR}/platform/esp32/docker-entrypoint.sh"
        "${ROOT_DIR}/platform/esp32/verify-docker-repro.sh"
        "${ROOT_DIR}/scripts/lib/boards.sh"
    )

    for script in "${scripts[@]}"; do
        require_file "${script}"
        bash -n "${script}"
    done
    pass 'shell entrypoints parse cleanly'
}

check_repl_input() {
    "${ROOT_DIR}/scripts/test-repl.sh"
    pass 'REPL CRLF and tab-completion tests passed'
}

check_rp_build_identity() {
    "${ROOT_DIR}/scripts/test-rp-build-identity.sh"
    pass 'RP Docker build identity handoff tests passed'
    node --test "${ROOT_DIR}/tests/docker-build.test.js"
    pass 'unified Docker entrypoint recording-stub tests passed (not real builds)'
}

check_python_syntax() {
    local script
    local scripts=(
        "${ROOT_DIR}/platform/esp32/make-uf2.py"
        "${ROOT_DIR}/platform/esp32/verify-component-lock.py"
        "${ROOT_DIR}/platform/esp32/hardware-smoke.py"
        "${ROOT_DIR}/platform/esp32/boot-smoke.py"
    )

    for script in "${scripts[@]}"; do
        require_file "${script}"
        PYTHONPYCACHEPREFIX="${TMPDIR:-/tmp}/mcujs-pycache" python3 -m py_compile "${script}"
    done
    pass 'Python entrypoints parse cleanly'
}

check_api_schema() {
    node --test "${ROOT_DIR}/tests/portable-api-schema.test.js"
    pass 'MCU.js 0.2 portable API schema and docs are consistent'
}

check_runtime_registry() {
    node "${ROOT_DIR}/scripts/generate-runtime-registry.js" --check
    "${ROOT_DIR}/scripts/test-portable-conformance.sh"
    pass 'runtime registry, discovery, native maps, boundaries, validation, and errors are consistent'
}

check_release_artifacts() {
    node --test "${ROOT_DIR}/tests/release-artifacts.test.js" "${ROOT_DIR}/tests/release-board-registry.test.js"
    pass 'release orchestration and XIAO artifact freshness checks passed'
}

check_platform_boundaries() {
    "${ROOT_DIR}/scripts/verify-platform-boundaries.sh"
    pass 'platform boundary checks passed'
}

check_docs_build() {
    if [[ "${RUN_DOCS}" -eq 0 ]]; then
        pass 'docs build skipped; pass --docs to enable it'
        return
    fi

    npm --prefix "${ROOT_DIR}/docs" run typecheck
    npm --prefix "${ROOT_DIR}/docs" run build
    pass 'docs typecheck and production build passed'
}

check_clean_tree
check_version
check_board_registry
check_docs_board_coverage
check_development_policy
check_shell_syntax
check_repl_input
check_rp_build_identity
check_python_syntax
check_api_schema
check_runtime_registry
check_release_artifacts
check_platform_boundaries
check_docs_build

printf '\nRelease source checks passed.\n'
