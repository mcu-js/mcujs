---
sidebar_position: 9
---

# Debugging and Recovery

If a script goes off the rails, you have a quick way back in. See the [Glossary](./glossary.md) if you need a refresher on terms like UF2 or REPL.

## Safe boot

Hold the **BOOTSEL** button during power-on to skip `index.js` auto-run. This keeps the on-device drive accessible so you can delete or edit scripts without reflashing.

If you are stuck in a loop, this is the fastest way back in.

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
