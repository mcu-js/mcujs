#!/usr/bin/env bash
# Real pinned IDF FatFs with native SDK boundary fixtures; no SDK install/device.
set -euo pipefail
ulimit -c 0
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/mcujs-esp-sd.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
python3 - "$ROOT" "$TMP" <<'PY'
import pathlib, sys, urllib.request
root,tmp=map(pathlib.Path,sys.argv[1:])
base='https://raw.githubusercontent.com/espressif/esp-idf/9d7f2d69f50d1288526d4f1027108e314e8c879f/'
for f in ['src/ff.c','src/ff.h','src/ffconf.h','src/diskio.h','diskio/diskio_impl.h']:
    (tmp/pathlib.Path(f).name).write_bytes(urllib.request.urlopen(base+'components/fatfs/'+f,timeout=60).read())
# Test declarations, not SDK implementations. Keep real ff.h first, never the
# small Sticky mount stub. The production adapter is compiled without edits.
for f in ['sdmmc_cmd.h','esp_vfs_fat.h']:
    (tmp/f).write_bytes((root/'tests/sticky_sd_stubs'/f).read_bytes())
(tmp/'sd_config.h').write_text('/* V2 policy supplied by compiler flags in this fixture. */\n')
print('ESP-IDF FatFs source pinned to 9d7f2d69f50d1288526d4f1027108e314e8c879f')
PY
cc -std=gnu17 -Wall -Wextra -Werror -Wno-misleading-indentation ${SD_TEST_CFLAGS:-} \
 -DMCUJS_HAS_SD=1 -DMCUJS_SD_SDMMC=1 -DMCUJS_SD_READONLY=0 -DMCUJS_USB_SD_MSC=1 \
 -DMCUJS_SD_SDMMC_CLK_PIN=39 -DMCUJS_SD_SDMMC_CMD_PIN=41 -DMCUJS_SD_SDMMC_D0_PIN=40 \
 -DMCUJS_SD_BAUD_HZ=4000000 -DMCUJS_SD_CARD_DETECT_PIN=-1 \
 -I"$TMP" -I"$ROOT/tests/esp_storage_stubs" -I"$ROOT/tests/canvas_epaper_stubs" \
 -I"$ROOT/tests/sticky_sd_stubs" -I"$ROOT/src/filesystem" -I"$ROOT/platform/esp32/main" \
 "$ROOT/tests/esp_sd_card_test.c" "$ROOT/platform/esp32/main/sd_card.c" "$TMP/ff.c" \
 -o "$TMP/esp-sd"
for scenario in sfd mbr malformed; do "$TMP/esp-sd" "$scenario"; done
