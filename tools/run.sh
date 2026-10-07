#!/usr/bin/env bash
# Build and launch the game.  Usage: tools/run.sh [debug|release] [game args...]
set -euo pipefail
cd "$(dirname "$0")/.."
preset=debug
case "${1:-}" in debug|release) preset="$1"; shift ;; esac
tools/build.sh "$preset" >/dev/null || tools/build.sh "$preset"
"out/build/$preset/MinecraftClone.exe" "$@" 2>&1 | tr -d '\r'
exit "${PIPESTATUS[0]}"
