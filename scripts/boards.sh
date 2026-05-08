#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/boards.sh"

show_help() {
    cat <<'EOF'
mcujs board registry

Usage:
  scripts/boards.sh [--ids|--table|--help]

Options:
  --ids     Print buildable board IDs, one per line (default)
  --table   Print a Markdown table with release metadata
  --help    Show this help text
EOF
}

case "${1:---ids}" in
    --ids)
        mcujs_list_boards
        ;;
    --table)
        mcujs_print_board_markdown_table
        ;;
    --help|-h)
        show_help
        ;;
    *)
        printf 'Unknown option: %s\n' "$1" >&2
        show_help >&2
        exit 1
        ;;
esac
