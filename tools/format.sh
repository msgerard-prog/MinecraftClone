#!/usr/bin/env bash
# clang-format our C++ sources in place (never third_party/).
# Usage: tools/format.sh [files...]   (no args = all of src/ and tests/)
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/clang_format_path.sh
if [ $# -eq 0 ]; then
  mapfile -t files < <(find src tests -name '*.cpp' -o -name '*.h')
else
  files=("$@")
fi
for f in "${files[@]}"; do
  "$CLANG_FORMAT" -i --style=file "$(wslpath -w "$f")"
done
