## Summary

- 

## Runtime Boundary

Which layer owns this change?

- [ ] JavaScript engine adapter (`javascript/`, `host/engine.*`, `host/jerry_port.c`)
- [ ] Module system (`host/module_loader.*`, `host/bindings/require.c`)
- [ ] Native binding (`host/bindings/`)
- [ ] Firmware service (`src/`, `src/usb/`, `src/filesystem/`)
- [ ] Board support (`board/<board-id>/`)
- [ ] Tooling, docs, examples, or tests

## Test Evidence

- Board ID:
- Firmware build ID from `.info`:
- Commands run:

## Checklist

- [ ] I kept USB CDC serial text on `\r\n` line endings where applicable.
- [ ] I updated docs or examples for user-facing behavior changes.
- [ ] I ran `scripts/verify-release.sh --allow-dirty` for board, release, script, or docs metadata changes.
- [ ] I ran `bun run e2e` or documented why hardware testing was not possible.
