---
description: Close out a FlickImp work session — update changelog, project brief, backlog, and memory.
allowed-tools: [Read, Write, Edit, Bash]
---

Review the work done this session using `git diff HEAD` and session context, then update all relevant documentation:

1. **CHANGELOG.md** — If the file does not exist, create it with a standard header and the current version section. Add entries under the current version for every user-visible change (features, fixes, behaviour changes). Skip purely internal refactors that don't affect users. Use the format:
   ```
   ## [0.x.y] — YYYY-MM-DD
   ### Added
   - ...
   ### Changed
   - ...
   ### Fixed
   - ...
   ```

2. **CLAUDE.md — Backlog / Shower Thoughts** — Move any items that were fully completed this session to a `## Completed` section (create it if absent, at the bottom before the SN line). Add any new backlog items discovered this session. Update the wording of items whose scope changed.

3. **PROJECT_project_brief.md** — Update if the architecture, feature set, API surface, or config format changed in a meaningful way this session. Read the file first; only edit if something actually changed.

4. **README.md** — Update the feature list, usage examples, or API notes if user-visible behaviour changed. Read first; only edit if needed.

5. **Memory** — Update `/home/iceberg/.claude/projects/-z-Nutball-Labs-FlickImp/memory/`:
   - In `MEMORY.md`: remove or update any stale entries whose facts changed this session.
   - In `postit_notes.md` (if it exists): remove any post-it items that moved into active backlog or were completed.
   - Add new memory entries (feedback, project decisions, architectural choices) for anything non-obvious that should survive to future sessions.
   - Update HWM notation in any memory file that tracks it.

Rules:
- Read each file before editing it.
- Do not fabricate changes — only document what actually happened in this session.
- Match the existing writing style and section format of each file.
- Keep entries concise — changelog, not a novel.
- After all edits, report which files were changed and which were skipped (and why).
