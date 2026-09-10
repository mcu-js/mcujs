#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

node --test \
    "${ROOT}/tests/events.test.js" \
    "${ROOT}/tests/devices.test.js" \
    "${ROOT}/tests/buttons.test.js" \
    "${ROOT}/tests/device-capabilities.test.js" \
    "${ROOT}/tests/canvas-2d.test.js" \
    "${ROOT}/tests/display-canvas.test.js" \
    "${ROOT}/tests/runtime-registry.test.js" \
    "${ROOT}/tests/display-capability-honesty.test.js" \
    "${ROOT}/tests/portable-api-conformance.test.js" \
    "${ROOT}/tests/examples-gpio-contract.test.js" \
    "${ROOT}/tests/examples-i2c-contract.test.js"
"${ROOT}/scripts/test-runtime-registry.sh"
"${ROOT}/scripts/test-runtime-bindings.sh"
bash "${ROOT}/scripts/test-promise-jobs.sh"
"${ROOT}/scripts/test-runtime-validation.sh"
"${ROOT}/scripts/test-repl.sh"

printf 'Portable API conformance passed for full RP, constrained RP2350, and ESP32 lanes.\n'
