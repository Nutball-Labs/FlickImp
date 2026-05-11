# Watching — Claude Code Project Context

This file is read automatically by Claude Code at session startup.
It contains standing instructions, architecture decisions, and conventions
for working on the Watching project. Read this before touching any code.

---

## Project Overview

**Watching** is [BRIEF DESCRIPTION — one sentence].
Private GitHub repo at https://github.com/Nutball-Labs/Watching — all work on `main` branch.

**Current version:** 0.1.0 (SN 00001)
**Config dir:** `~/.config/watching/`
**Build system:** CMake

---

## Developer Profile

Experienced Linux sysadmin (Perl/Python/bash), learning C++ via AI assistance.
Full root on Alma 9.x dev machine. Uses VSCode + vim. Has 40+ years sysadmin
experience — don't over-explain Linux basics. Does need help with C++ idioms.

---

## Source Files

*(No source files written yet — scaffolding phase.)*

### Library (`lib/`) — compiled into `libwatchinglib.a`

| File | Role |
|---|---|
| `lib/platform.cpp/.hpp` | OS abstraction: home/config paths — `namespace Watching::Platform` |
| `lib/version.hpp` | Version string from components via macros |
| `lib/watching.hpp` | Umbrella public API header |

### CLI front-end (`cli/`) — compiled into `watching` binary

| File | Role |
|---|---|
| `cli/main.cpp` | CLI argument parsing, orchestration |

### GUI (`gui/`) — compiled into `watching-gui` binary (Qt6)

| File | Role |
|---|---|
| `gui/main.cpp` | Qt application entry point |
| `gui/MainWindow.cpp/.h` | Main window |
| `gui/AboutDialog.cpp/.h` | About dialog (Nutball-Labs logo) |

---

## Serial Number (SN) Convention

Every source file carries a serial number comment at the bottom of the file:

```cpp
// SN: 00001       ← C++ files and headers
# SN: 00001        ← cmake files and shell scripts
<!-- SN: 00001 --> ← Markdown (.md) files (HTML comment — invisible when rendered)
```

**Rules:**
- There is one project-wide **high-water mark** SN, currently `00001`
- When files are modified in a build/fix session, bump their SN to the
  current high-water mark
- When cutting a new release, increment the high-water mark by 1 and apply
  it to ALL files touched in that release
- Files that were NOT changed in a session keep their existing SN — do not
  bump unchanged files
- Use `^// SN:` and `^# SN:` grep anchors to find/replace SNs and avoid
  false matches in comments or documentation
- Run `cmake --build build-linux --target sn-audit` to regenerate `sn_audit.txt`
- `sn_audit.cmake` uses **last-match-wins** per file — intentional, prevents doc
  examples in CLAUDE.md from generating false duplicate entries. Do not change this.

**Example workflow:**
- Working on a bug fix: change the files, bump their SN to current HWM
- Cutting a release: increment HWM (e.g. 00001 → 00002), apply to all changed
  files, update `version.hpp` with new patch/minor version

---

## Deliverable Format

**When working in VSCode + Claude Code (current setup):** files are edited
in-place and delivered via `git push origin main`.
"Cut the tarball" in this context means: implement changes, verify clean build,
bump SNs, bump version, commit, and push.

---

## Architecture — Critical Decisions

*(Document project-specific architecture decisions here as they are made.)*

---

## Hard Dependencies

| Tool/Lib | Install | Notes |
|---|---|---|
| g++ (C++17) | `gcc-c++` (Alma base) | GCC 11+ recommended |
| CMake 3.16+ | `cmake` (Alma base) | Build system |

---

## Known Issues / Pending Work

- Source files not yet written — scaffolding phase only

---

## Git Workflow

- All work on `main` branch
- Commit after each stable version cut
- Commit message format: `"Fix/Add/Update description — Watching vX.Y.Z (SN NNNNN)"`
- Remote: `git@github.com:Nutball-Labs/Watching.git` (SSH key auth)
- After committing: `git push origin main`

---

## What "Cut the Tarball" Means

When working in **VSCode + Claude Code**, "cut the tarball" or "cut as X.Y.Z" means:
1. Implement all requested changes
2. Verify `cmake --build` compiles with no errors
3. Note warnings — fix if trivial, add to queue if not
4. Bump SN on all changed files to current high-water mark
5. If this is a new version: increment HWM, update `version.hpp`
6. Commit and `git push origin main`
7. Summarize what changed

No tarball file is produced — `git push` is the delivery mechanism.

---

## What "Shower Thought" or "Workout Thought" Means

When a communication is prefaced with either label:
- Thought is to be recorded into knowledge base for the project.
- Add an entry on the ToDo list for the thought.
- Do not take any action at that time.
- Virtual Post-It note stuck on the virtual keyboard.

---

## Phase Status

- **Phase 1 (CLI):** Not yet started — architecture planned
- **Phase 2 (Qt6 GUI):** Not yet started — architecture planned
- All work on `main` branch

<!-- SN: 00001 -->
