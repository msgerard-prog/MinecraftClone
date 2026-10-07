#!/usr/bin/env bash
# Stop hook: incremental debug build so Claude can't finish on broken code.
# Exit 2 + stderr = Claude is told the errors and keeps working.
# Skips when nothing build-relevant changed since the last good build.
# Gives up after 3 consecutive blocks so a stuck fix can't loop forever.
cd "$CLAUDE_PROJECT_DIR" || exit 0
input="$(cat)"
stamp=out/.last-good-build
strikes=out/.stop-build-strikes
mkdir -p out

changed="$(find src tests cmake third_party CMakeLists.txt CMakePresets.json \
  -newer "$stamp" -type f -print -quit 2>/dev/null)"
if [ -f "$stamp" ] && [ -z "$changed" ]; then
  exit 0
fi

if output="$(tools/build.sh debug 2>&1)"; then
  touch "$stamp"
  rm -f "$strikes"
  exit 0
fi

count=$(( $(cat "$strikes" 2>/dev/null || echo 0) + 1 ))
echo "$count" > "$strikes"
active="$(printf '%s' "$input" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("stop_hook_active", False))' 2>/dev/null)"
if [ "$count" -gt 3 ] && [ "$active" = "True" ]; then
  rm -f "$strikes"
  echo '{"systemMessage": "Stop hook: debug build is STILL FAILING after 3 attempts. Claude was allowed to stop; run tools/build.sh to see the errors."}'
  exit 0
fi

{
  echo "The debug build is failing (tools/build.sh debug). Fix it before finishing:"
  printf '%s\n' "$output" | grep -E "error|FAILED|undefined|unresolved" | head -30
  echo "--- last lines ---"
  printf '%s\n' "$output" | tail -15
} >&2
exit 2
