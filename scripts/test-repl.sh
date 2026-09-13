#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT}/scripts/lib/boards.sh"
BINARY="$(mktemp "${TMPDIR:-/tmp}/mcujs-repl-test.XXXXXX")"
trap 'rm -f "${BINARY}"' EXIT

compile_repl_test() {
    local output="$1"
    shift
    cc -std=gnu17 -Wall -Wextra -Werror \
        -DMCUJS_BUILD_ID='"test"' \
        "$@" \
        -I"${ROOT}/src" \
        -I"${ROOT}/src/usb" \
        -I"${ROOT}/src/filesystem" \
        -I"${ROOT}/host" \
        -I"${ROOT}/board" \
        -I"${ROOT}/platform/esp32/main" \
        "${ROOT}/tests/repl_input_test.c" \
        "${ROOT}/src/repl.c" \
        "${ROOT}/host/runtime_registry.c" \
        -o "${output}"
}

# Experimental runtime profiles need the same help/editor contract as release boards.
REPL_BOARDS=("${MCUJS_RELEASE_BOARDS[@]}" waveshare_esp32s3_epaper_1.54_v2 seeed_reterminal_sticky)
for board in "${REPL_BOARDS[@]}"; do
    define="${board^^}"
    define="${define//./_}"
    flags=("-DMCUJS_BOARD_${define}=1" "-I${ROOT}/board/${board}")
    case "${board}" in
        seeed_xiao_esp32s3|waveshare_esp32s3_epaper_1.54_v2|seeed_reterminal_sticky)
            flags+=(-DMCUJS_PLATFORM_ESP32=1) ;;
    esac
    compile_repl_test "${BINARY}" "${flags[@]}"
    printf '%s: ' "${board}"
    "${BINARY}"
done
