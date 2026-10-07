#!/usr/bin/env bash
# Build and run all doctest tests.  Usage: tools/test.sh [debug|release] [doctest args...]
# Extra args go straight to doctest, e.g.  tools/test.sh debug -tc="*chunk*"
set -euo pipefail
cd "$(dirname "$0")/.."
preset="${1:-debug}"
shift || true
if [ $# -eq 0 ]; then
  cmd.exe /d /c "tools\\win\\build.cmd $preset test" 2>&1 | tr -d '\r'
  exit "${PIPESTATUS[0]}"
fi
tools/build.sh "$preset"
"out/build/$preset/tests/mc_tests.exe" "$@" 2>&1 | tr -d '\r'
exit "${PIPESTATUS[0]}"
