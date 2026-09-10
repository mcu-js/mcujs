#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
for BITS in 32 64; do
    EXTRA=""
    if [ "$BITS" = 64 ]; then EXTRA="-DSD_TEST_LBA64=1"; fi
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g \
        ${SD_TEST_CFLAGS:-} $EXTRA -DMCUJS_HAS_SD=1 \
        -I"$ROOT/tests/native_stubs/sd_spi" \
        -I"$ROOT/board/waveshare_rp2350_lcd_1.47_a" \
        -I"$ROOT/platform/rp2/filesystem" \
        "$ROOT/platform/rp2/filesystem/sd_spi.c" "$ROOT/tests/sd_spi_test.c" \
        -o "$TMP/sd-spi-$BITS"
    "$TMP/sd-spi-$BITS"
done
