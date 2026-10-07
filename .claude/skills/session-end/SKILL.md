---
name: session-end
description: End-of-session checklist — update ROADMAP.md, docs, run tests, commit, push at milestone end. Use when finishing a work session, a milestone, or when the user says to wrap up.
---
# Session end

1. `tools/test.sh` passes; the Stop-hook build is green. If anything is broken, fix it
   or record it in ROADMAP.md › Next as the first item with the error.
2. **ROADMAP.md:** rewrite Status (date, what works now, what's half done), rewrite
   Next (concrete ordered steps), tick finished milestones, add to Done (keep 10),
   Waiting on the user.
3. **Docs** touched by this session's changes: architecture.md, data-formats.md,
   game-design.md (Known deviations), subsystem CLAUDE.md, new ADRs. Root CLAUDE.md
   still under 150 lines (`wc -l CLAUDE.md`).
4. Skills: if a step's real API differed from a skill's instructions, fix the skill.
5. Commit everything (`git status` clean). Message: `<area>: <what>` + body.
6. **Milestone finished?** Run the `code-reviewer` agent on the milestone's diff
   (`git diff <milestone-start>..HEAD`) and `design-keeper` on its features; fix
   findings; then `git push origin main`.
7. Report to the user: what was done, what you verified (and how), screenshots,
   anything that needs their decision.
