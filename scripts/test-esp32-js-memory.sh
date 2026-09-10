#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JERRY_ROOT="${JERRYSCRIPT_PATH:-/opt/jerryscript}"
EXPECTED=50200152feb724a74a5f64e44d7885151537cfad
[[ "$(git -C "${JERRY_ROOT}" rev-parse HEAD)" == "${EXPECTED}" ]]
git -C "${JERRY_ROOT}" diff --quiet HEAD --
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
ulimit -c 0

python3 - "${TMP}" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
(p/'esp_log.h').write_text('''#include <stdio.h>
#define ESP_LOGE(tag, ...) do { (void)(tag); fprintf(stderr, __VA_ARGS__); fputc('\\n', stderr); } while (0)
#define ESP_LOGI(tag, ...) do { (void)(tag); fprintf(stdout, __VA_ARGS__); fputc('\\n', stdout); } while (0)
''')
(p/'sdkconfig.h').write_text('''#define CONFIG_SPIRAM 1
#define CONFIG_SPIRAM_MODE_OCT 1
#define CONFIG_SPIRAM_BOOT_INIT 1
#define CONFIG_SPIRAM_USE_MALLOC 1
''')
PY

build_test() {
    local kib="$1" external="$2" enabled=OFF
    [[ "${external}" == 0 ]] || enabled=ON
    python3 "${JERRY_ROOT}/tools/build.py" \
        --builddir="${TMP}/jerry-${kib}" --jerry-cmdline=OFF --jerry-ext=OFF \
        --jerry-math=OFF --jerry-port=OFF --lto=OFF --strip=OFF \
        --profile=es.next --amalgam=ON --line-info=ON --vm-throw=ON \
        --error-messages=ON --mem-stats=ON \
        --cpointer-32bit=OFF --external-context="${enabled}" --mem-heap="${kib}" \
        >"${TMP}/build-${kib}.log" 2>&1 || { python3 -c 'from pathlib import Path; import sys; print("\n".join(Path(sys.argv[1]).read_text().splitlines()[-40:]))' "${TMP}/build-${kib}.log"; return 1; }
    cc -std=gnu17 -Wall -Wextra -Werror \
        -DMCUJS_JS_HEAP_KIB="${kib}" -DMCUJS_JS_HEAP_EXTERNAL="${external}" -DESP_PLATFORM \
        -I"${TMP}" -I"${ROOT}/tests" -I"${ROOT}/tests/canvas_epaper_stubs" \
        -I"${JERRY_ROOT}/jerry-core/include" \
        "${ROOT}/tests/esp32_js_heap_test.c" "${ROOT}/platform/esp32/main/jerry_port.c" \
        "${TMP}/jerry-${kib}/lib/libjerry-core.a" -lm -o "${TMP}/test-${kib}"
}

build_test 256 1
"${TMP}/test-256"
build_test 128 0
python3 - "${TMP}/test-128" <<'PY'
import subprocess,sys,signal
p = subprocess.run([sys.argv[1]], capture_output=True, text=True)
assert p.returncode == -signal.SIGABRT, (p.returncode, p.stdout, p.stderr)
assert 'fatal error: 10' in p.stderr, p.stderr
print('PASS control: the identical live object graph exhausts the original 128KiB heap')
PY

# Missing PSRAM must be a compilation error, not a silent internal fallback.
printf '' > "${TMP}/sdkconfig.h"
if cc -std=gnu17 -fsyntax-only -DESP_PLATFORM -DMCUJS_JS_HEAP_KIB=256 -DMCUJS_JS_HEAP_EXTERNAL=1 \
    -I"${TMP}" -I"${ROOT}/tests/canvas_epaper_stubs" -I"${JERRY_ROOT}/jerry-core/include" \
    "${ROOT}/platform/esp32/main/jerry_port.c" >"${TMP}/invalid.log" 2>&1; then
    printf 'ERROR: accepted external heap without initialized PSRAM\n' >&2
    exit 1
fi
python3 - "${TMP}/invalid.log" <<'PY'
from pathlib import Path
import sys
assert 'requires initialized octal PSRAM' in Path(sys.argv[1]).read_text()
print('PASS configuration guard: external JS heap requires initialized PSRAM')
PY
