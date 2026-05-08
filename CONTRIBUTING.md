# Contributing to mcujs

Thanks for helping make JavaScript on microcontrollers easier to use.

## Start here

```bash
scripts/verify-release.sh --allow-dirty
./build.sh pico
bun run e2e
```

The hardware test harness expects a connected Pico-compatible board. If you cannot run hardware tests, include that in your pull request notes.

## Runtime boundaries

- `javascript/` and `host/engine.*` contain JerryScript-specific integration.
- `host/bindings/` contains JavaScript-facing native APIs.
- `src/` contains firmware services such as boot, REPL, USB, and filesystem plumbing.
- `board/<board-id>/` contains board feature flags, pins, flash size, and memory maps.
- `scripts/lib/boards.sh` is the release board registry used by build and packaging scripts.

Keep new APIs in the narrowest layer that owns the behavior. Keep USB CDC serial output on `\r\n` line endings.

## Pull request notes

Include:

- Board ID tested
- Firmware build ID from `.info`
- Commands run
- Any hardware or host OS details relevant to the change

Use the issue and pull request templates when opening GitHub work. They ask for the board, runtime boundary, and test evidence needed to review firmware changes without guessing.

See `docs/docs/contributing.md` and `docs/docs/architecture.md` for the detailed contributor guide.
