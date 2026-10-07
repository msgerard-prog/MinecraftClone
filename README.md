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

## Controls
Click the window to capture the mouse (Esc releases it). WASD to fly, Space up,
Left Shift down, Left Ctrl sprint.
