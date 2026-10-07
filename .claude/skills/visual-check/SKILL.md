---
name: visual-check
description: Verify a visual change by rendering a screenshot and looking at it. Use after any change that affects what is drawn, and whenever the user asks how something looks.
---
# Visual check

1. Pick a scripted view that shows the change:
   `tools/screenshot.sh <name> [--seed N] [--frames N] [--size WxH] [--pos x,y,z --look yaw,pitch]`
   (`--pos/--look` exist from M1). Use enough `--frames` for chunks to load (worlds:
   start at 120). Release build is used for speed.
2. The script prints `out/screenshots/<name>.png`. **Read the PNG.** Never claim a
   visual result you haven't looked at.
3. Check the run log for `[error]` / `[warn] GL` lines; a new GL error is a bug.
   For before/after or multi-view checks, combine shots into one image:
   `tools/stitch_png.py out/screenshots/<name>-all.png a.png b.png ...` and Read that.
4. Judge it against expectations and vanilla: orientation, colours, missing faces,
   z-fighting, seams, light levels. Compare with an earlier screenshot of the same
   view when changing existing visuals (name them `<feature>-before/after`).
5. Report what you saw in words, plus the path. Fix defects now; log purely cosmetic
   issues in ROADMAP.md › Backlog (user preference: basic graphics first).
6. To show the user, send the PNG (SendUserFile) when they're remote.
