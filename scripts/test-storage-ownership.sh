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

"${TMP_ROOT}/fs-ownership-test" handoff
"${TMP_ROOT}/fs-ownership-test" unmount-failure
"${TMP_ROOT}/fs-ownership-test" remount-failure

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
    -DMCUJS_TEST_ESP32=1 \
    -I"${ROOT}/tests/native_stubs/usb" \
    -I"${ROOT}/src/filesystem" \
    -I"${ROOT}/platform/esp32/main" \
    -I"${ROOT}/src/usb" \
    "${ROOT}/tests/usb_msc_backend_test.c" \
    "${ROOT}/src/usb/msc_ownership.c" \
    "${ROOT}/platform/esp32/main/usb_msc.c" \
    -o "${TMP_ROOT}/esp32-msc-backend-test"

"${TMP_ROOT}/esp32-msc-backend-test"
node --test "${ROOT}/tests/runtime-registry.test.js"
