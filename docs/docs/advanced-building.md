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

Prepare each SDK image explicitly. This phase may download dependencies; repeat
it after Dockerfile or dependency changes:

```bash
./build.sh pico --prepare-image
./build.sh seeed_xiao_esp32s3 --prepare-image
```

Then compile firmware without network access or implicit image acquisition:

```bash
./build.sh pico
./build.sh pico2
./build.sh seeed_xiao_esp32s3
./build.sh all --clean
```

`all` (also the default) retains the existing RP board list; it does not include
ESP32. Both lanes mount source read-only, compile an ephemeral copy, and write
artifacts as the caller's numeric UID/GID. RP UF2/BIN/ELF/MAP outputs land in
`build/`; XIAO outputs stay in `platform/esp32/build-docker/` for release tooling.
Docker builds are always fresh; `--clean` only changes local RP cache behavior.
No host SDK is needed. Use a local Docker daemon with access to the checkout.

Image overrides accept a local tag or immutable ID:

```bash
MCUJS_RP_DOCKER_IMAGE=my-rp-builder:reviewed ./build.sh pico
MCUJS_ESP32_DOCKER_IMAGE=my-esp-builder:reviewed ./build.sh seeed_xiao_esp32s3
```

Defaults are `mcujs-builder` and `mcujs-esp32-builder:v5.3.2-amd64`. A build resolves
the configured identity to a local immutable image ID and runs with `--pull never`
and `--network none`. Missing images fail before outputs are touched, with an
explicit preparation command. Use a tag, not an immutable ID, when preparing.
The pinned direct dependency checks and ESP component-lock checks remain in place.
RP uses the local image architecture; ESP32 forces `linux/amd64` (other hosts need
compatible emulation). Stub tests do not establish cross-architecture or all-target
buildability, firmware safety, or byte reproducibility.

`--docker-network VALUE` and `MCUJS_DOCKER_NETWORK` configure preparation only.
The flag is rejected on an ordinary build; the environment variable is ignored
for compilation. `--rebuild-image` is now an alias for preparation only, so run
the build command separately afterward. The release wrapper's explicit
`--rebuild-image` flag still prepares RP then compiles; prepare ESP32 separately.
`--debug` and `--no-docker` remain RP-only; XIAO rejects these flags rather than
silently ignoring them. The existing `platform/esp32/docker-build.sh` supports the
same separate preparation/build phases.

Build completion is not permission to flash. The current ESP32 `uf2` factory
partition-size warning remains an unresolved pre-flash gate; this workflow does
not change flash layout or establish recovery safety.

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

Use the release script to verify metadata, build every RP board from a clean
CMake directory, freshly build the XIAO ESP32-S3 with its pinned Docker lane,
and package deterministic release assets:

```bash
scripts/release.sh
```

The package step preflights every source, rejects unsupported XIAO UF2 flags or
an `ota_0` payload overrun, then privately stages and atomically replaces:

- `dist/mcujs-<version>-<git-sha>/` with every UF2, `RELEASE_MANIFEST.txt`, and `SHA256SUMS.txt`
- `dist/mcujs-<version>-<git-sha>.tar.gz`
- top-level UF2 assets under `dist/` for upload to GitHub Releases

If CI already built the UF2 files, package them without rebuilding:

```bash
scripts/release.sh --skip-build
```

`--no-docker` selects the local Pico toolchain only for RP boards; the XIAO
release artifact always uses its pinned Docker lane. Even with `--skip-build`,
packaging verifies that the XIAO UF2 matches its adjacent binary, embeds the
current source build ID, and carries the generated capability manifest, so a
stale image cannot be relabelled as the current release.

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
