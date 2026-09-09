#!/usr/bin/env python3
"""Report a CMake-selected profile; this script never chooses/enables hardware."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--board', required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--configured-display', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
# A board identifier is a basename, not an arbitrary input path.
if not args.board or Path(args.board).name != args.board or '/' in args.board or '\\' in args.board:
    parser.error('invalid board identifier')
manifest = json.loads((root / 'runtime/manifests' / (args.board + '.json')).read_text())
if args.configured_display:
    manifest['capabilities']['devices'] = json.loads((root / 'runtime/device-capabilities.json').read_text())
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(manifest, indent=2) + '\n')
