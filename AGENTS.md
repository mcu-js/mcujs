# mcujs Agent Instructions

## Project Principles

- mcujs is a JavaScript runtime for microcontrollers in the same spirit as Node.js for servers.
- JerryScript is the current JavaScript engine, analogous to V8 in Node.js.
- The engine is swappable: keep runtime boundaries clean to allow future interpreter changes.
- The firmware targets Raspberry Pi Pico boards today, but should remain extensible to other MCUs with UF2 boot support.
- Favor composable, modular code so core systems can be replaced or extended without large rewrites.

## Development Cycle (Flash + Test Loop)

- Default workflow: `build.sh <board>` then flash the UF2 and run REPL tests.
- The runtime can enter UF2 mode via `.uf2!` or `board.enterUf2()` over serial.
- When automating, confirm device state:
  - UF2 mode if the `RPI-RP2` (RP2040) or `RP2350` (RP2350) volume is mounted.
  - Runtime mode if a CDC serial device (e.g. `/dev/ttyACM*`) is present and responds to the REPL prompt.
- Prefer an automated loop that:
  - Builds firmware.
  - Reboots into UF2 mode from the REPL.
  - Copies the UF2 to the mounted volume.
  - Waits for CDC to reappear and runs REPL tests.
- Keep tooling minimal and cross-platform. Node/Bun-based harnesses are preferred over Python.

## Device Detection and Mounting

### Identifying Devices

When multiple boards are connected, identify them by connecting to each serial port and running the `.info` command. The `Board:` line in the output shows which board is on that port.

### Block Device Detection

Check device state and labels:

```bash
# List block devices with labels
lsblk -o NAME,LABEL,SIZE,FSTYPE

# UF2 mode shows as: RPI-RP2 (RP2040) or RP2350 (RP2350) - 128M, vfat
# Runtime mode shows as: MCUJS (~1.4-15.4M depending on flash size, vfat)
```

### Mounting Volumes

Use `udisksctl` for non-root mounting:

```bash
# Mount a volume (auto-detects mount point)
udisksctl mount -b /dev/sda1

# Unmount before entering UF2 mode or formatting
udisksctl unmount -b /dev/sda1

# Mount points appear at /run/media/$USER/<LABEL>
```

### Entering UF2 Mode

```bash
# Via REPL command (requires serial connection)
python3 -c "
import serial, time
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
ser.write(b'\r.uf2!\r')
ser.close()
"

# Wait for UF2 volume to appear (RPI-RP2 for RP2040, RP2350 for RP2350)
sleep 3
lsblk -o NAME,LABEL | grep -E "RPI-RP2|RP2350"
```

### Flashing Firmware

```bash
# Mount UF2 volume and copy firmware
udisksctl mount -b /dev/sda1
cp build/mcujs-0.1.0-<board>.uf2 /run/media/$USER/RPI-RP2/
sync

# Device auto-reboots after UF2 copy - wait for CDC to reappear
sleep 3
ls /dev/ttyACM*
```

### Filesystem Commands

The runtime provides REPL commands for filesystem management:

- `.ls` - List files
- `.cat FILE` - Display file contents
- `.rm FILE` - Delete a file
- `.format` - Format filesystem (prompted, 3s countdown)
- `.format!` - Format filesystem immediately

The filesystem auto-formats on first boot or if corruption is detected.

### Expected Filesystem Sizes

Filesystem size = Flash size - Firmware (~550KB) - EEPROM reservation (4KB):

| Board | Flash | Expected FS |
|-------|-------|-------------|
| pico | 2MB | ~1.4MB |
| pico2 | 4MB | ~3.4MB |
| waveshare_rp2040_zero | 2MB | ~1.4MB |
| waveshare_rp2040_touch_lcd_1.28 | 4MB | ~3.4MB |
| waveshare_rp2350_lcd_1.47_a | 16MB | ~15.4MB |
