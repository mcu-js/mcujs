#!/usr/bin/env bash
set -euo pipefail

API_URL="${DYNADOT_API_URL:-https://api.dynadot.com/api3.json}"
APPLY=0
VERIFY=0
ENABLE_HTTPS=0

GITHUB_PAGES_IPV4=(
    185.199.108.153
    185.199.109.153
    185.199.110.153
    185.199.111.153
)

GITHUB_PAGES_IPV6=(
    2606:50c0:8000::153
    2606:50c0:8001::153
    2606:50c0:8002::153
    2606:50c0:8003::153
)

show_help() {
    cat <<'EOF'
mcujs GitHub Pages DNS helper

Usage:
  scripts/configure-pages-dns.sh [options]

Options:
  --apply          Apply Dynadot DNS changes. Requires DYNADOT_API_KEY.
  --verify         Check DNS and GitHub Pages domain health.
  --enable-https   Ask GitHub Pages to enforce HTTPS for both custom domains.
  --help           Show this help text.

Default behavior is a dry run that prints the Dynadot API changes without
requiring an API key.

DNS target:
  mcujs.org and mcujs.com apex A/AAAA records -> GitHub Pages
  www.mcujs.org and www.mcujs.com CNAME      -> mcu-js.github.io
  mcujs.com nameservers                      -> ns1/ns2.dyna-ns.net

Optional GitHub Pages org-domain verification TXT values:
  GITHUB_PAGES_VERIFY_MCUJS_ORG -> _github-pages-challenge-mcu-js.mcujs.org
  GITHUB_PAGES_VERIFY_MCUJS_COM -> _github-pages-challenge-mcu-js.mcujs.com

Note: Dynadot set_dns2 overwrites existing DNS settings. Public DNS currently
shows no MX, TXT, or CAA records for these domains; re-check before applying if
that changes.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --apply)
            APPLY=1
            shift
            ;;
        --verify)
            VERIFY=1
            shift
            ;;
        --enable-https)
            ENABLE_HTTPS=1
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

dynadot_request() {
    local label="$1"
    shift
    local params=("$@")
    local response
    local status
    local code

    printf '\n%s\n' "$label"
    for param in "${params[@]}"; do
        printf '  %s\n' "$param"
    done

    if [[ "${APPLY}" -eq 0 ]]; then
        printf '  dry-run: not sent\n'
        return
    fi

    require_command curl
    require_command jq
    [[ -n "${DYNADOT_API_KEY:-}" ]] || fail 'DYNADOT_API_KEY is required with --apply'

    local curl_args=(-sS --get "${API_URL}" --data-urlencode "key=${DYNADOT_API_KEY}")
    for param in "${params[@]}"; do
        curl_args+=(--data-urlencode "${param}")
    done

    response="$(curl "${curl_args[@]}")"
    printf '%s\n' "${response}" | jq .

    status="$(printf '%s\n' "${response}" | jq -r '.. | objects | .Status? // empty' | head -n 1)"
    code="$(printf '%s\n' "${response}" | jq -r '.. | objects | (.ResponseCode? // .SuccessCode? // empty)' | head -n 1)"

    [[ "${status}" == "success" && "${code}" == "0" ]] || fail "Dynadot API call failed: ${label}"
}

set_dynadot_nameservers() {
    local domain="$1"

    dynadot_request "Set Dynadot nameservers for ${domain}" \
        "command=set_ns" \
        "domain=${domain}" \
        "ns0=ns1.dyna-ns.net" \
        "ns1=ns2.dyna-ns.net"
}

set_dynadot_dns() {
    local domain="$1"
    local verification_token=""
    local params=(
        "command=set_dns2"
        "domain=${domain}"
        "ttl=300"
    )
    local index=0
    local ip

    for ip in "${GITHUB_PAGES_IPV4[@]}"; do
        params+=("main_record_type${index}=a")
        params+=("main_record${index}=${ip}")
        index=$((index + 1))
    done

    for ip in "${GITHUB_PAGES_IPV6[@]}"; do
        params+=("main_record_type${index}=aaaa")
        params+=("main_record${index}=${ip}")
        index=$((index + 1))
    done

    params+=(
        "subdomain0=www"
        "sub_record_type0=cname"
        "sub_record0=mcu-js.github.io"
    )

    case "${domain}" in
        mcujs.org)
            verification_token="${GITHUB_PAGES_VERIFY_MCUJS_ORG:-}"
            ;;
        mcujs.com)
            verification_token="${GITHUB_PAGES_VERIFY_MCUJS_COM:-}"
            ;;
    esac

    if [[ -n "${verification_token}" ]]; then
        params+=(
            "subdomain1=_github-pages-challenge-mcu-js"
            "sub_record_type1=txt"
            "sub_record1=${verification_token}"
        )
    fi

    dynadot_request "Set Dynadot DNS for ${domain}" "${params[@]}"
}

check_dns() {
    require_command python3

    python3 - <<'PY'
import socket

for name in ("mcujs.org", "www.mcujs.org", "mcujs.com", "www.mcujs.com"):
    try:
        values = sorted({item[4][0] for item in socket.getaddrinfo(name, None)})
        print(f"{name}: {values}")
    except OSError as exc:
        print(f"{name}: {type(exc).__name__}: {exc}")
PY

    if command -v gh >/dev/null 2>&1; then
        printf '\nGitHub Pages health: mcu-js/mcujs\n'
        pages_health mcu-js/mcujs

        printf '\nGitHub Pages health: mcu-js/mcujs.com\n'
        pages_health mcu-js/mcujs.com
    fi
}

pages_health() {
    local repo="$1"
    local output

    if ! output="$(gh api "repos/${repo}/pages/health" 2>&1 >/dev/null)"; then
        printf '{"status":"GitHub Pages health check unavailable; run locally with a token that can read Pages health."}\n'
        return 0
    fi

    sleep 5
    if ! output="$(gh api "repos/${repo}/pages/health" \
        --jq 'if .domain == null and .alt_domain == null then
                {status: "GitHub Pages health check is still pending; rerun --verify."}
              else
                {domain: .domain.reason, alt_domain: .alt_domain.reason}
              end' 2>&1)"; then
        printf '{"status":"GitHub Pages health check unavailable; run locally with a token that can read Pages health."}\n'
        return 0
    fi

    printf '%s\n' "${output}"
}

enable_https() {
    require_command gh

    gh api -X PUT repos/mcu-js/mcujs/pages -F https_enforced=true \
        --jq '{cname,https_enforced,https_certificate}'
    gh api -X PUT repos/mcu-js/mcujs.com/pages -F https_enforced=true \
        --jq '{cname,https_enforced,https_certificate}'
}

set_dynadot_dns mcujs.org
set_dynadot_dns mcujs.com
set_dynadot_nameservers mcujs.com

if [[ "${VERIFY}" -eq 1 ]]; then
    printf '\nDNS verification\n'
    check_dns
fi

if [[ "${ENABLE_HTTPS}" -eq 1 ]]; then
    printf '\nHTTPS enforcement\n'
    enable_https
fi
