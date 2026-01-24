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

## Filesystem recovery

The filesystem auto-formats on first boot or if corruption is detected. If you need to manually reset the filesystem:

- Use `.format` for a prompted format (3-second countdown, press any key to cancel)
- Use `.format!` for immediate format without confirmation

**Warning:** Formatting erases all files on the device.

The filesystem size is automatically calculated based on the board's flash size minus the firmware and a small EEPROM reservation.

## REPL tips

- Use the [REPL](./glossary.md#repl) for quick checks without editing files
- `.info` shows firmware build IDs and board details
- `.run` is a quick way to retry a script after edits

## Key terms

- [UF2](./glossary.md#uf2)
- [BOOTSEL](./glossary.md#bootsel)
- [REPL](./glossary.md#repl)
- [Firmware](./glossary.md#firmware)
