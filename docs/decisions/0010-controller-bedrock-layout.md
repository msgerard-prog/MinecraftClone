# 0010. Controller support with Bedrock's layout (a deviation from Java)
- Status: Accepted (2026-10-09, user request: "Yes, follow bedrock layout for controllers")
- Context: Vanilla Java Edition has no controller support (players use mods such as
  Controlify). Bedrock Edition supports controllers natively. The user asked for controller
  support with Bedrock's layout.
- Decision: `core/Gamepad` maps the first connected controller with a standard (Xbox-style)
  mapping (GLFW gamepad API) to Bedrock's default layout (wiki: Controls): left stick move
  (analog) / L3 sprint, right stick look / R3 fly down, A jump, B sneak, X crafting, Y
  inventory, LB/RB hotbar, LT use / RT attack, D-pad up perspective / down drop / right
  chat, Menu pause. On screens the left stick moves the cursor, A clicks, X right-clicks
  (half / one), Y quick-moves (shift-click), B goes back, the right stick scrolls. `Window`
  feeds the result into the existing press counters, held keys and mouse deltas, so game
  code reads keyboard, mouse and controller alike. Always on when a controller is present.
- Best assumptions (the wiki documents the in-game layout, not the controller inventory):
  the screen-button roles above; dead zone 0.2 (radial), triggers at half pull, look speed
  ~270 degrees a second at full tilt and default sensitivity (squared curve); L3 sprint stays
  on until the stick is released; X opens the inventory (its 2x2 grid is our crafting
  screen); D-pad left (emote) and View (effects drawer) are unassigned - we have neither.
- Alternatives: Java mods' layouts (Controlify); a remappable layout (later, if wanted).
- Consequences: no rumble, no on-screen button prompts, no controller typing in chat.
