#!/usr/bin/env bash
# Render N frames in a hidden window and save a PNG for visual checks.
# Usage: tools/screenshot.sh [name] [game args...]   -> out/screenshots/<name>.png
# Example: tools/screenshot.sh spawn --seed 42 --frames 120
set -euo pipefail
cd "$(dirname "$0")/.."
name="${1:-shot}"
shift || true
out="out/screenshots/$name.png"
rm -f "$out"
# Debug build: it has the GL debug context, so GL errors show up in the log.
tools/run.sh debug --hidden --screenshot "$out" "$@"
[ -f "$out" ] || { echo "error: no screenshot produced" >&2; exit 1; }
echo "$out"
