#!/usr/bin/env bash
# Run only inside the authorized pinned SDK lane; no downloads or Jerry rebuild.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JERRY_ROOT="${JERRYSCRIPT_PATH:-/opt/jerryscript}"
JERRY_BUILD="${JERRYSCRIPT_BUILD:-$JERRY_ROOT/build}"
PICOJPEG_PATH="${PICOJPEG_PATH:-/opt/picojpeg}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
flags=(-std=gnu17 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
 -DMCUJS_BOARD_PICO=1 -I"$ROOT/host" -I"$ROOT/host/bindings" -I"$ROOT/src/filesystem"
 -I"$JERRY_ROOT/jerry-core/include" -I"$PICOJPEG_PATH")
"${CC:-cc}" "${flags[@]}" -c "$ROOT/host/bindings/image.c" -o "$TMP/image.o"
"${CC:-cc}" "${flags[@]}" -Dmalloc=jpeg_test_malloc -Dcalloc=jpeg_test_calloc -Dfree=jpeg_test_free -c "$ROOT/host/bindings/jpeg.c" -o "$TMP/jpeg.o"
python3 "$ROOT/scripts/prepare-picojpeg.py" "$PICOJPEG_PATH/picojpeg.c" "$TMP/picojpeg.c"
"${CC:-cc}" -std=gnu17 -I"$PICOJPEG_PATH" -c "$TMP/picojpeg.c" -o "$TMP/picojpeg.o"
"${CC:-cc}" "${flags[@]}" "$ROOT/tests/image_jpeg_jerry_test.c" "$ROOT/host/bindings/bindings.c" "$TMP/image.o" "$TMP/jpeg.o" "$TMP/picojpeg.o" -Wl,--gc-sections "$JERRY_BUILD/lib/libjerry-core.a" "$JERRY_BUILD/lib/libjerry-port.a" -lm -o "$TMP/test"
"$TMP/test" "$ROOT"
