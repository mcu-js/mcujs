#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/mcujs-rp-build-identity.XXXXXX")"
REAL_GIT="$(command -v git)"
trap 'rm -rf "${TMP_ROOT}"' EXIT

fail() {
    printf '[FAIL] %s\n' "$1" >&2
    exit 1
}

make_fixture() {
    local destination="$1"
    mkdir -p "${destination}/scripts/lib"
    cp "${ROOT}/build.sh" "${destination}/build.sh"
    cp "${ROOT}/version.txt" "${destination}/version.txt"
    cp "${ROOT}/scripts/lib/boards.sh" "${destination}/scripts/lib/boards.sh"
}

FAKE_BIN="${TMP_ROOT}/bin"
DOCKER_LOG="${TMP_ROOT}/docker.log"
mkdir -p "${FAKE_BIN}"
cat > "${FAKE_BIN}/docker" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
if [[ "${1:-}" == "image" && "${2:-}" == "inspect" ]]; then
    exit 0
fi
if [[ "${1:-}" == "run" ]]; then
    printf '%s\n' "$@" > "${MCUJS_TEST_DOCKER_LOG}"
    source_root=''
    git_sha=''
    for argument in "$@"; do
        case "${argument}" in
            *:/workspace) source_root="${argument%:/workspace}" ;;
            MCUJS_BUILD_GIT_SHA=*) git_sha="${argument#MCUJS_BUILD_GIT_SHA=}" ;;
        esac
    done
    board_index=$(( $# - 1 ))
    board="${!board_index}"
    version="$(tr -d '[:space:]' < "${source_root}/version.txt")"
    mkdir -p "${source_root}/cmake-build-${board}"
    build_identity="${version}+${git_sha}"
    if [[ "${MCUJS_TEST_BAD_ELF:-0}" -eq 1 ]]; then
        build_identity="${version}+unknown"
    fi
    elf="${source_root}/cmake-build-${board}/mcujs-${version}-${board}.elf"
    {
        printf '%s\n' "${build_identity}"
        for ((padding_line = 0; padding_line < 10000; padding_line += 1)); do
            printf 'fixture-padding-%05d\n' "${padding_line}"
        done
    } > "${elf}"
    exit 0
fi
printf 'Unexpected fake Docker invocation: %s\n' "$*" >&2
exit 1
EOF
chmod +x "${FAKE_BIN}/docker"

assert_sha_handoff() {
    local checkout="$1"
    local expected_sha
    expected_sha="$(${REAL_GIT} -C "${checkout}" rev-parse --short HEAD)"
    : > "${DOCKER_LOG}"
    PATH="${FAKE_BIN}:${PATH}" MCUJS_TEST_DOCKER_LOG="${DOCKER_LOG}" \
        bash "${checkout}/build.sh" pico >/dev/null
    grep -Fxq "MCUJS_BUILD_GIT_SHA=${expected_sha}" "${DOCKER_LOG}" ||
        fail "RP Docker build did not receive exact source SHA ${expected_sha} from ${checkout}"
}

NORMAL="${TMP_ROOT}/normal"
LINKED="${TMP_ROOT}/linked"
make_fixture "${NORMAL}"
"${REAL_GIT}" -C "${NORMAL}" init -q
"${REAL_GIT}" -C "${NORMAL}" add .
"${REAL_GIT}" -C "${NORMAL}" -c user.name=Test -c user.email=test@example.invalid commit -qm fixture
"${REAL_GIT}" -C "${NORMAL}" worktree add -q -b linked-test "${LINKED}"

assert_sha_handoff "${NORMAL}"
assert_sha_handoff "${LINKED}"

if PATH="${FAKE_BIN}:${PATH}" MCUJS_TEST_DOCKER_LOG="${DOCKER_LOG}" MCUJS_TEST_BAD_ELF=1 \
    bash "${NORMAL}/build.sh" pico >/dev/null 2>&1; then
    fail 'RP Docker build accepted an ELF with the wrong build identity'
fi

NO_GIT="${TMP_ROOT}/no-git"
make_fixture "${NO_GIT}"
: > "${DOCKER_LOG}"
if PATH="${FAKE_BIN}:${PATH}" MCUJS_TEST_DOCKER_LOG="${DOCKER_LOG}" \
    bash "${NO_GIT}/build.sh" pico >/dev/null 2>&1; then
    fail 'RP Docker build accepted source without a Git identity'
fi
[[ ! -s "${DOCKER_LOG}" ]] || fail 'RP Docker build invoked Docker without a Git identity'

INVALID_BIN="${TMP_ROOT}/invalid-bin"
mkdir -p "${INVALID_BIN}"
cat > "${INVALID_BIN}/git" <<'EOF'
#!/usr/bin/env bash
printf 'not-a-sha\n'
EOF
chmod +x "${INVALID_BIN}/git"
: > "${DOCKER_LOG}"
if PATH="${INVALID_BIN}:${FAKE_BIN}:${PATH}" MCUJS_TEST_DOCKER_LOG="${DOCKER_LOG}" \
    bash "${NORMAL}/build.sh" pico >/dev/null 2>&1; then
    fail 'RP Docker build accepted a malformed Git identity'
fi
[[ ! -s "${DOCKER_LOG}" ]] || fail 'RP Docker build invoked Docker with a malformed Git identity'

printf 'RP Docker build identity tests passed\n'
