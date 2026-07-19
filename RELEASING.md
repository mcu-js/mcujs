# Releasing mcujs

This repository ships firmware as UF2 files. Release artifacts should be produced by scripts so the board list, checksums, and manifests stay deterministic.

## Release branch policy

- `main` is the protected release branch (not `master`) and is release-only.
- Feature branches start from the latest approved `development` tip.
- Each implementation requires independent review before integration.
- Release candidates are immutable.
- Physical QA gates the `development` to `main` pull request.
- The pull request must precede the merge.
- Release tags and publishing happen only after the merge.

Normal implementation work lands on `development` only after review. A release candidate is a specific, immutable `development` commit: any code, documentation, metadata, or test change creates a new candidate and restarts candidate verification. After physical QA passes, open and approve the `development` to `main` pull request before merging it. Tag and publish the resulting `main` commit; never tag or publish the candidate before merge.

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

This builds all RP boards with the Pico builder and freshly builds the XIAO
ESP32-S3 with its pinned Docker lane. Before packaging, the release gate checks
that the XIAO UF2 reconstructs the adjacent binary, contains the current source
build ID, uses only supported flashing flags, fits the authoritative `ota_0`
partition, and has the generated capability manifest. The package step
preflights and privately stages every output before atomically replacing the
matching release assets in `dist/`. It then writes:

- `dist/mcujs-<version>-<git-sha>/`
- `dist/mcujs-<version>-<git-sha>.tar.gz`
- `dist/mcujs-<version>-<git-sha>-SHA256SUMS.txt`
- `dist/mcujs-<version>-<git-sha>-manifest.txt`
- top-level `dist/mcujs-<version>-<board-id>.uf2` assets

Use `scripts/release.sh --rebuild-image` after RP Dockerfile or dependency
changes. The XIAO Docker image build step always runs when builds are enabled.

## Tagging and publishing

Only after the approved `development` to `main` pull request has merged, use a `v<version>` tag matching `version.txt` on the resulting `main` commit:

```bash
git tag -a v0.1.0 -m "mcujs v0.1.0"
git push origin v0.1.0
```

The release workflow can build and upload artifacts from the tag. Publishing before the merge is prohibited because the protected release branch must contain the exact released commit.

## Manual upload checklist

- Upload every top-level `dist/mcujs-<version>-<board-id>.uf2`.
- Upload `dist/mcujs-<version>-<git-sha>.tar.gz`.
- Upload `dist/mcujs-<version>-<git-sha>-SHA256SUMS.txt`.
- Upload `dist/mcujs-<version>-<git-sha>-manifest.txt`.
- Paste the relevant `CHANGELOG.md` section into the release notes.
