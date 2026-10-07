#!/usr/bin/env bash
# PostToolUse (Edit|Write|MultiEdit): clang-format the edited C++ file in place.
# Never fails the tool call; formatting problems are only reported.
cd "$CLAUDE_PROJECT_DIR" || exit 0
file="$(python3 -c 'import json,sys; print(json.load(sys.stdin).get("tool_input",{}).get("file_path",""))')"
case "$file" in
  *.cpp|*.h|*.hpp|*.c) ;;
  *) exit 0 ;;
esac
case "$file" in
  */third_party/*|*/out/*) exit 0 ;;
esac
[ -f "$file" ] || exit 0
source tools/clang_format_path.sh
"$CLANG_FORMAT" -i --style=file "$(wslpath -w "$file")" 2>&1 | tr -d '\r' >&2 || true
exit 0
