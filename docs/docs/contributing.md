---
sidebar_position: 16
---

# Contributing

Contributions are welcome. The easiest way to help is to keep changes small, run the deterministic checks, and explain which board or runtime surface you tested.

## Branch and review policy

- `main` is the protected release branch (not `master`) and is release-only.
- Feature branches start from the latest approved `development` tip.
- Each implementation requires independent review before integration.
- Release candidates are immutable.
- Physical QA gates the `development` to `main` pull request.
- The pull request must precede the merge.
- Release tags and publishing happen only after the merge.

Reviewed feature work integrates into `development`; contributors do not develop directly on `main`. See [Releasing](./releasing.md) for the candidate, QA, pull request, merge, tag, and publish sequence.

## Development flow

1. Create a feature branch from the latest approved `development` tip.
2. Run `scripts/verify-release.sh --allow-dirty` before editing release metadata or board support.
3. Build a [UF2](./glossary.md#uf2) for the board you are changing.
4. Flash and test on hardware when behavior touches firmware, board config, USB, filesystem, or a hardware API.
5. Run `bun run e2e` when you can test with a connected Pico.
6. Obtain independent review of the implementation.
7. Include the board ID, firmware build ID, and test notes in the pull request.

GitHub issue and pull request templates collect the board, runtime boundary, and test evidence reviewers need. Use the board support template for new boards so the release registry, docs, and CMake config stay aligned.

## Runtime boundaries

- `javascript/` owns the JerryScript integration. Keep engine-specific code here so a future engine swap does not leak through the runtime.
- `host/engine.*` and `host/module_loader.*` own JavaScript evaluation and CommonJS loading.
- `host/bindings/` owns JavaScript-facing native APIs such as `gpio`, `fs`, `process`, `keyboard`, and `mouse`.
- `src/` owns firmware services: boot, REPL, USB CDC/MSC, filesystem plumbing, and board startup.
- `src/filesystem/` and `src/usb/` are service layers. Prefer calling their APIs instead of reaching through their internals.
- `board/<board-id>/` owns pin maps, flash size, memory maps, and feature flags.
- `examples/` should stay non-blocking. Use timers for demos and avoid long loops that prevent REPL recovery.

## Adding a runtime API

1. Decide whether it is a global, a built-in module, or a board-specific helper.
2. Add the native binding under `host/bindings/` unless it is purely firmware plumbing.
3. Keep board-specific constants in `board/<board-id>/board_config.h`.
4. Use `\r\n` for any text written to USB CDC serial.
5. Document the API in `docs/docs/built-in-modules.md` or `docs/docs/runtime-javascript.md`.
6. Add an example that exits cleanly or can be interrupted from the REPL.

Portable API changes must follow the [MCU.js 0.2 portable API and capability-discovery design](./development/mcujs-0.2-portable-api.md). Feature-detect capabilities instead of branching on board names, omit unsupported APIs, and use stable errors for failures from supported APIs.

## Adding a board

1. Create `board/<board-id>/board_config.h` and `board/<board-id>/board_config.cmake`.
2. Add `memmap_mcujs.ld` if the flash layout differs from an existing supported board.
3. Add the board to `scripts/lib/boards.sh`.
4. Document it in [Hardware and Boards](./hardware-boards.md).
5. Build it with `./build.sh <board-id>`.

## Checks

```bash
scripts/verify-release.sh --allow-dirty
npm --prefix docs run build
bun run e2e
```

Hardware-specific changes should include the exact board ID from `scripts/boards.sh`.

## Good first steps

- Improve docs or examples
- Add a focused Bun test to the hardware harness
- Expand an existing hardware API
- Add board-specific examples for a supported display or sensor

## Key terms

- [Firmware](./glossary.md#firmware)
- [Runtime](./glossary.md#runtime)
- [UF2](./glossary.md#uf2)
