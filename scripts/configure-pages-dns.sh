#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -f "${ROOT_DIR}/.env" ]]; then
    set -a
    # shellcheck disable=SC1091
    source "${ROOT_DIR}/.env"
    set +a
fi

API_BASE_URL="${DYNADOT_API_BASE_URL:-https://api.dynadot.com}"
API_VERSION="${DYNADOT_API_VERSION:-v2}"
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
  --apply          Apply Dynadot DNS changes. Requires DYNADOT_API_KEY and DYNADOT_API_SECRET.
  --verify         Check DNS and GitHub Pages domain health.
  --enable-https   Ask GitHub Pages to enforce HTTPS for both custom domains.
  --help           Show this help text.

Default behavior is a dry run that prints the Dynadot REST API changes without
requiring API credentials. Local .env is loaded automatically when present.

DNS target:
  mcujs.org and mcujs.com apex A/AAAA records -> GitHub Pages
  www.mcujs.org and www.mcujs.com CNAME      -> mcu-js.github.io
  mcujs.com nameservers                      -> ns1/ns2.dyna-ns.net

Optional GitHub Pages org-domain verification TXT values:
  GITHUB_PAGES_VERIFY_MCUJS_ORG -> _gh-mcu-js-o.mcujs.org
  GITHUB_PAGES_VERIFY_MCUJS_COM -> _gh-mcu-js-o.mcujs.com

Note: Dynadot records are sent with add_dns_to_current_setting=false, replacing
existing DNS settings. During apply, existing _gh-mcu-js-o TXT verification
records are preserved when explicit token values are not supplied. Public DNS
currently shows no MX, TXT, or CAA records for these domains; re-check before
applying if that changes.
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

json_value() {
    local json="$1"
    local filter="$2"

    printf '%s\n' "${json}" | jq -r "${filter}"
}

request_id() {
    if command -v uuidgen >/dev/null 2>&1; then
        uuidgen | tr '[:upper:]' '[:lower:]'
    elif [[ -r /proc/sys/kernel/random/uuid ]]; then
        cat /proc/sys/kernel/random/uuid
    else
        local hex
        hex="$(openssl rand -hex 16)"
        printf '%s-%s-%s-%s-%s\n' "${hex:0:8}" "${hex:8:4}" "${hex:12:4}" "${hex:16:4}" "${hex:20:12}"
    fi
}

dynadot_signature() {
    local path="$1"
    local request_id="$2"
    local body="$3"

    printf '%s\n%s\n%s\n%s' \
        "${DYNADOT_API_KEY}" \
        "${path}" \
        "${request_id}" \
        "${body}" |
        openssl dgst -sha256 -hmac "${DYNADOT_API_SECRET}" -binary |
        openssl base64 -A
}

dynadot_rest_request() {
    local label="$1"
    local method="$2"
    local path="$3"
    local body="${4:-}"
    local response
    local response_body
    local http_code
    local code

    printf '\n%s\n' "$label"
    printf '  %s %s%s\n' "${method}" "${API_BASE_URL}" "${path}"
    if [[ -n "${body}" ]]; then
        printf '%s\n' "${body}" | jq . | sed 's/^/  /'
    fi

    if [[ "${APPLY}" -eq 0 ]]; then
        printf '  dry-run: not sent\n'
        return
    fi

    require_command curl
    require_command jq
    require_command openssl
    [[ -n "${DYNADOT_API_KEY:-}" ]] || fail 'DYNADOT_API_KEY is required with --apply'
    [[ -n "${DYNADOT_API_SECRET:-}" ]] || fail 'DYNADOT_API_SECRET is required with --apply'

    if [[ -n "${body}" ]]; then
        body="$(printf '%s\n' "${body}" | jq -c .)"
    fi

    local id
    local signature
    id="$(request_id)"
    signature="$(dynadot_signature "${path}" "${id}" "${body}")"

    local curl_args=(
        -sS
        -w $'\n%{http_code}'
        -X "${method}"
        "${API_BASE_URL}${path}"
        -H "Accept: application/json"
        -H "Authorization: Bearer ${DYNADOT_API_KEY}"
        -H "X-Request-ID: ${id}"
        -H "X-Signature: ${signature}"
    )

    if [[ -n "${body}" ]]; then
        curl_args+=(
            -H "Content-Type: application/json"
            --data "${body}"
        )
    fi

    response="$(curl "${curl_args[@]}")"
    http_code="${response##*$'\n'}"
    response_body="${response%$'\n'*}"

    if printf '%s\n' "${response_body}" | jq . >/dev/null 2>&1; then
        printf '%s\n' "${response_body}" | jq .
        code="$(json_value "${response_body}" '.code // .Code // empty')"
    else
        printf '%s\n' "${response_body}"
        code=""
    fi

    [[ "${http_code}" =~ ^2[0-9][0-9]$ ]] || fail "Dynadot API call failed: ${label} (HTTP ${http_code})"
    [[ -z "${code}" || "${code}" =~ ^2[0-9][0-9]$ ]] || fail "Dynadot API call failed: ${label} (code ${code})"
}

dynadot_existing_txt_record() {
    local domain="$1"
    local host="$2"
    local path="/restful/${API_VERSION}/domains/${domain}/records"
    local response
    local response_body
    local http_code
    local id
    local signature

    require_command curl
    require_command jq
    require_command openssl
    [[ -n "${DYNADOT_API_KEY:-}" ]] || fail 'DYNADOT_API_KEY is required with --apply'
    [[ -n "${DYNADOT_API_SECRET:-}" ]] || fail 'DYNADOT_API_SECRET is required with --apply'

    id="$(request_id)"
    signature="$(dynadot_signature "${path}" "${id}" "")"
    response="$(
        curl -sS -w $'\n%{http_code}' -X GET "${API_BASE_URL}${path}" \
            -H "Accept: application/json" \
            -H "Authorization: Bearer ${DYNADOT_API_KEY}" \
            -H "X-Request-ID: ${id}" \
            -H "X-Signature: ${signature}"
    )"
    http_code="${response##*$'\n'}"
    response_body="${response%$'\n'*}"

    [[ "${http_code}" =~ ^2[0-9][0-9]$ ]] || return 0
    printf '%s\n' "${response_body}" |
        jq -r --arg host "${host}" '
            .data.name_server_settings.sub_domains[]?
            | select(.sub_host == $host and .record_type == "txt")
            | .value
        ' |
        head -n 1
}

set_dynadot_nameservers() {
    local domain="$1"
    local body

    body="$(jq -c -n '{nameserver_list: ["ns1.dyna-ns.net", "ns2.dyna-ns.net"]}')"

    dynadot_rest_request \
        "Set Dynadot nameservers for ${domain}" \
        "PUT" \
        "/restful/${API_VERSION}/domains/${domain}/nameservers" \
        "${body}"
}

set_dynadot_dns() {
    local domain="$1"
    local verification_token=""
    local verification_host="_gh-mcu-js-o"
    local main_json="[]"
    local sub_json="[]"
    local body
    local ip

    for ip in "${GITHUB_PAGES_IPV4[@]}"; do
        main_json="$(jq -c --arg ip "${ip}" '. + [{record_type: "a", record_value1: $ip}]' <<< "${main_json}")"
    done

    for ip in "${GITHUB_PAGES_IPV6[@]}"; do
        main_json="$(jq -c --arg ip "${ip}" '. + [{record_type: "aaaa", record_value1: $ip}]' <<< "${main_json}")"
    done

    sub_json="$(jq -c '. + [{sub_host: "www", record_type: "cname", record_value1: "mcu-js.github.io"}]' <<< "${sub_json}")"

    case "${domain}" in
        mcujs.org)
            verification_token="${GITHUB_PAGES_VERIFY_MCUJS_ORG:-}"
            verification_host="${GITHUB_PAGES_VERIFY_MCUJS_ORG_HOST:-${verification_host}}"
            ;;
        mcujs.com)
            verification_token="${GITHUB_PAGES_VERIFY_MCUJS_COM:-}"
            verification_host="${GITHUB_PAGES_VERIFY_MCUJS_COM_HOST:-${verification_host}}"
            ;;
    esac

    if [[ -z "${verification_token}" && "${APPLY}" -eq 1 ]]; then
        verification_token="$(dynadot_existing_txt_record "${domain}" "${verification_host}")"
    fi

    if [[ -n "${verification_token}" ]]; then
        sub_json="$(
            jq -c --arg host "${verification_host}" --arg token "${verification_token}" \
                '. + [{sub_host: $host, record_type: "txt", record_value1: $token}]' \
                <<< "${sub_json}"
        )"
    fi

    body="$(
        jq -c -n \
            --argjson main "${main_json}" \
            --argjson sub "${sub_json}" \
            '{dns_main_list: $main, sub_list: $sub, ttl: 300, add_dns_to_current_setting: false}'
    )"

    dynadot_rest_request \
        "Set Dynadot DNS for ${domain}" \
        "POST" \
        "/restful/${API_VERSION}/domains/${domain}/records" \
        "${body}"
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
set_dynadot_nameservers mcujs.com
set_dynadot_dns mcujs.com

if [[ "${VERIFY}" -eq 1 ]]; then
    printf '\nDNS verification\n'
    check_dns
fi

if [[ "${ENABLE_HTTPS}" -eq 1 ]]; then
    printf '\nHTTPS enforcement\n'
    enable_https
fi
