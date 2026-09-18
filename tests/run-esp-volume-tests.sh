#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
python3 - "$ROOT" "$TMP" <<'PY'
from pathlib import Path
import sys
root,tmp=map(Path,sys.argv[1:])
s=(root/'platform/esp32/main/filesystem.c').read_text()
enum=s[s.index('typedef enum {'):s.index('static atomic_int s_storage_state')]
state='static atomic_uint s_sd_open_files;\nstatic atomic_int s_sd_state = STORAGE_UNINITIALIZED;\nstatic uint32_t s_sd_start, s_sd_sectors;\n'
body=s[s.index('bool fs_volume_host_owned('):s.index('fs_result_t fs_open(')]
(tmp/'esp-volume-functions.inc').write_text(enum+state+body)
helpers=s[s.index('static fs_result_t errno_to_fs('):s.index('static fs_result_t ensure_initialized(void) {')]
operations=s[s.index('fs_result_t fs_open('):]
(tmp/'esp-path-functions.inc').write_text(helpers+operations)
PY
cc -std=gnu17 -Wall -Wextra -Werror -DMCUJS_USB_SD_MSC=1 -DMCUJS_SD_READONLY=0 \
 -I"$ROOT/src/filesystem" -I"$TMP" "$ROOT/tests/esp_volume_test.c" -o "$TMP/volumes"
"$TMP/volumes"
for policy in none ro rw; do
 flags=""
 case "$policy" in ro) flags="-DMCUJS_HAS_SD=1 -DMCUJS_SD_READONLY=1";; rw) flags="-DMCUJS_HAS_SD=1 -DMCUJS_SD_READONLY=0";; esac
 cc -std=gnu17 -Wall -Wextra -Werror -Wno-unused-function -DTEST_ESP_PATHS $flags \
  -I"$ROOT/src/filesystem" -I"$TMP" "$ROOT/tests/fs_path_test.c" -o "$TMP/paths"
 "$TMP/paths"
done
