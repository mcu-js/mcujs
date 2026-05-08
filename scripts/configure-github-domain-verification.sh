#!/usr/bin/env bash
set -euo pipefail

ORG="${GITHUB_ORG:-mcu-js}"
DOMAINS=(mcujs.org mcujs.com)
ADD=0
VERIFY=0
LIST=0

show_help() {
    cat <<'EOF'
mcujs GitHub organization domain verification helper

Usage:
  scripts/configure-github-domain-verification.sh [options]

Options:
  --add      Add mcujs.org and mcujs.com as verifiable domains and print TXT tokens
  --verify   Ask GitHub to verify existing verifiable domains
  --list     List existing verifiable domains and TXT token state
  --help     Show this help text

Requires `gh` authenticated with `admin:org`:

  gh auth refresh -h github.com -s admin:org

The TXT tokens printed by --add/--list can be passed to:

  GITHUB_PAGES_VERIFY_MCUJS_ORG=... \
  GITHUB_PAGES_VERIFY_MCUJS_COM=... \
  DYNADOT_API_KEY=... \
  scripts/configure-pages-dns.sh --apply
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --add)
            ADD=1
            shift
            ;;
        --verify)
            VERIFY=1
            shift
            ;;
        --list)
            LIST=1
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

if [[ "${ADD}" -eq 0 && "${VERIFY}" -eq 0 && "${LIST}" -eq 0 ]]; then
    LIST=1
fi

fail() {
    printf '[FAIL] %s\n' "$1" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "Missing required command: $1"
}

graphql() {
    local response

    if ! response="$(gh api graphql "$@" 2>&1)"; then
        if printf '%s\n' "${response}" | grep -Fq 'admin:org'; then
            printf '[FAIL] GitHub token lacks admin:org scope.\n' >&2
            printf 'Run: gh auth refresh -h github.com -s admin:org\n' >&2
        else
            printf '%s\n' "${response}" >&2
        fi
        return 1
    fi

    if printf '%s\n' "${response}" | jq -e '.errors? // empty' >/dev/null; then
        if printf '%s\n' "${response}" | jq -e '.errors[]?.message | contains("admin:org")' >/dev/null; then
            printf '[FAIL] GitHub token lacks admin:org scope.\n' >&2
            printf 'Run: gh auth refresh -h github.com -s admin:org\n' >&2
        else
            printf '%s\n' "${response}" | jq . >&2
        fi
        return 1
    fi

    printf '%s\n' "${response}"
}

owner_id() {
    graphql -f query='query($login: String!) { organization(login: $login) { id } }' \
        -f login="${ORG}" \
        --jq '.data.organization.id'
}

list_domains_json() {
    graphql -f query='query($login: String!) {
      organization(login: $login) {
        domains(first: 20) {
          nodes {
            id
            domain
            dnsHostName
            verificationToken
            isVerified
            hasFoundVerificationToken
            tokenExpirationTime
          }
        }
      }
    }' -f login="${ORG}" \
        --jq '.data.organization.domains.nodes'
}

print_domains() {
    list_domains_json | jq -r '
      if length == 0 then
        "No verifiable domains configured."
      else
        .[] | [
          "domain=" + .domain,
          "dnsHostName=" + .dnsHostName,
          "verificationToken=" + .verificationToken,
          "isVerified=" + (.isVerified | tostring),
          "hasFoundVerificationToken=" + (.hasFoundVerificationToken | tostring)
        ] | @tsv
      end'
}

add_domain() {
    local id="$1"
    local domain="$2"

    graphql -f query='mutation($ownerId: ID!, $domain: URI!) {
      addVerifiableDomain(input: {ownerId: $ownerId, domain: $domain}) {
        domain {
          id
          domain
          dnsHostName
          verificationToken
          isVerified
          hasFoundVerificationToken
          tokenExpirationTime
        }
      }
    }' -f ownerId="${id}" -f domain="${domain}" \
        --jq '.data.addVerifiableDomain.domain'
}

verify_domain() {
    local id="$1"

    graphql -f query='mutation($id: ID!) {
      verifyVerifiableDomain(input: {id: $id}) {
        domain {
          id
          domain
          dnsHostName
          isVerified
          hasFoundVerificationToken
        }
      }
    }' -f id="${id}" \
        --jq '.data.verifyVerifiableDomain.domain'
}

require_command gh
require_command jq

if [[ "${ADD}" -eq 1 ]]; then
    org_id="$(owner_id)"
    for domain in "${DOMAINS[@]}"; do
        printf '\nAdding verifiable domain: %s\n' "${domain}"
        add_domain "${org_id}" "${domain}" | jq .
    done
fi

if [[ "${LIST}" -eq 1 || "${ADD}" -eq 1 ]]; then
    printf '\nCurrent verifiable domains:\n'
    print_domains
fi

if [[ "${VERIFY}" -eq 1 ]]; then
    current="$(list_domains_json)"
    for domain in "${DOMAINS[@]}"; do
        id="$(printf '%s\n' "${current}" | jq -r --arg domain "${domain}" '.[] | select(.domain == $domain) | .id' | head -n 1)"
        [[ -n "${id}" ]] || fail "No verifiable domain found for ${domain}; run --add first."
        printf '\nVerifying domain: %s\n' "${domain}"
        verify_domain "${id}" | jq .
    done
fi
