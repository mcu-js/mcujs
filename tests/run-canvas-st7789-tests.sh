#!/bin/sh
# No Pico SDK installation or board access: build against focused SDK fakes.
set -eu
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g ${CANVAS_TEST_CFLAGS:-} \
    -Ihost/bindings -Itests/canvas_st7789_stubs \
    tests/canvas_st7789_test.c host/bindings/canvas_display_st7789.c \
    host/bindings/canvas_spi.c -Wl,--wrap=calloc -Wl,--wrap=free \
    -o "$build_dir/canvas_st7789_test"
"$build_dir/canvas_st7789_test"
