#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

node --test \
    "${ROOT}/tests/runtime-registry.test.js" \
    "${ROOT}/tests/portable-api-conformance.test.js"
"${ROOT}/scripts/test-runtime-registry.sh"
"${ROOT}/scripts/test-runtime-bindings.sh"
"${ROOT}/scripts/test-runtime-validation.sh"
"${ROOT}/scripts/test-repl.sh"

printf 'Portable API conformance passed for full RP, constrained RP2350, and ESP32 lanes.\n'
