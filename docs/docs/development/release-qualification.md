---
title: Per-target release qualification
---

# Per-target release qualification

This is the compact protocol for [#29](https://github.com/mcu-js/mcujs/issues/29),
not a hardware result. [#30](https://github.com/mcu-js/mcujs/issues/30) freezes the
candidate; [#31](https://github.com/mcu-js/mcujs/issues/31) executes this protocol
on those exact bytes; [#32](https://github.com/mcu-js/mcujs/issues/32) publishes
only after disposition. Completing this document authorizes **no device action**.
Do not add a runner, scheduler or parallel evidence format.

## 1. Freeze the inventory, not an old board count

In the clean checkout of the frozen RC, run these host-only commands from the
repository root and retain the output with the qualification record:

```sh
git rev-parse HEAD
git status --porcelain
bash -c 'source scripts/lib/boards.sh; printf "%s\n" "${MCUJS_RELEASE_BOARDS[@]}"'
```

Reconcile that list with every `board=` row in the unpacked package's
`RELEASE_MANIFEST.txt`, its `git_sha`, and the paired UF2/capability sidecars.
From the unpacked package directory run `sha256sum -c SHA256SUMS.txt`.
Require a clean checkout, matching frozen source/build identity, all checksums
valid, and exactly one firmware/sidecar pair per target. Stop on disagreement;
a filename, version label or successful upload alone is not identity evidence.
Archive build configuration/feature flags too: physical inventory does not mean
an optional driver was compiled. Do not rebuild between target runs and call it
the same candidate. Record any different artifact/profile separately.

Current baseline: **11 release-package targets**, in `scripts/lib/boards.sh` order.
Create one copy of the record below for **each**, including unavailable boards:

- `pico` — RP2040.
- `pico2` — RP2350.
- `pico2_w` — RP2350; managed CYW43 LED is not a GPIO LED.
- `waveshare_rp2040_zero` — RP2040; onboard NeoPixel, not a plain LED.
- `waveshare_rp2040_pizero` — RP2040; DVI hardware needs its own output evidence.
- `waveshare_rp2040_touch_lcd_1.28` — RP2040; panel inventory is not touch support.
- `waveshare_rp2350_lcd_1.47_a` — RP2350; distinguish optional SD from `/app`.
- `waveshare_rp2350_touch_lcd_1.69` — RP2350; verify configured input separately.
- `waveshare_rp2350_touch_lcd_2.8` — RP2350; no declared external GPIO/LED/button;
  panel hardware is listed, Canvas is experimental opt-in, touch/SD/sensors are
  not supported by the initial port. Do not widen its default image to get a pass.
- `adafruit_feather_rp2040` — RP2040; LED and onboard NeoPixel are distinct.
- `seeed_xiao_esp32s3` — ESP32-S3; onboard LED is outside the PWM allowlist;
  compatible TinyUF2 application update, not RP ROM transport.

**Experimental profiles, outside that package list:** `seeed_reterminal_sticky`
and `waveshare_esp32s3_epaper_1.54_v2`. If tested, give each its own record and
artifact/config identity. Do not treat them as XIAO: Sticky has no advertised
USB host transfer or UF2 entry; ePaper V2 does not advertise UF2 entry either.
Their outcomes do not silently add/remove a release target. Reconfirm inventory
at freeze and document any maintainer-approved scope change explicitly.

## 2. One record per target/profile

Use this as an issue comment or ordinary Markdown attachment in #31, not a new
service or schema. Evidence can be files/links; retain raw originals privately
when they contain user files, identifiers or credentials. Public evidence uses
a stable redacted device identifier and hashes, not raw flash/user data.

```text
Target / profile:
RC source SHA / build configuration / UF2 filename and SHA256:
Package manifest and capability sidecar SHA256:
Physical board revision / chip / flash / stable device identifier:
Attachment host / serial and disk identity across runtime and recovery modes:
Operator / date / board-scoped authorization (actions, files, pins, power changes):
Previous build / recovery image hash / backup hashes and restore procedure:
Fixture / wiring photo / instruments, settings and calibration:
Q1 identity + preservation + update: NOT_RUN — evidence/reason
Q2 discovery + onboard inventory: NOT_RUN — evidence/reason
Q3 serial conformance + example lifecycle: NOT_RUN — evidence/reason
Q4 ADC / PWM / I2C / SPI / NeoPixel: one row per protocol case/route — NOT_RUN
Q5 storage ownership + persistence: NOT_RUN — evidence/reason
Q6 reconnect + idle/workload responsiveness: NOT_RUN — evidence/reason
Q7 startup + physical safe boot + recovery: one row per reset/power mode — NOT_RUN
Q8 restoration + final state: NOT_RUN — evidence/reason
Defects / retests / explicit maintainer waivers (target, case, reason, RC hash):
Reviewer / release disposition:
```

Every subcase gets exactly one status:

- **PASS** — this exact candidate was observed satisfying the stated criterion,
  with linked raw evidence. A group is PASS only if every applicable subcase is.
- **FAIL** — observed mismatch, unsafe behavior, data loss or unexpected reset.
  Preserve the failing trace; stop dependent work and track the defect.
- **NOT_RUN** — missing board, instrument, safe wiring/access, authorization or
  observation. A supported route that cannot safely be reached is NOT_RUN.
- **NOT_APPLICABLE** — the frozen contract/scope does not expose this feature;
  cite the descriptor/configuration and verify truthful absence where possible.
  A broken advertised feature is FAIL, not NOT_APPLICABLE.

Map serial `skip` records to NOT_RUN unless a specific contract-based
NOT_APPLICABLE reason is established. A waiver never changes the measured status.
Missing hardware/instruments require an **explicit maintainer waiver for release**
identifying the target, cases, exact candidate, rationale and accepted risk.
No implicit waiver, invented pass or unilateral target removal. FAIL blocks
acceptance until fixed/retested or explicitly dispositioned by the maintainer;
retain the failure even then. A changed binary gets a new candidate record and
applicable retests, not inherited physical passes from an older build.

## 3. Execute Q1–Q8 in order, one authorized board at a time

### Q1 — inspect, back up, prepare rollback, update

1. Obtain separate board-scoped approval for firmware writes, application files,
   wiring and reset/power/recovery tests. Bind serial and disk paths to the actual
   board across modes; identical `MCUJS` labels are not sufficient. Stop if the
   connected board, revision, flash layout or recovery transport is uncertain.
2. Capture the installed `.info` and `.help`, photographs of board/controls,
   current startup files and safe-mode setting (where advertised). Back up all
   application files; hash and independently verify the copies. Keep two matching
   full-flash recovery reads for RP upgrades; stop on inconsistent reads. Store
   backups outside the checkout. Never print credentials from user files.
3. Write the exact rollback commands/transport for this board in the record
   **before** updating. Include the verified known-good artifact, original layout
   and file-restore method. For ESP preserve bootloader, partition, recovery,
   NVS and user-storage regions; establish a verified compatible recovery baseline.
   If backup/rollback cannot be established, mark NOT_RUN and do not flash.
4. Follow [application upgrade preservation](./app-namespace.md#explicit-upgrade-procedure)
   and [board-specific recovery](../debugging-recovery.md#board-specific-safe-boot-and-recovery).
   RP firmware-size changes can relocate storage: authorize any recreation and
   restore separately. XIAO uses its compatible application-only TinyUF2 path;
   do not substitute stock full-flash, repartition or erase commands. Experimental
   ESP profiles require their own documented recovery plan, not the XIAO plan.
5. Apply only the approved matching artifact. Save the updater log, verify written
   bytes/readback where the transport permits, then independently capture runtime
   `.info` and reconcile it to the full RC SHA and artifact hash. Verify original
   files after any necessary restore. A disconnect during copying is neither proof
   of success nor permission to retry blindly. Wrong identity or lost files is FAIL.

### Q2 — discovery and onboard inventory

Capture `.info`, `.help` and the following non-driving REPL observations:

```js
JSON.stringify(require('board').capabilities())
JSON.stringify(require('board').devices)
JSON.stringify(require('mcujs:module').builtinModules)
```

Compare live capabilities/exports, `modules.has()`, actual require-ability and
independently captured help using the [serial conformance procedure](./portable-api-conformance.md#serial-hardware-evidence).
Resolve pins/routes/limits from that build's descriptor, then check physical
access and schematic before driving them. Do not substitute board-name pin tables.

For each advertised onboard device, record configured support separately from
physical presence. Run applicable maintained examples twice, let them finish,
and verify stop/close and handle reopen. For an LED, photograph off/on/off. For a
button, capture ten deliberate press/release pairs with timestamps (including a
two-second hold), no extra repeat events during the hold, and cessation after
close. Button events do not prove a safe-boot gesture.

For configured display support, use `devices.display.open()` and
`display.canvas.getContext('2d')`; follow the [display](./display-canvas.md) and
[pointer](./canvas-pointer.md) acceptance steps with labeled before/after images
and input traces. Check edges, orientation, colors and persistent rerun cleanup.
DVI needs the connected monitor in the evidence. Canvas-off means no rendering
pass, even on a board with a panel. Audio/SD remain explicitly experimental, but
**any optional feature enabled in a shipping artifact still needs its own cases**
and evidence or explicit maintainer disposition; the label is not a waiver.
For configured WAV output, repeat the [speaker contract](../speaker.md#contract)
and bounded playback/stop/close/reopen checks with serial logs and acoustic
recordings. For configured SD, use the [asset demonstration](./sd-assets.md#evidence-required-for-the-asset-demonstration),
respect read-only profiles and power-off card swaps, and retain byte comparisons
plus A/B display evidence where rendering is supported. Record these separately
from shipping-core claims; never inherit the historical passes in those pages.
A static e-paper image does not prove a live MCU.

### Q3 — serial contract and bounded example lifecycle

Use the existing [serial probe](./portable-api-conformance.md#serial-hardware-evidence),
not a new harness: copy `tests/conformance/serial-hardware-probe.js` from the RC
checkout to an approved unused path such as USB-root `serial-hardware-probe.js`,
eject host storage, confirm `require('board').storageReady()`, then run
`.run /app/serial-hardware-probe.js`. Preserve any pre-existing file first.
The first run is discovery-only, **not a full suite pass**. Configure the existing
schema-derived plan, independent help list and approved safe callbacks by stable
case ID. Retain the RS/US-framed records and validator output; ignore command echo
and prompts. Unregistered cases stay skipped; do not enable them blindly.

Use the [maintained example inventory](https://github.com/mcu-js/mcujs/blob/development/examples/README.md)
from the RC checkout: hello, discovery, storage ownership, bundled config/modules,
and applicable LED/button/PWM/display/pointer entries. Preserve sibling imports
and approved destination files. Run each twice in the persistent realm, record
completion/stop markers, no continuing callbacks after its documented duration,
and release/reopen of owned handles. Do not use a reset to hide rerun leaks.
Save submitted JavaScript hashes and full serial output. Host/native conformance
and allocation-failure tests are complementary, never physical evidence.

### Q4 — reuse the existing peripheral procedures

For **each target**, select every applicable case/route from these procedures.
Their exact limits and evidence requirements remain authoritative; do not copy
weakened thresholds into the result. Record NOT_RUN for unavailable equipment or
unsafe access, rather than guessing a reserved pin or manufacturing a fault.

- [ADC](./adc-voltage-protocol.md): measured reference voltages, raw sample sets,
  pin/channel statistics and stated tolerances; calibrated meter/source and wiring.
- [PWM](./pwm-waveform-protocol.md): analyzer frequency/duty/rail measurements,
  shared-output survivor captures and rejection/lifecycle cases. Visible fading
  alone does not prove a waveform. XIAO LED exclusion does not skip external PWM.
- [I2C](./i2c-peripheral-protocol.md): isolated `0x42` target emulator, common ground,
  rated pull-ups, ACK/NACK, exact payload/count and SCL captures. No-peripheral
  scans or arbitrary sensor register writes are not transfer-boundary evidence.
- [SPI](./spi-loopback-protocol.md): isolated MOSI/MISO loopback plus analyzer,
  scalar/array byte equality, modes and clock/length boundaries. Floating MISO
  does not prove full duplex. Never connect competing outputs.
- [NeoPixel](./neopixel-hardware-protocol.md): rated supply/shifter, color photos,
  component-order/timing captures and maximum-load USB/watchdog health. A
  disconnected strip proves only the specifically observed software layer.

These protocols operate on an already-installed image. Their no-flash/no-storage-
write boundaries remain intact; Q1/Q5/Q7 are separately authorized operations.
SDK-stub fault injection does not authorize electrical shorts or driver corruption.

### Q5 — exclusive storage ownership and persistence

With an approved unused scratch filename and verified original-file inventory:

1. While the host owns `MCUJS`, retain stable nonzero capacity and successful CDC
   responses. A device `fs.readFileSync('/app/<scratch>', 'utf8')` must report
   `EBUSY`, not fabricated empty data; capture the exact error.
2. On the host, create scratch content with a unique recorded token, rename it,
   sync and **eject** (unmount alone may not transfer ownership). Require
   `board.storageReady()` true before `.ls`, `.cat` or runtime file calls. Confirm
   exact token bytes through `fs.readFileSync` at the logical `/app` path.
3. Device-write a second token with `fs.writeFileSync`, close any binary handles,
   then use the recorded supported load/reconnect sequence to reacquire host
   ownership. Verify the second token on the host. Do not access FAT concurrently.
4. Sync/eject and software restart, reconnect, eject again, then device-read the
   token. Repeat three full handoff cycles; preserve serial/host logs, hashes and
   capacity observations. Follow Q7's separate cold-start row for power persistence.
5. Compare all original-file hashes again. Never format to cure `EBUSY`. If this
   profile has no host transfer, mark only host-handoff cases NOT_APPLICABLE and
   still check device-side persistence through its approved console. Optional
   `/sd` is not a fallback for an unavailable `/app`.

### Q6 — reconnect, watchdog and responsiveness

After safe sync/eject, perform three USB disconnect/reconnect cycles; record mode,
capacity, identity and prompt after each. Separately observe 60 seconds idle and
60 seconds of an applicable bounded workload (use the peripheral protocol's longer
window if required). Send a uniquely labeled arithmetic expression through CDC
once per second, retaining sent/received timestamps and actual result lines.
Require every response, advancing uptime where available, no unexpected reset,
watchdog message or spontaneous disconnect. Exclude command echo as evidence.
Record measured latency without inventing a universal timing guarantee. If no CDC
is configured, mark CDC-specific rows NOT_APPLICABLE and test responsiveness over
the profile's supported console. Do not confuse reconnect with power loss when
another supply or battery remains attached.

### Q7 — startup, physical safe boot and recovery

Back up `/app/index.js` and every affected sibling before replacement. Use approved
scratch content only; USB root is `/app`, not an extra `app/` directory. Qualify
these separately and retain exact files, logs and operator observations:

1. **Normal startup:** install a bounded startup entry with a relative sibling
   import and a unique marker; software restart and confirm automatic execution
   without `.run`. Execution precedes MSC exposure, so attach the collector before
   restart or use a verified persisted marker after eject rather than missing boot
   output. Check original files plus the Q5 token.
2. **Cold start:** sync/eject, safely remove all power sources according to this
   board's instructions, then power without a USB host claiming storage. Observe
   a unique boot marker via an independent console or an approved persisted boot
   count read after reconnect/eject. Photograph any configured visible indicator;
   it supplements, not replaces, evidence that the startup actually ran. Verify
   scratch-token persistence. Record power sources and elapsed unpowered time.
3. **Throwing startup:** install an entry that deliberately throws a labeled
   error. Require usable REPL recovery without formatting; preserve the error.
   Where `board.capability('boot').safeMode` is true, record initial state, reset
   again, require skipped startup/safe mode, repair the entry, then clear safe mode
   only outside its qualification window. Capture `EBUSY`/`EIO` rather than erase
   NVS or bypass a failure. Restore the initial setting in Q8.
4. **Physical recovery:** record the exact board-manual gesture and observed USB/
   console mode before attempting it. On RP, held BOOTSEL at power-up can enter
   ROM (`RPI-RP2`/`RP2350`), **not** the MCU.js REPL or filesystem. Prove ROM entry
   and the prepared approved route back to the same RC; separately record whether
   the runtime's physical safe-boot gate was exercised. On XIAO, distinguish ROM
   BOOT/download from compatible `XIAOS3BOOT` TinyUF2 and from persistent runtime
   safe mode. A software `.uf2!` result is not evidence of physical button recovery.
   If the intended gesture cannot be safely performed/observed, mark NOT_RUN.
5. For profiles whose frozen startup contract promises watchdog recovery from a
   non-returning entry, exercise that **only with explicit fault-test approval and
   a proven independent recovery path**. Require the reset and subsequent skipped
   startup; retain watchdog/boot logs. Do not install a busy loop on RP merely to
   imitate ESP behavior. Missing approval/recovery evidence leaves this NOT_RUN.

### Q8 — restore, verify, disposition

Stop/close only test-owned resources, leave outputs safe, remove only test-created
files, restore replaced originals and initial safe-mode settings, then verify the
entire original-file inventory and hashes. Record the final firmware, storage
ownership, power/connection state and responsive prompt. If rollback was needed,
verify both recovered firmware identity and user files; keep the candidate FAIL
and label the board's final state. A prepared but unused rollback is not an
executed rollback pass.

Review every target/subcase and evidence link in #31. Carry known NOT_RUN areas
from earlier checkpoints forward: #28's three-board example runs do not qualify
all targets or this RC, Canvas-on rendering, physical presses, electrical buses,
held-button recovery or charger-powered cold start. Keep experimental omissions
and maintainer waivers visible. Only the explicit per-target disposition authorizes
moving toward publication; writing or merging this checklist does not.
