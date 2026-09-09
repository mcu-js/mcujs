# Seeed reTerminal Sticky — experimental MCU.js target

Board ID `seeed_reterminal_sticky`; ESP32-S3, 8MB octal PSRAM, 32MB quad flash.
Not a release-qualified target and excluded from shipping board packaging.
`version.txt` remains 0.1.0. The standard JavaScript Canvas API is unchanged.

## Hardware and source provenance

- Seeed hardware overview: https://www.seeedstudio.com/sticky/docs/en/device-guide/hardware-overview/
- Primary reference: https://files.seeedstudio.com/wiki/reterminal_sticky/res/Sticky_dashboard_demo.zip
  SHA256 `f52918c42411f73375db1125b4c5ee0fc8691d8b8523c26d3c8aab7e8a256101`.
  `main/pin_config.h`, `main/board/board.cpp`, `main/devices/sticky_display.cpp`,
  and `components/seeed_epaper/driver/ssd1677.c` specify wiring, rail levels,
  full-update sequence and landscape orientation (rotate180 then mirrorX).
- Independently compared SSD1677 full sequence with
  `Lukilyy/reterminal-sticky-2048-eink-game` commit
  `c2ed847c3654085299d74ef42f7e1a4d760a5b53`, same driver path.

Private EPD pins: MOSI14 SCK13 CS15 DC16 RESET17 BUSY18(high busy), EN47(active
high). Power hold45 and lock46 high; unused SD10, microphone38, touch41/42 and
buzzer48 stay low. Charger39 is untouched. No public GPIO, touch, sensors,
audio, SD, Wi-Fi or BLE implementation is advertised in this first slice.

## Memory and display

Sticky uses a 128KiB JavaScript heap (the 64KiB bring-up heap exhausted on the
full artwork with live runtime bindings) and a configured 240MHz CPU. Other
boards keep their existing JS heap and CPU settings.

One 768000-byte RGB565 surface is explicitly allocated from PSRAM. No fallback
into insufficient internal SRAM. PSRAM must initialize successfully. The ctx
0.1.18 profile permits 800x480/coordinates +/-1024 only for Sticky; smaller-board
limits and scratch sizes stay unchanged. Each dirty JS task is presented once.

Full monochrome only: convert rows to 1bpp, send both SSD1677 planes, use the
vendor OTP full waveform (0x22/F7), wait with a 10-second BUSY deadline, deep
sleep then remove panel power. SPI polling sends <=50-byte pieces; the PSRAM
surface is never passed to DMA. No partial/grayscale promise in this target.
Timeout/transport errors close the display and remove its rail.

## Serial, boot and recovery

The onboard bridge uses UART0 TX43/RX44 at 115200. The private console ABI is
implemented by `serial_uart.c`; TinyUSB CDC/MSC are disabled and never started
on the microphone pins19/20. Filesystem ownership stays on the device, allowing
existing fs APIs and /index.js. No USB mass-storage drive or TinyUF2. UART
connection state means initialized transport, not detected host attachment.

The USB-UART bridge supports esptool default_reset/hard_reset without physical
button input on the tested device. Verify bridge and chip identities before any
write. Preserve a full32MB backup twice with matching hashes, and validate the
security state. Do not erase the chip or transplant XIAO/Waveshare firmware.

Initial layout preserves factory NVS9000/7d000 and PHY88000/1000. MCU.js uses
factory app690000/600000 (the original app1 slot) and FFAT1c00000/400000.
Both regions must be verified unused/erased before this layout is provisioned.
Original app0 bytes remain intact but the MCU.js table no longer selects them.
Replace bootloader/table only after backup; restore the original full image to
return completely to factory. Existing NVS is not erased or reset automatically.
No custom battery shutdown/deep-sleep UI is qualified in this slice.

## Pinned Docker build

Use the existing ESP Docker image pinned by digest, source read-only, output
writable, no network, no capabilities and a writable ephemeral /tmp copy:

```
MCUJS_BOARD=seeed_reterminal_sticky
MCUJS_EXPERIMENTAL_CANVAS=1
MCUJS_BUILD_GIT_SHA=<exact-source-SHA>
CTX_PATH=/opt/ctx
```

Invoke `platform/esp32/docker-entrypoint.sh build` through the existing container
lane. ctx.h hash is pinned in CMake. Outputs are BIN/ELF/MAP, bootloader,
partition table, flash metadata and board capability manifest — not UF2.
`build.sh` validates app-only offset690000, 32MB flash settings, disabled native
USB and required octal PSRAM settings. Start with a clean build directory.

Native tests cover full-frame far-edge rendering, literal byte orientation,
BUSY/transport failures and power-off. Hardware acceptance must separately prove
embedded build identity, PSRAM startup/allocation, UART file roundtrip, a real
completed presentation, reset/startup and continued watchdog-clean operation.
