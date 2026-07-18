---
sidebar_position: 17
---

# Releasing

MCU.js releases promote a reviewed, physically tested development commit to the protected release branch before any tag or publication is created.

## Release branch policy

- `main` is the protected release branch (not `master`) and is release-only.
- Feature branches start from the latest approved `development` tip.
- Each implementation requires independent review before integration.
- Release candidates are immutable.
- Physical QA gates the `development` to `main` pull request.
- The pull request must precede the merge.
- Release tags and publishing happen only after the merge.

Normal feature work must not target `main` directly. Reviewed implementation branches integrate into `development`. A release candidate is one specific `development` commit; any source, documentation, metadata, or test change creates a new candidate and invalidates the previous candidate's verification.

## Candidate preparation

1. Select and record the exact `development` commit.
2. Update `version.txt` and `CHANGELOG.md` before freezing the candidate.
3. Confirm every release board is registered by `scripts/boards.sh`.
4. Run the release source and documentation checks:

```bash
scripts/verify-release.sh --docs
```

5. Build and package every release board from the candidate:

```bash
scripts/release.sh
```

The build writes board-qualified UF2 files plus checksums and a release manifest. See [Advanced: Building from Source](./advanced-building.md#release-build) for the artifact layout.

## Physical QA and promotion

Run the candidate's required hardware matrix, including recovery and persistence paths affected by the release. Record the board IDs, firmware build IDs, commands, and results. If any candidate file changes, freeze a new commit and repeat verification and physical QA.

Only after the immutable candidate passes physical QA:

1. Open a pull request from `development` to `main` for that exact candidate.
2. Review and approve the pull request before merge.
3. Merge the approved pull request without changing the candidate contents.
4. Verify the resulting `main` commit is the intended release commit.
5. Create the `v<version>` tag on that `main` commit.
6. Publish artifacts generated from the tagged commit.

Never tag or publish a candidate directly from `development`, and never merge first with a promise to open or reconstruct the pull request later.

## Upload checklist

- Upload every top-level `dist/mcujs-<version>-<board-id>.uf2`.
- Upload `dist/mcujs-<version>-<git-sha>.tar.gz`.
- Upload `dist/mcujs-<version>-<git-sha>-SHA256SUMS.txt`.
- Upload `dist/mcujs-<version>-<git-sha>-manifest.txt`.
- Use the matching `CHANGELOG.md` section as the release notes.
