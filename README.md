# MinecraftClone

A from-scratch replica of **Minecraft Java Edition 1.21.x** in C++20 and OpenGL 4.6 —
a non-commercial learning project to understand how vanilla Minecraft works.
Not affiliated with Mojang or Microsoft; contains no Mojang code or assets.

## Status
See [ROADMAP.md](ROADMAP.md). Currently: M0 (project setup) done — a window with a
hello-triangle, tests, and screenshot tooling.

## Requirements
- Windows 10/11 with **Visual Studio 2022+** ("Desktop development with C++",
  which includes CMake, Ninja and clang tools)
- An OpenGL 4.6 GPU
- WSL (the `tools/*.sh` scripts) — or build directly from a VS Developer prompt

## Build and run
From WSL:
```bash
tools/build.sh            # debug build   (first build downloads deps, ~1 min)
tools/test.sh             # build + run tests
tools/run.sh              # build + launch (release; `tools/run.sh debug` for the debug build)
tools/screenshot.sh hello # render to out/screenshots/hello.png and exit
```
From a Visual Studio Developer Command Prompt:
```bat
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
out\build\debug\MinecraftClone.exe
```
Or open the folder in Visual Studio (it reads `CMakePresets.json`).

## Command-line options
| Option | Meaning |
|---|---|
| `--size WxH` | Window size (default 1280x720) |
| `--seed N` | World seed |
| `--screenshot PATH` | Render, save a PNG to PATH, exit |
| `--frames N` | Frames to render before the screenshot (default 60) |
| `--hidden` | No visible window |
| `--no-vsync` | Uncapped frame rate (for measuring performance) |
| `--render-distance N` | Chunks loaded and drawn around you (2..32, default 12) |
| `--flat` | The superflat test world instead of generated terrain |
| `--auto-fly` / `--max-fps N` | Streaming benchmark: fly forward fast / cap the frame rate |
| `--demo-edit` | Scripted break/place clicks after loading (visual test) |
| `--resourcepacks DIR` | Folder of resource packs to load (default `resourcepacks/`) |
| `--pos x,y,z` | Start position (blocks) |
| `--look yaw,pitch` | Start rotation, vanilla degrees (yaw 0 = south, pitch +90 = down) |

## Layout
```
src/{core,world,rendering,audio,gameplay,ui}   subsystems (see docs/architecture.md)
assets/                                        shaders, our placeholder resource pack, data JSON
tests/                                         doctest unit tests
docs/                                          architecture, game design, data formats, ADRs
tools/                                         build/test/run/screenshot scripts
```

## Using your own Minecraft textures
The repo only contains our own placeholder textures. To see the real ones on your
machine, put your own copy of Minecraft's textures in the git-ignored `resourcepacks/`
folder. The game reads folders, resource-pack `.zip` files, and the client `.jar`
directly (a jar is a zip with `assets/` inside):
```bat
mkdir resourcepacks
copy "%APPDATA%\.minecraft\versions\1.21.10\1.21.10.jar" resourcepacks\
```
(Use the version you have installed; launch it once in the official launcher first so
the jar exists.) Packs load in alphabetical order, later ones override earlier ones
file by file, and anything missing falls back to our placeholders. Only blocks the
clone has implemented show up, of course. `--resourcepacks DIR` points at another
folder. Never commit these files (ADR 0004).

## Worlds
The game saves to `saves/New World/` (vanilla's Anvil layout) when you quit and every
5 minutes; it continues that world on the next start. `--world NAME` picks another
world, `--no-save` plays without saving, `--generator overworld` creates a world with the
M8 overworld (no ravines, structures or the M18 biomes) and `--generator terrain` one
with the old M3 placeholder terrain instead of the default "overworld2". Screenshot/benchmark runs don't save unless
given `--world`.

## Controls
`tools/run.sh` opens the title screen: Singleplayer lists your worlds (or create one: name, seed, Survival/Creative, Default/Superflat); Options... sets FOV, render/simulation distance, sensitivity, GUI scale, volume, clouds, VSync, hotbar numbers, Display (Windowed / Fullscreen), Resolution (a drop-down list) and Borderless Window (saved in `options.txt`); F11 toggles full screen. In a world, Esc opens the Game Menu (the game pauses; Save and Quit to Title). Click the window to capture the mouse. WASD to walk, Space to jump,
Left Shift to sneak, Left Ctrl to sprint. Double-tap Space to start/stop flying
(creative); while flying, Space rises and Shift descends. Left click breaks the block
under the crosshair, right click places the selected block; 1-9 or the mouse wheel
select it in the hotbar. F3 toggles the debug screen. F5 switches the view (first person, third person behind, in front). T opens chat, / opens it with a
command: `/tp x y z` (`~` = relative), `/time set day|noon|night|midnight|<ticks>`,
`/time add <n>[d|s|t]`, `/time query daytime|gametime|day`, `/give @s <block>`
(into the selected slot), `/seed`, `/help`. Enter sends, Esc cancels, Up/Down recall. E opens the inventory: in creative, every item (click to pick up, click a hotbar slot to
put it there, or hover and press 1-9); in survival, your inventory with a 2x2 crafting
grid. `/gamemode survival` switches to survival: blocks take time to break (tools help),
drop items you pick up by walking over them, health and hunger matter, falls hurt.
Right-click a crafting table for 3x3 recipes or a furnace to smelt (fuel below, input
above). Q drops the held item (Ctrl+Q the whole stack), F swaps it with the offhand; in inventory screens drag a stack across slots to split it (right-drag: one each), double-click to gather a kind, press 1-9 or F over a slot to swap it with the hotbar or offhand, Q over a slot to drop from it; `/kill` respawns you.
Cows graze in grassy biomes and zombies come out in the dark (they burn in daylight);
left-click a mob to hit it. `/summon zombie|cow [x y z]` spawns one; F3 shows how
many hostile mobs are around.
Redstone: dust (the `redstone` item), redstone torches, repeaters (right-click for the
delay), levers and buttons (right-click), blocks of redstone, lamps and pistons work
like vanilla's, including torch burnout, repeater locking and quasi-connectivity.
`/setblock x y z <block>` places any block state, e.g. `repeater[facing=west,delay=3]`;
`/fill x1 y1 z1 x2 y2 z2 <block>` fills a box.
The Nether and the End: build an obsidian frame (inside 2x3 or larger) and right-click
inside it with flint and steel (iron ingot + flint), then stand in the portal (4 s in
survival). Twelve end portal frames around a 3x3 hole, each given an eye of ender
(creative inventory), open an end portal; the End's exit portal brings you back.
`--dimension nether|end` starts a run there.


**Xbox-style controller** (Bedrock Edition's layout; plug it in before or while playing):
left stick move (click: sprint), right stick look (click: fly down), A jump, B sneak, X crafting,
Y inventory, LB/RB hotbar, LT use/place, RT attack/break, D-pad up view (F5), down drop,
right chat, Menu pause. In menus and inventories: left stick moves the cursor, A click, X
right-click (half a stack / one item), Y quick-move, B back, right stick scrolls.