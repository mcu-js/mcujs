# Waveshare ESP32-S3-ePaper-1.54 V2 — development profile

Not a published release or a fully hardware-qualified port. Target `waveshare_esp32s3_epaper_1.54_v2`.
N8R8: 8 MB flash, 8 MB octal PSRAM. Native CDC+MSC/FFAT runtime; no public GPIO,
SPI, I2C, LED, audio, touch, RTC, SD or sensor support. Pins are reserved until qualified.
XIAO remains the default ESP target with its original partitions/recovery.

## Provenance
Primary discovery: https://docs.waveshare.com/ESP32-S3-ePaper-1.54/Resources-And-Documents
and https://docs.waveshare.com/ESP32-S3-ePaper-1.54/ESP-IDF .
Vendor repository https://github.com/waveshareteam/ESP32-S3-ePaper-1.54,
commit `9957d0f4fc7cd40d1d42880cb1b74a8d6782a6c2`:
`02_Example/ESP-IDF/V2/07_BATT_PWR_Test/`:
- `main/user_config.h`: SPI2; DC10 CS11 SCK12 MOSI13 RESET9 BUSY8; EPD power GPIO6.
- `components/board_power_bsp/board_power_bsp.cpp`: power **active low**.
- `components/epaper_driver_bsp/epaper_driver_bsp.cpp`: BUSY **high**; full init,
  159-byte full LUT, 0x24 pixel RAM and 0x22/C7 full update. LUT matched byte-for-byte.
- Panel datasheet: https://files.waveshare.com/wiki/common/1.54inch_e-paper_V2_Datasheet.pdf
  sections 4.2 and 5 verify 0x10/01 deep sleep, hardware reset to wake,
  MSB-first pixels, 1=white and 0=black. Controller inventory uses vendor panel
  name rather than claiming a silicon marking not independently observed.
Artifacts downloaded with urllib Mozilla UA and HTMLParser link discovery are in
local investigation storage outside the repository (not vendored dependencies).

## Canvas — development defaults

Normal builds of this V2 profile enable Canvas, the existing vendor partial
waveform, packed-image duplicate suppression and screen-space Bayer dithering.
Other ESP profiles retain their previous defaults. The public path is unchanged:
`require('devices').display.open()` returns a handle with `display.canvas` and
`display.present()`. The canvas is 200x200, using unchanged
`host/bindings/canvas_renderer.c` and ctx 0.1.18. `CTX_PATH/ctx.h` SHA256 must be
`bf2b18bcea31bf191f7e82c9beef98bd717bd5ccd635908f07ab8a8cf1698bb3`.

Weighted RGB565 luminance is converted to MSB-first monochrome, 1=white, with a
fixed screen-space 4x4 Bayer matrix. Uniform shades can cover 0 through 16 of the
16 sites; these are monochrome dots, not native panel grey levels. Pure black
and white remain exact. Moving geometry does not move the dither phase, and the
source RGB565 surface is not modified.

Before touching power, reset, SPI or BUSY, the driver compares the complete
5000-byte packed candidate to the last **successful** image. An identical image
returns success without any panel activity or consuming the partial budget,
even if RGB565 values changed or a cleaning refresh is due. First presentation,
including an all-white frame, always refreshes. History is committed only after
BUSY completion, sleep and rail-off; any failed attempt invalidates it. Close
and reopen start fresh. Normal builds contain no `EPAPER_AB` diagnostic logging.
Generic successful-presentation counters include these skips; they are not
physical refresh counts or visible FPS.

### Vendor partial waveform and escape hatches

The existing `WF_PARTIAL_1IN54_0` LUT and `0x22/CF` sequence are unchanged. This
is a partial **waveform**, not cropped SPI transfers: changed images still send
a complete 5000-byte monochrome frame. First update is full; at most four
successful changed updates are partial, then the next changed update is a full
cleaning refresh. Both controller RAM planes are restored from the successful
base after power-on before a partial update. No new waveforms or pin changes.

The default PSRAM allocation is 90000 bytes: the original 80000-byte RGB565
surface plus a 5000-byte successful history and a 5000-byte candidate. There is
no second RGB565 framebuffer. Each actual update resets/wakes, uploads,
refreshes, waits (15-second BUSY timeout), sleeps and disables panel power. SPI
remains 4 MHz; USB and watchdog are serviced while BUSY. Errors cut power and
force full recovery on the next driver update; the public handle closes on a
presentation failure and must be reopened.

The existing build-option names remain available. Each accepts environment
`1`/`0` or CMake `ON`/`OFF`; environment 0/1 takes precedence:

| Option | V2 Canvas-enabled default | OFF behavior |
| --- | --- | --- |
| `MCUJS_EXPERIMENTAL_EPAPER_PARTIAL` | ON | Full waveform for every changed image |
| `MCUJS_EXPERIMENTAL_EPAPER_DIFF` | ON | No duplicate suppression; 85000 bytes with partial, otherwise 80000 |
| `MCUJS_EXPERIMENTAL_EPAPER_DITHER` | ON | Original weighted-luminance threshold 128 |
| `MCUJS_EXPERIMENTAL_CANVAS` | ON | No configured Canvas display; fresh feature defaults OFF |

For the original full-refresh/threshold path, set PARTIAL, DIFF and DITHER to
`0` together. For Canvas-off, disable any previously cached feature options too.
Use a clean build directory when changing boards or validating defaults; CMake
preserves explicit/cached options. Explicit ePaper features with Canvas off or
on another board fail configuration.

### Retained experiment evidence, not new-build qualification

The matched instrumented PRE and POST both used the same vendor partial
waveform and four-partial cleaning policy. In one synchronous duplicate-blink
run, POST skipped 8 of 16 requests: elapsed time was 20.690s PRE versus 10.775s
POST. Changing-frame moving/shading refresh rates were effectively unchanged
under those matched flags; duplicate suppression is not a faster waveform.

A separate exact-original firmware replay lacked native refresh telemetry and
used its original configuration. Moving/blink/shades synchronous durations were
24.067s/31.972s/16.123s versus POST 15.688s/10.775s/10.536s. These are replay
durations, not measured visible FPS, and must not be confused with the matched
PRE comparison. The operator observed distinct POST shades and selected POST.
That physical evidence belongs to the retained experiment, not newly built
upstream firmware. Quantitative visible FPS, ghosting, endurance and exact
upstream-build hardware qualification remain open. The board is still
experimental and outside the published release package.

## Parent-owned Docker build / provisioning
Use the pinned image supplied by the parent; no host SDK installs. Mount source
read-only at `/source`, an empty output directory at `/output`, and the ctx header
directory at `/opt/ctx`. Environment:
```
MCUJS_BOARD=waveshare_esp32s3_epaper_1.54_v2
MCUJS_EXPERIMENTAL_CANVAS=1
MCUJS_BUILD_GIT_SHA=<actual-source-SHA>
CTX_PATH=/opt/ctx
```
Invoke `bash /source/platform/esp32/docker-entrypoint.sh build` in that container.
The wrapper `docker-build.sh` has not been qualified for this board; invoke the
entrypoint directly with these mounts/env. Outputs include app BIN/ELF/MAP,
bootloader.bin, partition-table.bin, and flasher_args.json. No UF2 is generated.
A clean build directory is mandatory when changing boards.

Partition layout: NVS 0x9000/0x6000; PHY 0xf000/0x1000;
factory application 0x10000/0x3f0000; FFAT 0x400000/0x400000.
After a verified full 8 MB backup, parent may selectively write bootloader at
0x0, partition table at 0x8000, application at 0x10000. Confirm generated flash
metadata before writing. Do not erase/write old NVS or PHY. Do not restore old
factory app tails over FFAT. First runtime boot formats FFAT as needed.
TinyUF2 recovery is explicitly unavailable: `board.enterUf2` and the
`boot.enterUf2` capability field are absent. The
shared REPL board service returns failure, never a XIAO loader reset hint.
Use BOOT held during a PWR off/on cycle to enter Espressif ROM. After flashing, release BOOT and cycle PWR normally if USB reset leaves the board waiting for download.

## Verification boundaries
Host tests cover conservative board discovery, the shared Jerry/Canvas path,
RGB565-to-monochrome conversion, frame order, BUSY timeout, power-off, sleep,
transport failure and close. Vendor LUT bytes were independently compared.
These tests do not prove physical orientation, ghosting or electrical behavior;
those require the actual panel and an exact firmware build. This board remains
outside shipping/release packaging; its registry and capability manifest are
still generated and checked. Do not install a V1 or XIAO recovery image.
