#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
for BOARD in 147 2040 28; do
for RATE in 5000000 10000000; do
for BITS in 32 64; do
    EXTRA=""
    if [ "$BITS" = 64 ]; then EXTRA="-DSD_TEST_LBA64=1"; fi
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g \
        ${SD_TEST_CFLAGS:-} $EXTRA -DSD_TEST_BOARD="$BOARD" -DMCUJS_SD_SPI_BAUD_HZ="$RATE" \
        -I"$ROOT/tests/native_stubs/sd_spi" \
        -I"$ROOT/platform/rp2/filesystem" \
        "$ROOT/platform/rp2/filesystem/sd_spi.c" "$ROOT/tests/sd_spi_test.c" \
        -o "$TMP/sd-spi-$BITS"
    printf 'Board %s SPI target %s Hz: ' "$BOARD" "$RATE"
    "$TMP/sd-spi-$BITS"
done
done
done
