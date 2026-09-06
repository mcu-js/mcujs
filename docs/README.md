# Website

The site uses Docusaurus. Website validation is separate from firmware and core
runtime tests; a website failure must not disable those checks. A passing website
typecheck and production build still gate website publication (the existing Docs
workflow retains both checks).

## Isolated, offline production build

From the repository root, with a **previously prepared, reviewed local image**:

```bash
MCUJS_DOCS_DOCKER_IMAGE=mcujs-docs-builder \
  bash docs/docker-build.sh /absolute/path/outside-checkout/empty-site-output
```

The output directory must be empty (or absent) and outside this checkout. No
existing files are deleted. This command never pulls an image, installs packages,
or prepares an image. It resolves the local image to its immutable ID, runs with
network disabled, mounts only `docs` read-only, and copies source into a temporary
container workspace. Checkout `node_modules`, `build`, and `.docusaurus` are
excluded; no other worktree is mounted. Dependencies come only from the image.
Both `package.json` and `package-lock.json` must exactly match the image or the
build stops before running npm. It runs typecheck, then the production build,
and exports the site only after both succeed. A failure returns nonzero; do not
publish that output (an interrupted final copy may leave partial files).

The container uses the caller's UID/GID, a read-only root, no extra capabilities,
2 CPUs, 2 GiB memory, 256 processes, and a 1536 MiB temporary filesystem. The
command has a 600-second limit followed by a 10-second kill grace. These are
initial bounded defaults, not measured production sizing guarantees.

### Dependency image preparation is a separate gate

`docs/Dockerfile` is a preparation recipe, **not part of the offline build**. It
requires an explicit Debian-based Node >=20 image reference and installs the
existing lock with `npm ci`. The context excludes everything except the manifests
and Dockerfile. There is no unpinned default base. After separate acquisition
approval, an operator may prepare it with a reviewed immutable base:

```bash
# Preparation may acquire the base image and locked npm packages. Not offline.
docker build --build-arg NODE_IMAGE='node@sha256:<reviewed-digest>' \
  -t mcujs-docs-builder -f docs/Dockerfile docs
```

Under a no-downloads policy, do not run that preparation to fix a missing image.
Report the missing reviewed image or exact missing locked packages instead. A
recipe or passing recording-fixture test is not proof of a successful website
build. Image preparation and real Docker validation require separate execution.

## Isolation tests (no packages or Docker daemon)

```bash
node --test tests/docs-docker.test.js
```

These exercise command isolation and real shell copying with recording Docker/npm
fixtures. They verify unchanged fixture source, exclusion of checkout dependencies,
lock mismatch refusal, and failure propagation. They do not build Docusaurus.

## Local development (optional; changes the checkout)

If installing contributor-local dependencies is permitted, use `npm ci` in `docs`
(the committed npm lock is authoritative), then `npm start`. `npm run typecheck`
and `npm run build` are the direct website checks used by CI. This local workflow
is not the isolated production path and must not borrow another worktree's
`node_modules`. Publication remains a separately authorized operation.
