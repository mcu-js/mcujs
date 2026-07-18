#!/usr/bin/env python3
"""Generate and independently verify an ESP32-S3 application-only UF2."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

TINYUF2_COMMIT = "9af754c408ae76e862830de33dbd03b93a9a82e1"
UF2_COMMIT = "84444ddb9d2914edf7f6d9e89a7ce41b53d64d98"
FAMILY_ID = 0xC47E5767
BOARD_ID = "seeed_xiao_esp32s3"


def git_commit(path: Path) -> str:
    return subprocess.check_output(
        ["git", "-C", str(path), "rev-parse", "HEAD"], text=True
    ).strip()


def require_clean_checkout(path: Path, label: str) -> None:
    status = subprocess.check_output(
        ["git", "-C", str(path), "status", "--porcelain", "--untracked-files=normal"],
        text=True,
    )
    if status:
        raise SystemExit(f"{label} checkout is dirty")


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    version = (script_dir.parents[1] / "version.txt").read_text(encoding="ascii").strip()
    default_output = script_dir / f"build/mcujs-{version}-{BOARD_ID}.uf2"
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=script_dir / "build/mcujs-esp32s3.bin")
    parser.add_argument("--output", type=Path, default=default_output)
    parser.add_argument("--ota-limit", type=lambda value: int(value, 0), default=0x200000)
    parser.add_argument(
        "--tinyuf2",
        type=Path,
        default=Path(os.environ.get("TINYUF2_PATH", Path.home() / "toolchains/tinyuf2-0.35.0")),
    )
    args = parser.parse_args()

    if args.output.is_symlink():
        raise SystemExit(f"refusing symlinked UF2 output: {args.output}")
    if args.output.exists() and not args.output.is_file():
        raise SystemExit(f"refusing non-regular UF2 output: {args.output}")
    legacy_output: Path | None = None
    if args.output.resolve(strict=False) == default_output.resolve(strict=False):
        legacy_output = script_dir / "build/mcujs-esp32s3.uf2"
        if legacy_output.is_symlink():
            raise SystemExit(f"refusing symlinked legacy UF2 output: {legacy_output}")
        if legacy_output.exists():
            if not legacy_output.is_file():
                raise SystemExit(f"refusing non-regular legacy UF2 output: {legacy_output}")

    if git_commit(args.tinyuf2) != TINYUF2_COMMIT:
        raise SystemExit("TinyUF2 checkout is not at the reviewed source pin")
    require_clean_checkout(args.tinyuf2, "TinyUF2")
    uf2_repo = args.tinyuf2 / "lib/uf2"
    if git_commit(uf2_repo) != UF2_COMMIT:
        raise SystemExit("Microsoft UF2 submodule is not at the reviewed source pin")
    require_clean_checkout(uf2_repo, "Microsoft UF2")

    converter = uf2_repo / "utils/uf2conv.py"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{args.output.name}.", suffix=".tmp", dir=args.output.parent
    )
    os.close(descriptor)
    temporary_output = Path(temporary_name)
    try:
        subprocess.run(
            [
                sys.executable,
                str(converter),
                "-f",
                hex(FAMILY_ID),
                "-b",
                "0x0",
                "-c",
                "-o",
                str(temporary_output),
                str(args.input),
            ],
            check=True,
        )

        binary = args.input.read_bytes()
        uf2 = temporary_output.read_bytes()
        if len(uf2) % 512:
            raise SystemExit("UF2 length is not block aligned")

        blocks: list[tuple[int, bytes, int]] = []
        for offset in range(0, len(uf2), 512):
            block = uf2[offset : offset + 512]
            magic0, magic1, flags, address, size, number, total, family = struct.unpack_from(
                "<IIIIIIII", block, 0
            )
            end_magic = struct.unpack_from("<I", block, 508)[0]
            if (magic0, magic1, end_magic) != (0x0A324655, 0x9E5D5157, 0x0AB16F30):
                raise SystemExit(f"invalid UF2 magic in block {number}")
            if not flags & 0x2000 or family != FAMILY_ID or size != 256:
                raise SystemExit(f"invalid ESP32-S3 metadata in block {number}")
            if number != len(blocks):
                raise SystemExit("UF2 blocks are not sequential")
            blocks.append((address, block[32 : 32 + size], total))

        if not blocks or any(total != len(blocks) for _, _, total in blocks):
            raise SystemExit("UF2 block count metadata is inconsistent")
        if [address for address, _, _ in blocks] != list(range(0, len(blocks) * 256, 256)):
            raise SystemExit("UF2 target addresses are not contiguous from zero")

        reconstructed = b"".join(payload for _, payload, _ in blocks)
        if reconstructed[: len(binary)] != binary or any(reconstructed[len(binary) :]):
            raise SystemExit("UF2 payload does not reconstruct the input image")
        target_end = blocks[-1][0] + 256
        if target_end > args.ota_limit:
            raise SystemExit("UF2 exceeds the configured OTA0 boundary")

        temporary_output.chmod(0o644)
        with temporary_output.open("rb") as handle:
            os.fsync(handle.fileno())
        os.replace(temporary_output, args.output)
        if legacy_output is not None and legacy_output.exists():
            legacy_output.unlink()
        directory_fd = os.open(args.output.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory_fd)
        finally:
            os.close(directory_fd)
    finally:
        if temporary_output.exists() or temporary_output.is_symlink():
            temporary_output.unlink()

    print(f"UF2: {args.output}")
    print(f"Blocks: {len(blocks)}")
    print(f"Target range: 0x0..0x{target_end:x}")
    print(f"Family ID: 0x{FAMILY_ID:08x}")
    print(f"Payload SHA-256: {hashlib.sha256(binary).hexdigest()}")
    print(f"UF2 SHA-256: {hashlib.sha256(uf2).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
