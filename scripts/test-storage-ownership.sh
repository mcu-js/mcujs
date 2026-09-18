#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/mcujs-storage-ownership.XXXXXX")"
trap 'rm -rf "${TMP_ROOT}"' EXIT

cc -std=gnu17 -Wall -Wextra -Werror \
    -I"${ROOT}/src/filesystem" \
    -I"${ROOT}/src/usb" \
    "${ROOT}/tests/msc_ownership_test.c" \
    "${ROOT}/src/usb/msc_ownership.c" \
    -o "${TMP_ROOT}/msc-ownership-test"

"${TMP_ROOT}/msc-ownership-test"

cc -std=gnu17 -Wall -Wextra -Werror \
    -I"${ROOT}/tests/native_stubs/storage" \
    -I"${ROOT}/src/filesystem" \
    -I"${ROOT}/src/usb" \
    "${ROOT}/tests/fs_ownership_test.c" \
    "${ROOT}/src/filesystem/fs.c" \
    -o "${TMP_ROOT}/fs-ownership-test"

"${TMP_ROOT}/fs-ownership-test" namespace
"${TMP_ROOT}/fs-ownership-test" handoff
"${TMP_ROOT}/fs-ownership-test" unmount-failure
"${TMP_ROOT}/fs-ownership-test" remount-failure

cc -std=gnu17 -Wall -Wextra -Werror -DMCUJS_HAS_SD=1 \
    -I"${ROOT}/tests/native_stubs/storage" -I"${ROOT}/src/filesystem" -I"${ROOT}/src/usb" \
    "${ROOT}/tests/fs_ownership_test.c" "${ROOT}/src/filesystem/fs.c" \
    -o "${TMP_ROOT}/fs-sd-test"
"${TMP_ROOT}/fs-sd-test" sd
"${TMP_ROOT}/fs-sd-test" sd-write

cc -std=gnu17 -Wall -Wextra -Werror -DMCUJS_HAS_SD=1 -DMCUJS_USB_SD_MSC=1 -DMCUJS_TEST_DUAL_MSC=1 \
    -I"${ROOT}/tests/native_stubs/storage" -I"${ROOT}/tests/native_stubs/usb" \
    -I"${ROOT}/src/filesystem" -I"${ROOT}/src/usb" \
    "${ROOT}/tests/fs_ownership_test.c" "${ROOT}/src/filesystem/fs.c" \
    "${ROOT}/src/usb/msc_ownership.c" "${ROOT}/platform/rp2/usb/usb_msc.c" \
    -o "${TMP_ROOT}/dual-msc-test"
"${TMP_ROOT}/dual-msc-test" sd-malformed
"${TMP_ROOT}/dual-msc-test" sd-bounds
"${TMP_ROOT}/dual-msc-test" dual
"${TMP_ROOT}/dual-msc-test" sd-read-checks
"${TMP_ROOT}/dual-msc-test" sd-sync-checks

cc -std=gnu17 -Wall -Wextra -Werror \
    -I"${ROOT}/tests/native_stubs/usb" \
    -I"${ROOT}/src/filesystem" \
    -I"${ROOT}/src/usb" \
    -I"${ROOT}/platform/rp2/usb" \
    "${ROOT}/tests/usb_msc_backend_test.c" \
    "${ROOT}/src/usb/msc_ownership.c" \
    "${ROOT}/platform/rp2/usb/usb_msc.c" \
    -o "${TMP_ROOT}/rp2-msc-backend-test"

"${TMP_ROOT}/rp2-msc-backend-test"

cc -std=gnu17 -Wall -Wextra -Werror \
    -DMCUJS_TEST_ESP32=1 -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1 \
    -I"${ROOT}/src/generated" \
    -I"${ROOT}/tests/native_stubs/usb" \
    -I"${ROOT}/src/filesystem" \
    -I"${ROOT}/platform/esp32/main" \
    -I"${ROOT}/src/usb" \
    "${ROOT}/tests/usb_msc_backend_test.c" \
    "${ROOT}/src/usb/msc_ownership.c" \
    "${ROOT}/platform/esp32/main/usb_msc.c" \
    -o "${TMP_ROOT}/esp32-msc-backend-test"

"${TMP_ROOT}/esp32-msc-backend-test"

cc -std=gnu17 -Wall -Wextra -Werror -DMCUJS_BOARD_WAVESHARE_ESP32S3_EPAPER_1_54_V2=1 \
    -I"${ROOT}/src/generated" \
    -I"${ROOT}/tests/native_stubs/usb" -I"${ROOT}/src/filesystem" \
    -I"${ROOT}/platform/esp32/main" -I"${ROOT}/src/usb" \
    "${ROOT}/tests/esp32_dual_msc_test.c" "${ROOT}/src/usb/msc_ownership.c" \
    "${ROOT}/platform/esp32/main/usb_msc.c" -o "${TMP_ROOT}/esp32-dual-msc-test"
"${TMP_ROOT}/esp32-dual-msc-test"
# Compile the actual ESP32 path-facing functions against native POSIX stubs.
# SDK/partition/ownership lifecycle code is deliberately outside this seam.
python3 - "${ROOT}" "${TMP_ROOT}" <<'PY'
from pathlib import Path
import sys
root, tmp = map(Path, sys.argv[1:])
source = (root / 'platform/esp32/main/filesystem.c').read_text()
helpers = source[source.index('static fs_result_t errno_to_fs('):source.index('static fs_result_t ensure_initialized(void) {')]
operations = source[source.index('fs_result_t fs_open('):]
(tmp / 'esp-path-functions.inc').write_text(helpers + operations)
PY
cc -std=gnu17 -Wall -Wextra -Werror \
    -DTEST_ESP_PATHS -I"${ROOT}/src/filesystem" -I"${TMP_ROOT}" \
    "${ROOT}/tests/fs_path_test.c" -o "${TMP_ROOT}/fs-path-test"
"${TMP_ROOT}/fs-path-test"

cc -std=gnu17 -Wall -Wextra -Werror -Wno-unused-function \
    -DTEST_ESP_PATHS -DMCUJS_HAS_SD=1 -I"${ROOT}/src/filesystem" -I"${TMP_ROOT}" \
    "${ROOT}/tests/fs_path_test.c" -o "${TMP_ROOT}/fs-sd-path-test"
"${TMP_ROOT}/fs-sd-path-test"

if [[ "${1:-}" != "--native-only" ]]; then
    node --test "${ROOT}/tests/runtime-registry.test.js"
fi
