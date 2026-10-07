# Architecture decision records

One short file per decision that had real alternatives: `NNNN-kebab-title.md`, numbered
in order. Never edit an accepted ADR's decision — supersede it with a new one and set
the old one's status to `Superseded by NNNN`.

Template:
```markdown
# NNNN. Title
- Status: Proposed | Accepted (YYYY-MM-DD) | Superseded by NNNN
- Context: what forced the decision (2–4 lines)
- Decision: what we do
- Alternatives: what else was considered and why not
- Consequences: what this makes easier / harder
```

| # | Decision | Status |
|---|---|---|
| 0001 | C++20 + OpenGL 4.6, MSVC driven from WSL | Accepted |
| 0002 | Reference version: Java Edition 1.21.x | Accepted |
| 0003 | Dependencies via pinned FetchContent | Accepted |
| 0004 | No Mojang code or assets in the repo | Accepted |
| 0005 | Dense global block-state ids, paletted sections | Accepted |
| 0006 | Original textures in vanilla's style, sharper than vanilla | Accepted |
