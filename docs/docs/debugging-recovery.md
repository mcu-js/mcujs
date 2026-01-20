---
sidebar_position: 9
---

# Debugging and Recovery

If a script goes off the rails, you have a quick way back in. See the [Glossary](./glossary.md) if you need a refresher on terms like UF2 or REPL.

## Safe boot

Hold the **BOOTSEL** button during power-on to skip `index.js` auto-run. This keeps the on-device drive accessible so you can delete or edit scripts without reflashing.

If you are stuck in a loop, this is the fastest way back in.

## Boot status indicator

On startup the board blinks its LED (or NeoPixel on boards without an LED) to show boot progress:

| Pattern | Meaning |
| --- | --- |
| 3 blinks | Reached main successfully |
| 10 rapid blinks + pause (repeating) | Filesystem init failed |
| 5 rapid blinks + pause (repeating) | JavaScript engine init failed |

If you see repeating rapid blinks, try reflashing the firmware.

## [UF2](./glossary.md#uf2) mode

- Use `.uf2` to reboot into [UF2](./glossary.md#uf2) mode (prompted)
- Use `.uf2!` to reboot immediately

## REPL tips

- Use the [REPL](./glossary.md#repl) for quick checks without editing files
- `.info` shows firmware build IDs and board details
- `.run` is a quick way to retry a script after edits

## Key terms

- [UF2](./glossary.md#uf2)
- [BOOTSEL](./glossary.md#bootsel)
- [REPL](./glossary.md#repl)
- [Firmware](./glossary.md#firmware)
