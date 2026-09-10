---
title: Runtime foundation hardware acceptance
---

# Runtime foundation hardware acceptance

This bounded development checkpoint covers the integration of PRs #13–#17,
the PSRAM arena alignment correction, and Sticky's board-level restart gesture.
It is not a release qualification or general endurance result.

## Tested candidate

Hardware candidate: `d7cfa414e5edd13bc057e53758069edf3725c164`.
The squash integration preserves that candidate's implementation; documentation
records the results below. Rebuilding after squashing changes the embedded build
identity, so the new artifact must not be described as already physically flashed.

## Seeed reTerminal Sticky

- Boot identified the configured 256 KiB JavaScript heap in PSRAM.
- The arena uses explicit eight-byte alignment, required by JerryScript's
  compressed pointers. The earlier ordinary allocation path caused startup
  invalid-memory-access panics; that candidate is not accepted.
- The Night Ferry and a full-screen 20-by-12 checkerboard of 40-pixel squares
  passed operator visual checks.
- A 5,000-live-object workload retained correct values across display refresh,
  release/GC and subsequent allocation, alongside the 768,000-byte framebuffer.
  No presentation failures, watchdog reset or crash were observed in the test.
- The operator confirmed the AI/Power hold-to-restart behavior. This is a
  board-level three-second gesture, not `devices.button` and not power-off.
- Safe-boot timing remains unchanged. An e-paper image alone does not establish
  that the MCU is running.

Battery endurance, full power-off/on cold startup, long-term ghosting and
sustained leak freedom remain outside this checkpoint.

## Vanilla Pico RP2040

The non-PSRAM reference board passed two matching full-flash recovery reads,
each verified against the device, before candidate installation. Candidate
application bytes and the protected flash tail passed readback checks.

Thirteen live automated checks passed:

- Truthful device discovery, frozen descriptors and cached module identity.
- Promise completion, async/await through a timer, and rejection handling.
- Timer progress during a 5,000-job self-replenishing Promise sequence.
- Event dispatch, once listeners, AbortSignal cleanup and cancelable events.
- Exclusive button ownership, repeatable close and stale-handle isolation.
- One hundred button open/close cycles.
- Two hundred retained objects with intact contents, followed by bounded
  allocation/reuse and a responsive REPL.
- Ten-second idle uptime progress without a reset.

The bounded physical input recording captured ten valid press/release pairs,
including 2.19- and 2.81-second holds without repeat events during those holds.
There were no reported sampler errors or invalid event targets. The operator
confirmed the LED response. The polling handle closed cleanly and returned its
descriptor to idle; the LED was turned off. This does not establish precise
interrupt-level debounce timing or delivery of arbitrarily short taps.

## Other boards and remaining work

Compilation is distinct from physical qualification. These results do not
qualify XIAO input, every display adapter, or RP2350 physical behavior.

PiZero candidate QA was stopped before flashing because the installed flash
returned inconsistent bytes even in cold ROM BOOTSEL mode. The original
application and files were preserved and verified. Investigation is tracked in
[issue #18](https://github.com/mcu-js/mcujs/issues/18), separately from this RAM
and runtime checkpoint.

The remaining [issue #8](https://github.com/mcu-js/mcujs/issues/8) slice is Canvas
touch/pointer input, not another button or peripheral abstraction.

## Upgrade preservation

Before pre-1.0 upgrades, back up user files and establish a verified full-flash
rollback path. RP2 currently places its filesystem immediately after the
aligned firmware end: a different-size application can relocate or overlap
storage. Do not assume an application-only UF2 preserves the old filesystem;
obtain approval for any necessary recreation and restore files from verified
backups. A successful upload does not prove safe startup or data preservation.
