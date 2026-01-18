---
sidebar_position: 15
---

# Advanced: Building from Source

Most users can stick to the prebuilt [UF2](./glossary.md#uf2) releases. Build from source if you are developing the runtime or need a custom build.

## Prerequisites

- Docker (recommended)
- Or ARM GCC toolchain, CMake 3.13+, Pico SDK 2.x
- A Pico board connected over [USB](./glossary.md#usb) for testing

## Build with Docker

```bash
./build.sh pico
./build.sh pico2
./build.sh all
```

Outputs land in `build/` as [UF2](./glossary.md#uf2) files you can drag onto the board.

## End-to-end tests

```bash
bun run e2e
```

## Manual build

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build
cmake -DBOARD=pico ..
make -j$(nproc)
```

## Key terms

- [UF2](./glossary.md#uf2)
- [Firmware](./glossary.md#firmware)
- [Pico SDK](./glossary.md#pico-sdk)
- [Bootloader](./glossary.md#bootloader)
