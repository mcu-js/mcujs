# Releasing mcujs

This repository ships firmware as UF2 files. Release artifacts should be produced by scripts so the board list, checksums, and manifests stay deterministic.

## Before release

1. Update `version.txt`.
2. Update `CHANGELOG.md`.
3. Confirm every release board is listed by `scripts/boards.sh`.
4. Run source checks:

```bash
scripts/verify-release.sh
```

## Build and package

```bash
scripts/release.sh
```

This builds every board with Docker, then writes:

- `dist/mcujs-<version>-<git-sha>/`
- `dist/mcujs-<version>-<git-sha>.tar.gz`
- `dist/mcujs-<version>-<git-sha>-SHA256SUMS.txt`
- `dist/mcujs-<version>-<git-sha>-manifest.txt`
- top-level `dist/mcujs-<version>-<board-id>.uf2` assets

Use `scripts/release.sh --rebuild-image` after Dockerfile or dependency changes.

## Tagging

Use a `v<version>` tag, matching `version.txt`:

```bash
git tag -a v0.1.0 -m "mcujs v0.1.0"
git push origin v0.1.0
```

The release workflow can build and upload artifacts from the tag.

## Manual upload checklist

- Upload every top-level `dist/mcujs-<version>-<board-id>.uf2`.
- Upload `dist/mcujs-<version>-<git-sha>.tar.gz`.
- Upload `dist/mcujs-<version>-<git-sha>-SHA256SUMS.txt`.
- Upload `dist/mcujs-<version>-<git-sha>-manifest.txt`.
- Paste the relevant `CHANGELOG.md` section into the release notes.
