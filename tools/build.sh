#!/usr/bin/env bash
# Build with MSVC + Ninja from WSL.  Usage: tools/build.sh [debug|release]
set -euo pipefail
cd "$(dirname "$0")/.."
preset="${1:-debug}"
cmd.exe /d /c "tools\\win\\build.cmd $preset" 2>&1 | tr -d '\r'
status="${PIPESTATUS[0]}"
# Keep ./compile_commands.json (WSL paths, for clangd) in sync with the debug build.
if [ "$status" -eq 0 ] && [ "$preset" = debug ]; then python3 tools/clangd_db.py debug; fi
exit "$status"
