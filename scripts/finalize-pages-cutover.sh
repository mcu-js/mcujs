#!/usr/bin/env bash
set -euo pipefail

APPLY=0
SKIP_LIVE_CHECK=0

CANONICAL_DOCS="https://mcujs.org/"
COM_REDIRECT="https://mcujs.com/"
FALLBACK_DOCS="https://mcu-js.github.io/"

show_help() {
    cat <<'EOF'
mcujs GitHub Pages cutover finalizer

Usage:
  scripts/finalize-pages-cutover.sh [options]

Options:
  --apply             Restore GitHub metadata and temporary docs links
  --skip-live-check   Do not require canonical domain HTTP checks to pass first
  --help              Show this help text

Default behavior is a dry run. The script verifies that canonical domains are
live, then shows the GitHub metadata, release note, and README changes that
should happen after DNS and HTTPS cutover.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --apply)
            APPLY=1
            shift
            ;;
        --skip-live-check)
            SKIP_LIVE_CHECK=1
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$1" >&2
            show_help >&2
            exit 1
            ;;
    esac
done

fail() {
    printf '[FAIL] %s\n' "$1" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "Missing required command: $1"
}

check_url() {
    local url="$1"
    local expected_prefix="$2"
    local label="$3"
    local result
    local code
    local effective

    result="$(curl -sS -L -o /dev/null -w '%{http_code} %{url_effective}' "${url}")"
    code="${result%% *}"
    effective="${result#* }"

    printf '%s: %s -> %s (%s)\n' "${label}" "${url}" "${effective}" "${code}"

    [[ "${code}" == "200" ]] || fail "${label} returned HTTP ${code}"
    [[ "${effective}" == "${expected_prefix}"* ]] || fail "${label} ended at ${effective}, expected ${expected_prefix}"
}

check_pages_https() {
    require_command gh
    local repo="$1"

    gh api "repos/${repo}/pages" \
        --jq '{cname,https_enforced,https_certificate,status}'

    local enforced
    enforced="$(gh api "repos/${repo}/pages" --jq '.https_enforced')"
    [[ "${enforced}" == "true" ]] || fail "${repo} does not have HTTPS enforcement enabled"
}

run_live_checks() {
    require_command curl

    printf 'Canonical HTTP checks\n'
    check_url "https://mcujs.org/" "https://mcujs.org/" "mcujs.org"
    check_url "https://www.mcujs.org/" "https://mcujs.org/" "www.mcujs.org"
    check_url "https://mcujs.com/" "https://mcujs.org/" "mcujs.com"
    check_url "https://www.mcujs.com/" "https://mcujs.org/" "www.mcujs.com"

    printf '\nGitHub Pages HTTPS state\n'
    check_pages_https mcu-js/mcujs
    check_pages_https mcu-js/mcujs.com
}

restore_github_metadata() {
    printf '\nRestore GitHub metadata\n'
    printf '  org website: %s\n' "${CANONICAL_DOCS}"
    printf '  mcu-js/mcujs homepage: %s\n' "${CANONICAL_DOCS}"
    printf '  mcu-js/mcujs.com homepage: %s\n' "${COM_REDIRECT}"

    if [[ "${APPLY}" -eq 0 ]]; then
        printf '  dry-run: metadata not changed\n'
        return
    fi

    require_command gh
    gh api -X PATCH orgs/mcu-js -f "blog=${CANONICAL_DOCS}" --jq '{login,blog}'
    gh repo edit mcu-js/mcujs --homepage "${CANONICAL_DOCS}"
    gh repo edit mcu-js/mcujs.com --homepage "${COM_REDIRECT}"
}

restore_readme() {
    printf '\nRestore README docs link\n'
    if ! grep -Fq "${FALLBACK_DOCS}" README.md; then
        printf '  README.md does not contain fallback URL; no change needed\n'
        return
    fi

    if [[ "${APPLY}" -eq 0 ]]; then
        printf '  dry-run: replace fallback docs line with Docs: %s\n' "${CANONICAL_DOCS}"
        return
    fi

    perl -0pi -e 's{Docs: https://mcu-js\.github\.io/ while the canonical `mcujs\.org` domain finishes DNS cutover\.}{Docs: https://mcujs.org/}' README.md
}

restore_release_notes() {
    printf '\nRestore release notes docs link\n'

    if [[ "${APPLY}" -eq 0 ]]; then
        printf '  dry-run: replace v0.1.0 fallback docs note with Docs: %s\n' "${CANONICAL_DOCS}"
        return
    fi

    require_command gh
    local tmp
    tmp="$(mktemp)"
    gh release view v0.1.0 --repo mcu-js/mcujs --json body --jq .body > "${tmp}"
    perl -0pi -e 's{\A## Public docs\n\nDocs are currently available at https://mcu-js\.github\.io/ while DNS and HTTPS are being cut over for the canonical https://mcujs\.org/ domain\.\n\n}{## Public docs\n\nDocs: https://mcujs.org/\n\n}' "${tmp}"
    gh release edit v0.1.0 --repo mcu-js/mcujs --notes-file "${tmp}"
    rm -f "${tmp}"
}

if [[ "${SKIP_LIVE_CHECK}" -eq 0 ]]; then
    run_live_checks
else
    printf 'Canonical live checks skipped by --skip-live-check\n'
fi

restore_github_metadata
restore_readme
restore_release_notes

if [[ "${APPLY}" -eq 0 ]]; then
    printf '\nDry run complete. Pass --apply after DNS and HTTPS are healthy.\n'
else
    printf '\nCutover cleanup applied. Review README.md, then commit and push if it changed.\n'
fi
