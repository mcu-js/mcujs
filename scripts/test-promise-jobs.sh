#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
J="${JERRYSCRIPT_PATH:-/opt/jerryscript}"
[[ "$(git -C "$J" rev-parse HEAD)" == 50200152feb724a74a5f64e44d7885151537cfad ]]
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
python3 - "$ROOT" "$J" "$T" <<'PY'
import importlib.util, sys
from pathlib import Path
root, source, temp = map(Path, sys.argv[1:])
spec = importlib.util.spec_from_file_location('jobs', root/'scripts/prepare-jerry-jobs.py')
jobs = importlib.util.module_from_spec(spec); spec.loader.exec_module(jobs)
staged = temp/'source'
jobs.prepare(source, staged)
jobs.prepare(source, staged)  # Idempotent reconfigure, untouched SDK source.
file = staged/'jerry-core/api/jerryscript.c'
saved = file.read_bytes(); file.write_bytes(saved+b'\n/* changed */\n')
try:
    jobs.prepare(source, staged)
except ValueError:
    pass
else:
    raise AssertionError('Modified staging accepted')
file.write_bytes(saved)
try:
    jobs.prepare(staged, temp/'unsupported-source')
except ValueError:
    pass
else:
    raise AssertionError('Non-pinned source accepted')
print('Pinned staging, repeat configure and mutation rejection: PASS')
PY
J="$T/source"
python3 "$J/tools/build.py" --builddir="$T/build" --jerry-cmdline=OFF --jerry-ext=OFF \
 --jerry-math=OFF --jerry-port=ON --lto=OFF --strip=OFF --profile=es.next \
 --line-info=ON --vm-throw=ON --error-messages=ON --cpointer-32bit=OFF --mem-heap=64 --cmake-param=-DJERRY_MEM_STATS=ON >"$T/build.log" 2>&1 || { cat "$T/build.log"; exit 1; }
mkdir -p "$T/stubs/hardware"
printf '#pragma once\n' >"$T/stubs/hardware/timer.h"
for platform in rp2 esp32; do
 if [[ "$platform" == rp2 ]]; then
  backend=platform/rp2
  flags=(-DMCUJS_PLATFORM_RP2=1 -DMCUJS_BOARD_PICO=1)
 else
  backend=platform/esp32/main
  flags=(-DMCUJS_PLATFORM_ESP32=1 -DMCUJS_BOARD_SEEED_XIAO_ESP32S3=1)
 fi
 cc -std=gnu17 -Wall -Wextra -ffunction-sections -fdata-sections "${flags[@]}" \
  -I"$ROOT/host" -I"$ROOT/host/bindings" -I"$ROOT/src/filesystem" \
  -I"$J/jerry-core/include" -I"$T/stubs" -I"$ROOT/tests/native_stubs/rp2" -I"$ROOT/tests/canvas_epaper_stubs" \
  "$ROOT/tests/promise_jobs_test.c" "$ROOT/host/engine.c" \
  "$ROOT/host/bindings/bindings.c" "$ROOT/$backend/bindings/timers.c" \
  -Wl,--gc-sections "$T/build/lib/libjerry-core.a" "$T/build/lib/libjerry-port.a" -lm -o "$T/test-$platform"
 timeout 15 "$T/test-$platform"
done
