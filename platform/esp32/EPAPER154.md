# Waveshare ESP32-S3-ePaper-1.54 V2 — experimental bring-up

Not a published release or a hardware-qualified port. Target `waveshare_esp32s3_epaper_1.54_v2`.
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

## Canvas
Build opt-in `MCUJS_EXPERIMENTAL_CANVAS=1`. Existing `require('canvas').display`
is 200x200, using unchanged `host/bindings/canvas_renderer.c` and ctx 0.1.18.
`CTX_PATH/ctx.h` SHA256 must be
`bf2b18bcea31bf191f7e82c9beef98bd717bd5ccd635908f07ab8a8cf1698bb3`.
RGB565 surface in PSRAM is converted to MSB-first monochrome by weighted luminance
threshold 128. No application show/present API: engine presents after execution
or timer processing only if dirty. Each update resets/wakes, uploads one full
image, refreshes, waits (15-second BUSY timeout), sleeps, and disables panel power.
SPI runs conservatively at 4 MHz instead of vendor's 40 MHz. Timeout closes the
surface and cuts power; USB and watchdog are serviced while waiting. This is not
an animation implementation in the default build.

### Opt-in partial-waveform experiment

`MCUJS_EXPERIMENTAL_EPAPER_PARTIAL=1` additionally enables the vendor V2
`WF_PARTIAL_1IN54_0` LUT and `0x22/CF` update sequence. The application still
uses ordinary Canvas drawing. This is a partial **waveform**, not cropped SPI
transfers: a complete 5000-byte monochrome image is sent.

First presentation is full; at most four subsequent presentations are partial,
then a full cleaning refresh is forced. A 5000-byte previous-image cache is
stored after the existing RGB565 surface in the same PSRAM allocation. It
restores both controller RAM planes after power-on before the next partial
update. There is no second RGB565 framebuffer. Every update still sleeps and
cuts panel power; errors invalidate the base image and force the next update
through the full path. These conservative choices favour a bounded experiment
over maximum refresh speed. Ghosting, timing and long-term endurance require
physical qualification; do not treat this as a production refresh policy.

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
TinyUF2 recovery is explicitly unavailable: `board.enterUf2()` throws and the
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
