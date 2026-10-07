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
tools/run.sh              # build + launch
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
world, `--no-save` plays without saving, `--generator terrain` creates a world with the
old M3 placeholder terrain instead of the 1.21-style overworld. Screenshot/benchmark runs don't save unless
given `--world`.

## Controls
Click the window to capture the mouse (Esc releases it). WASD to walk, Space to jump,
Left Shift to sneak, Left Ctrl to sprint. Double-tap Space to start/stop flying
(creative); while flying, Space rises and Shift descends. Left click breaks the block
under the crosshair, right click places the selected block; 1-9 or the mouse wheel
select it in the hotbar. F3 toggles the debug screen. T opens chat, / opens it with a
command: `/tp x y z` (`~` = relative), `/time set day|noon|night|midnight|<ticks>`,
`/time add <n>[d|s|t]`, `/time query daytime|gametime|day`, `/give @s <block>`
(into the selected slot), `/seed`, `/help`. Enter sends, Esc cancels, Up/Down recall. E opens the inventory: in creative, every item (click to pick up, click a hotbar slot to
put it there, or hover and press 1-9); in survival, your inventory with a 2x2 crafting
grid. `/gamemode survival` switches to survival: blocks take time to break (tools help),
drop items you pick up by walking over them, health and hunger matter, falls hurt.
Right-click a crafting table for 3x3 recipes or a furnace to smelt (fuel below, input
above). Q drops the held item; `/kill` respawns you.
Cows graze in grassy biomes and zombies come out in the dark (they burn in daylight);
left-click a mob to hit it. `/summon zombie|cow [x y z]` spawns one; F3 shows how
many hostile mobs are around.
