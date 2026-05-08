---
sidebar_position: 15
---

# Advanced: Building from Source

Most users can use prebuilt [UF2](./glossary.md#uf2) releases. Build from source when you are developing the runtime, adding a board, or preparing a release.

## Prerequisites

- Docker (recommended)
- Or ARM GCC toolchain, CMake 3.13+, and Pico SDK 2.2.0
- Bun for the hardware end-to-end test harness
- Node.js 20+ for docs builds
- A supported board connected over [USB](./glossary.md#usb) for hardware testing

## Board registry

The release board list lives in `scripts/lib/boards.sh`.

```bash
scripts/boards.sh
scripts/boards.sh --table
```

When adding or removing a board, update that registry first, then run:

```bash
scripts/verify-release.sh --allow-dirty
```

## Build with Docker

```bash
./build.sh pico
./build.sh pico2
./build.sh all --clean
```

Outputs land in `build/` as [UF2](./glossary.md#uf2) files. Rebuild the Docker image after dependency or Dockerfile changes:

```bash
./build.sh all --clean --rebuild-image
```

## Manual build

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
./build.sh pico --no-docker
```

## End-to-end tests

The Bun harness builds firmware, flashes the connected board when needed, and exercises the REPL and filesystem. It currently targets the default Pico workflow.

```bash
bun run e2e
```

## Release build

Use the release script to verify metadata, build every board from a clean CMake directory, and package deterministic release assets:

```bash
scripts/release.sh
```

The package step writes:

- `dist/mcujs-<version>-<git-sha>/` with every UF2, `RELEASE_MANIFEST.txt`, and `SHA256SUMS.txt`
- `dist/mcujs-<version>-<git-sha>.tar.gz`
- top-level UF2 assets under `dist/` for upload to GitHub Releases

If CI already built the UF2 files, package them without rebuilding:

```bash
scripts/release.sh --skip-build
```

## Source verification

Run the source verifier before opening a pull request that changes boards, scripts, docs, or release files:

```bash
scripts/verify-release.sh --allow-dirty
```

Run docs checks too when touching the Docusaurus site:

```bash
scripts/verify-release.sh --allow-dirty --docs
```

## Key terms

- [UF2](./glossary.md#uf2)
- [Firmware](./glossary.md#firmware)
- [Pico SDK](./glossary.md#pico-sdk)
- [Bootloader](./glossary.md#bootloader)
