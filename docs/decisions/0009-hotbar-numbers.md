# 0009. Hotbar slot numbers (a deviation from vanilla)
- Status: Accepted (2026-10-09, user request)
- Context: Play-testing, the user found the hotbar confusing: nothing tells which number key
  selects which slot, so players count slots. Vanilla Java shows no labels.
- Decision: an option "Hotbar Numbers" (Options screen; `clone_hotbarNumbers` in options.txt),
  on by default, draws each slot's key 1-9 in its top-left corner over the item, at half size
  where whole screen pixels allow (GUI scale 2+), full size at GUI scale 1.
- Alternatives: off by default (vanilla's look first - but the user asked for it to help
  players); numbers under the bar (collides with nothing, but further from the slot and lost
  behind the item tooltip/action bar text).
- Consequences: OFF restores vanilla's hotbar exactly; vanilla ignores the unknown options.txt
  key.
