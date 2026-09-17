#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
for RATE in 5000000 10000000; do
for BITS in 32 64; do
    EXTRA=""
    if [ "$BITS" = 64 ]; then EXTRA="-DSD_TEST_LBA64=1"; fi
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g \
        ${SD_TEST_CFLAGS:-} $EXTRA -DMCUJS_HAS_SD=1 -DMCUJS_SD_SPI_BAUD_HZ="$RATE" \
        -I"$ROOT/tests/native_stubs/sd_spi" \
        -I"$ROOT/board/waveshare_rp2350_lcd_1.47_a" \
        -I"$ROOT/platform/rp2/filesystem" \
        "$ROOT/platform/rp2/filesystem/sd_spi.c" "$ROOT/tests/sd_spi_test.c" \
        -o "$TMP/sd-spi-$BITS"
    printf 'SPI target %s Hz: ' "$RATE"
    "$TMP/sd-spi-$BITS"
done
done
