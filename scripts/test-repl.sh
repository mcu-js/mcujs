#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="$(mktemp "${TMPDIR:-/tmp}/mcujs-repl-test.XXXXXX")"
trap 'rm -f "${BINARY}"' EXIT

cc -std=gnu17 -Wall -Wextra -Werror \
    -DMCUJS_BUILD_ID='"test"' \
    -I"${ROOT}/src" \
    -I"${ROOT}/src/usb" \
    -I"${ROOT}/src/filesystem" \
    -I"${ROOT}/host" \
    -I"${ROOT}/board" \
    -I"${ROOT}/platform/esp32/main" \
    "${ROOT}/tests/repl_input_test.c" \
    "${ROOT}/src/repl.c" \
    -o "${BINARY}"

"${BINARY}"
