#!/usr/bin/env bash
set -euo pipefail
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
repo_dir="$(dirname "$source_dir")"
fail() { printf 'Website build: %s\n' "$*" >&2; exit 1; }
[[ $# == 1 ]] || fail 'usage: bash docs/docker-build.sh /absolute/empty/output (outside checkout)'
[[ "$1" == /* ]] || fail 'output must be absolute'
output="$(realpath -m -- "$1")"
case "$output/" in "$repo_dir/"*) fail 'output must be outside checkout';; esac
[[ "$output" != *:* && "$source_dir" != *:* ]] || fail 'colon in mount path is unsupported'
if [[ -e "$output" ]]; then
    [[ -d "$output" ]] || fail 'output must be a directory'
    shopt -s nullglob dotglob
    entries=("$output"/*)
    [[ ${#entries[@]} == 0 ]] || fail 'output must be empty; existing artifacts are never removed'
fi
image="${MCUJS_DOCS_DOCKER_IMAGE:-mcujs-docs-builder}"
image_id="$(docker image inspect --format '{{.Id}}' "$image" 2>/dev/null)" || fail "local image $image unavailable; no pull or preparation attempted"
[[ "$image_id" =~ ^sha256:[0-9a-f]{64}$ ]] || fail 'invalid local image ID'
mkdir -p -- "$output"
# Only docs is mounted: sibling worktrees and their dependencies are inaccessible.
exec docker run --rm --pull never --network none --read-only \
    --user "$(id -u):$(id -g)" --cap-drop ALL --security-opt no-new-privileges \
    --pids-limit 256 --memory 2g --cpus 2 --stop-timeout 10 \
    --tmpfs /tmp:rw,nosuid,nodev,size=1536m,mode=1777 \
    -e HOME=/tmp -e NODE_ENV=production \
    -v "$source_dir:/source:ro" -v "$output:/output" \
    --entrypoint timeout "$image_id" --signal=TERM --kill-after=10 600 \
    bash /source/docker-entrypoint.sh /source /opt/docs /output
