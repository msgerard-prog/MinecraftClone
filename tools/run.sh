#!/usr/bin/env bash
# Build and launch the game.  Usage: tools/run.sh [debug|release] [game args...]
# Release by default (v1.5.1): the debug build's checked STL is ~10x slower to stream and
# mesh, which made play look broken. `tools/run.sh debug` for debugging.
set -euo pipefail
cd "$(dirname "$0")/.."
preset=release
case "${1:-}" in debug|release) preset="$1"; shift ;; esac
tools/build.sh "$preset" >/dev/null || tools/build.sh "$preset"
"out/build/$preset/MinecraftClone.exe" "$@" 2>&1 | tr -d '\r'
exit "${PIPESTATUS[0]}"
