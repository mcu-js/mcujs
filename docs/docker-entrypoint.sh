#!/usr/bin/env bash
set -euo pipefail
# Arguments are paths so the copy/lock contract can also be tested without Docker.
[[ $# == 3 ]] || { printf 'Website build: expected source, image dependencies, output\n' >&2; exit 1; }
source_dir="$1"; dependencies="$2"; output="$3"
for file in package.json package-lock.json; do
    cmp -s "$source_dir/$file" "$dependencies/$file" || {
        printf 'Website build: image %s differs from source; matching locked dependencies required\n' "$file" >&2
        exit 1
    }
done
[[ -d "$dependencies/node_modules" ]] || { printf 'Website build: image dependencies missing\n' >&2; exit 1; }
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
# Never import checkout dependencies, generated site/cache, or Git metadata.
tar -C "$source_dir" --exclude=node_modules --exclude=build --exclude=.docusaurus --exclude=.git -cf - . | tar -C "$work" -xf -
cp -R "$dependencies/node_modules" "$work/node_modules"
cd "$work"
unset NODE_PATH NODE_OPTIONS
export HOME="$work" npm_config_cache="$work/.npm" npm_config_offline=true
npm run typecheck
npm run build -- --out-dir "$work/site"
[[ -f "$work/site/index.html" ]] || { printf 'Website build: missing site/index.html\n' >&2; exit 1; }
cp -R "$work/site/." "$output/"
