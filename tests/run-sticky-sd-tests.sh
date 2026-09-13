#!/usr/bin/env bash
# Native boundary tests only: no SDK build, downloads or device access.
set -euo pipefail
ulimit -c 0
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/mcujs-sticky-sd.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
cc -std=gnu17 -Wall -Wextra -Werror ${SD_TEST_CFLAGS:-} \
    -I"$ROOT/tests/sticky_sd_stubs" -I"$ROOT/tests/canvas_epaper_stubs" \
    -I"$ROOT/src/filesystem" -I"$ROOT/platform/esp32/main" \
    "$ROOT/tests/sticky_sd_test.c" "$ROOT/platform/esp32/main/sticky_sd.c" \
    -o "$TMP/sticky-sd"
for scenario in ok absent unsupported read-failure foreign-bus gpio-failure host-failure device-failure drive-failure vfs-failure mount-failure sector-size; do
    "$TMP/sticky-sd" "$scenario"
done
cc -std=gnu17 -Wall -Wextra -Werror ${SD_TEST_CFLAGS:-} \
    -I"$ROOT/tests/canvas_epaper_stubs" -I"$ROOT/host/bindings" -I"$ROOT/src/filesystem" \
    "$ROOT/tests/canvas_sticky_test.c" -o "$TMP/sticky-display"
"$TMP/sticky-display"
