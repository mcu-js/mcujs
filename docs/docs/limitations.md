---
sidebar_position: 10
---

# Limitations

These are normal constraints for tiny boards. Keep them in mind as you build.

## Memory

- RP2040: 264KB SRAM total, ~100KB JS heap
- RP2350: 520KB SRAM total, ~200KB JS heap
- If you see out-of-memory errors, slim arrays or move data into files

## JavaScript

- No `async`/`await`
- No `Proxy` or `Reflect`
- Limited `RegExp` support
- No `BigInt`
- 32-bit integers only

## Filesystem

- ~1MB usable on Pico, ~3.5MB on Pico 2
- Host OS caching may delay visibility of device-written files

## [USB](./glossary.md#usb)

- [CDC](./glossary.md#cdc) + [MSC](./glossary.md#msc) can conflict on some hosts
- Windows may cache the filesystem aggressively

## Key terms

- [CDC](./glossary.md#cdc)
- [MSC](./glossary.md#msc)
- [USB](./glossary.md#usb)
