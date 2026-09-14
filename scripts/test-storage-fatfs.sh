#!/usr/bin/env bash
# Actual FatFs R0.16 on both sides of actual fs.c + RP2 MSC callbacks.
# No block devices, mounts, SDK, Docker, privileges or hardware involved.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/mcujs-fatfs-roundtrip.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
# The Dockerfile is the dependency authority. An offline archive is allowed but
# must match the same digest; nothing is vendored or trusted by pathname alone.
python3 - "$ROOT" "$TMP" "${FATFS_ARCHIVE:-}" <<'PY'
import hashlib, io, pathlib, re, sys, urllib.request, zipfile
root, tmp = map(pathlib.Path, sys.argv[1:3])
expected = re.search(r'^ENV FATFS_SHA256=([0-9a-f]{64})$', (root/'Dockerfile').read_text(), re.M).group(1)
data = pathlib.Path(sys.argv[3]).read_bytes() if sys.argv[3] else urllib.request.urlopen('https://elm-chan.org/fsw/ff/arc/ff16.zip', timeout=60).read()
actual = hashlib.sha256(data).hexdigest()
if actual != expected:
    raise SystemExit(f'FatFs checksum mismatch: {actual} != {expected}')
print(f'FatFs R0.16 archive SHA-256 verified: {actual}')
with zipfile.ZipFile(io.BytesIO(data)) as z:
    for name in ['ff.c', 'ff.h', 'diskio.h', 'ffunicode.c']:
        (tmp/name).write_bytes(z.read('source/'+name))
PY
CC="${CC:-cc}"
FLAGS=(-std=gnu17 -Wall -Wextra -Werror -O1 -g -DMCUJS_HAS_SD=1 -DMCUJS_USB_SD_MSC=1
    -I"$TMP" -I"$ROOT/src/filesystem" -I"$ROOT/src/usb" -I"$ROOT/tests/native_stubs/usb")
# Optional sanitizer/compiler flags (e.g. -fsanitize=undefined).
read -r -a EXTRA <<< "${STORAGE_TEST_CFLAGS:-}"
"$CC" "${FLAGS[@]}" "${EXTRA[@]}" -c "$TMP/ff.c" -o "$TMP/device-ff.o"
# Make an independent host FatFs instance with its own static mount/cache state.
# Rename only its defined public API and disk hooks; libc/Unicode stay shared.
python3 - "$TMP" <<'PY'
import pathlib, subprocess, sys
p=pathlib.Path(sys.argv[1])
names=[line.split()[-1] for line in subprocess.check_output(['nm','-g','--defined-only',str(p/'device-ff.o')],text=True).splitlines()]
names += ['disk_initialize','disk_status','disk_read','disk_write','disk_ioctl']
(p/'host-symbols').write_text(''.join(f'{n} host_{n}\n' for n in names))
PY
objcopy --redefine-syms="$TMP/host-symbols" "$TMP/device-ff.o" "$TMP/host-ff.o"
"$CC" "${FLAGS[@]}" "${EXTRA[@]}" "$ROOT/tests/storage_fatfs_roundtrip_test.c" \
    "$ROOT/src/filesystem/fs.c" "$ROOT/src/usb/msc_ownership.c" \
    "$ROOT/platform/rp2/usb/usb_msc.c" "$TMP/ffunicode.c" \
    "$TMP/device-ff.o" "$TMP/host-ff.o" -Wl,--wrap=f_mkfs,--wrap=f_setlabel -o "$TMP/roundtrip"
for scenario in roundtrip absent unsupported sync-fault write-fault remount-fault; do
    "$TMP/roundtrip" "$scenario"
done
