#!/usr/bin/env bash
# Host-only IDF boundary tests; no SDK, downloads, Docker or device access.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC="${CC:-cc}"
TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/mcujs-epaper.XXXXXX")"
trap 'rm -rf "${TMP_ROOT}"' EXIT
for partial in 0 1; do
 for diff in 0 1; do
  for dither in 0 1; do
   flags=(-std=gnu17 -Wall -Wextra -Werror
    "-I${ROOT}/host/bindings" "-I${ROOT}/tests/canvas_epaper_stubs")
   [[ "$partial" == 0 ]] || flags+=(-DMCUJS_EXPERIMENTAL_EPAPER_PARTIAL=1)
   [[ "$diff" == 0 ]] || flags+=(-DMCUJS_EXPERIMENTAL_EPAPER_DIFF=1)
   [[ "$dither" == 0 ]] || flags+=(-DMCUJS_EXPERIMENTAL_EPAPER_DITHER=1)
   printf 'ePaper flags PARTIAL=%s DIFF=%s DITHER=%s\n' "$partial" "$diff" "$dither"
   "$CC" "${flags[@]}" "${ROOT}/tests/canvas_epaper_test.c" -o "${TMP_ROOT}/driver"
   "${TMP_ROOT}/driver"
  done
 done
done
