# FlickImp — Claude Code Project Context

This file is read automatically by Claude Code at session startup.
It contains standing instructions, architecture decisions, and conventions
for working on the FlickImp project. Read this before touching any code.

---

## Project Overview

**FlickImp** is [BRIEF DESCRIPTION — one sentence].
Private GitHub repo at https://github.com/Nutball-Labs/FlickImp — all work on `main` branch.

**Current version:** 0.1.0 (SN 00001)
**Config dir:** `~/.config/flickimp/`
**Build system:** CMake

---

## Developer Profile

Experienced Linux sysadmin (Perl/Python/bash), learning C++ via AI assistance.
Full root on Alma 9.x dev machine. Uses VSCode + vim. Has 40+ years sysadmin
experience — don't over-explain Linux basics. Does need help with C++ idioms.

---

## Source Files

### Library (`lib/`) — compiled into `libwatchinglib.a`

| File | Role |
|---|---|
| `lib/version.hpp` | Version macros; `APP_NAME`, `APP_VERSION`, license notice |
| `lib/flickimp.hpp` | Umbrella header — include this in consumers of the lib |
| `lib/models.hpp` | POD structs: `Show`, `Movie`; `ShowStatus`, `MovieStatus` enums |
| `lib/platform.cpp/.hpp` | XDG paths: `config_dir()`, `data_dir()`, `db_path()` |
| `lib/database.cpp/.hpp` | SQLite wrapper — CRUD for shows and movies; `DbError` exception |
| `lib/scraper.cpp/.hpp` | IMDB scraping via libcurl — episode count, title lookup (TODO) |

### Service (`service/`) — compiled into `flickimp` binary (HTTP daemon)

| File | Role |
|---|---|
| `service/main.cpp` | Arg parsing (`--port`, `--web`), web root resolution, starts server |
| `service/server.hpp/.cpp` | `httplib` HTTP server; REST API routes; JSON serialisation |

### Web frontend (`web/`) — static assets served by the daemon

| File | Role |
|---|---|
| `web/index.html` | Single-page app shell |
| `web/style.css` | Dark theme styles |
| `web/app.js` | Vanilla JS — fetch API, render shows/movies tables, add/delete/update |

### Third-party (vendored, not committed)

| Dep | File(s) | Fetched by |
|---|---|---|
| SQLite amalgamation | `third_party/sqlite3/sqlite3.{c,h}` | `scripts/get-deps.sh` |
| cpp-httplib | `third_party/httplib.h` | `scripts/get-deps.sh` |
| nlohmann/json | `third_party/json.hpp` | `scripts/get-deps.sh` |

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

- **Config format: JSON** — all configuration files use JSON; chosen for human editability without technical knowledge, flexibility, and familiarity. No INI, TOML, YAML, or custom formats.

---

## Hard Dependencies

| Tool/Lib | Install | Notes |
|---|---|---|
| g++ (C++17) | `gcc-c++` (Alma base) | GCC 11+ recommended |
| CMake 3.16+ | `cmake` (Alma base) | Build system |
| libcurl | `libcurl-devel` | IMDB scraping |
| SQLite3 amalgamation | `scripts/get-deps.sh` | Vendored in `third_party/` |
| cpp-httplib | `scripts/get-deps.sh` | Vendored in `third_party/` |
| nlohmann/json | `scripts/get-deps.sh` | Vendored in `third_party/` |

Run before first build: `sudo dnf install libcurl-devel && ./scripts/get-deps.sh`

---

## Backlog / Shower Thoughts

- **IMDB clipboard import/export** — copy IMDB URL for a show, movie, or specific episode to/from clipboard; useful for quick lookup or pasting into browser
- **systemd service unit** — ship a `flickimp.service` unit file so the daemon can be managed with `systemctl` like any other service; should handle restart-on-failure, run as a non-root user, and point to the installed binary and web assets
- **flickimp-gui** — optional Qt6 desktop GUI for direct local editing on the host machine; talks to the same SQLite database as the daemon; useful when you're sitting at the server and don't want to open a browser
- **named queues / watchlists** — configurable named queues (e.g. "Patsy", "Steve", "Together") that appear as tabs in both the web UI and the Qt GUI; each queue is an independent list of shows and movies; allows isolating solo watching from shared watching without mixing entries
- **per-queue PIN protection** — DEFERRED / LOW PRIORITY; optional PIN on individual queues to keep adult content away from kids; intentionally not implementing full security — no desire to maintain an auth system; if pursued, keep it minimal (simple PIN, no sessions, no crypto beyond basic hashing)
- **My Services** — user-configurable list of streaming services they actually subscribe to; used to filter out availability results for services they don't have; stored in config, editable from the UI; the service field on shows should draw from this list as a dropdown
- **Mark watched and advance** — one-click to increment episode (or roll to next season) without opening an edit form; most common action deserves the shortest path
- **Last-watched timestamp** — date field on show/movie records; surfaces stale entries and shows what's actively in progress
- **Cancelled/abandoned status** — additional show status beyond Watching/Paused/Finished for shows that died mid-run or that the user gave up on; keeps them out of the active list without deleting history
- **On deck view** — web UI view filtered to shows where IMDB says unwatched episodes exist ahead of current position; surfaces the CLI `--check` results directly in the browser
- **Bulk CSV import** — import an initial watchlist from a simple CSV; saves manual entry on first run

---

## Known Issues / Pending Work

- `scraper.cpp`: IMDB episode parsing implemented; depends on IMDB's `__NEXT_DATA__` JSON structure which can change — if `--check` stops returning episode data, re-examine the path under `props.pageProps`
- No IMDB ID auto-lookup on show/movie add — must enter manually for now
- No streaming availability lookup — phase 2
- No systemd unit file yet — run manually with `./build-linux/flickimp`

---

## Git Workflow

- All work on `main` branch
- Commit after each stable version cut
- Commit message format: `"Fix/Add/Update description — FlickImp vX.Y.Z (SN NNNNN)"`
- Remote: `git@github.com:Nutball-Labs/FlickImp.git` (SSH key auth)
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

- **Phase 1 (HTTP service + web UI):** Source structure complete; needs `get-deps.sh` run + first build
- **Phase 2 (IMDB scraping):** Stubs in place; implementation pending
- **Phase 3 (Streaming availability):** Out of scope — not planned
- All work on `main` branch

<!-- SN: 00001 -->
