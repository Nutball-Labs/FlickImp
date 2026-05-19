# FlickImp — Claude Code Project Context

This file is read automatically by Claude Code at session startup.
It contains standing instructions, architecture decisions, and conventions
for working on the FlickImp project. Read this before touching any code.

---

## Project Overview

**FlickImp** is [BRIEF DESCRIPTION — one sentence].
Private GitHub repo at https://github.com/Nutball-Labs/FlickImp — all work on `main` branch.

**Current version:** 0.2.1a (SN 00002)
**Config file:** `/etc/flickimp/fi_config.json` (system service) · `~/.config/flickimp/fi_config.json` (dev user)
**Build system:** CMake

---

## Developer Profile

Experienced Linux sysadmin (Perl/Python/bash), learning C++ via AI assistance.
Full root on Alma 9.x dev machine. Uses VSCode + vim. Has 40+ years sysadmin
experience — don't over-explain Linux basics. Does need help with C++ idioms.

---

## Source Files

### Library (`lib/`) — two static libs

**`flickimpcorelib`** (platform + config only — no SQLite/curl; linked by daemon and Qt app):

| File | Role |
|---|---|
| `lib/platform.cpp/.hpp` | XDG/AppData/Library paths: `config_dir()`, `data_dir()`, `db_path()` |
| `lib/config.cpp/.hpp` | JSON config file (`fi_config.json`): `load_config()`, `save_config()`; fields: port, web_root, tmdb_api_key, tmdb_bearer_token |

**`flickimplib`** (full library — links flickimpcorelib + adds SQLite/curl):

| File | Role |
|---|---|
| `lib/version.hpp` | Version macros; `APP_NAME`, `APP_VERSION`, license notice |
| `lib/flickimp.hpp` | Umbrella header — include this in consumers of the lib |
| `lib/models.hpp` | POD structs: `Show`, `Movie`; `ShowStatus`, `MovieStatus` enums |
| `lib/database.cpp/.hpp` | SQLite wrapper — CRUD for shows and movies; `DbError` exception |
| `lib/scraper.cpp/.hpp` | TMDB REST API (primary): show info, season list, episode list; IMDB scraping (secondary, `--check` only) |

### Service (`service/`) — compiled into `flickimp` binary (HTTP daemon)

| File | Role |
|---|---|
| `service/main.cpp` | Loads `config.json`, arg parsing (`--port`, `--web`, `--check`), starts server |
| `service/server.hpp/.cpp` | `httplib` HTTP server; REST API routes; JSON serialisation |
| `service/flickimp.service` | systemd unit template (`User=FLICKIMP_USER` placeholder) |

### Qt configurator (`gui/`) — compiled into `flickimp-config` binary (Linux only)

| File | Role |
|---|---|
| `gui/main.cpp` | Qt application entry point |
| `gui/MainWindow.h/.cpp` | Service control window: start/stop/restart, boot toggle, port, browser |

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
- There is one project-wide **high-water mark** SN, currently `00002`
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
- **named queues / watchlists** — configurable named queues (e.g. "Patsy", "Steve", "Together") that appear as tabs in both the web UI and the Qt GUI; each queue is an independent list of shows and movies; allows isolating solo watching from shared watching without mixing entries
- **per-queue PIN protection** — DEFERRED / LOW PRIORITY; optional PIN on individual queues to keep adult content away from kids; intentionally not implementing full security — no desire to maintain an auth system; if pursued, keep it minimal (simple PIN, no sessions, no crypto beyond basic hashing)
- **My Services** — user-configurable list of streaming services they actually subscribe to; used to filter out availability results for services they don't have; stored in config, editable from the UI; the service field on shows should draw from this list as a dropdown
- **Mark watched and advance** — one-click from the card to increment episode (or roll to next season) without opening the popup; most common action deserves the shortest path
- **Last-watched timestamp** — date field on show/movie records; surfaces stale entries and shows what's actively in progress
- **Cancelled/abandoned status** — additional show status beyond Watching/Paused/Finished for shows that died mid-run or that the user gave up on; keeps them out of the active list without deleting history
- **On deck view** — web UI view filtered to shows where TMDB says unwatched episodes exist ahead of current position
- **Bulk CSV import** — import an initial watchlist from a simple CSV; saves manual entry on first run
- **Multi-daemon / multi-instance support** — user wants to run more than one daemon on the same host; open design question: one `fi_config.json` with per-instance sections, or a separate config file path per instance baked into each systemd service unit file; resolve before finalising config/service architecture
- **VERSION_SUFFIX not picked up by package build** — package build scripts are not consuming VERSION_SUFFIX; investigate and fix so alpha/beta/rc suffixes appear correctly in package filenames and metadata
- **flickimp-config identity header** — the Qt configurator window should display the FlickImp logo and version string; combine the About information with the config UI rather than a separate dialog; surfaces branding and makes the version immediately visible to the user
- **[ROADMAP — way down the road]** User-level (non-system-service) deployment mode — run flickimp as a regular user without a dedicated service account; defer until core application is stable and the system-service path is well-exercised
- **Season select-all checkbox** — in the season/episode picker popup, a checkbox on each season button that marks all episodes of that season as watched in one click
- **RPM package arch in filename** — should the RPM use `noarch` or `x86_64`? FlickImp compiles native C++ so `x86_64` is technically correct, but investigate whether the CPack config is setting arch correctly and whether `noarch` would ever be appropriate
- **Diagnose IMDB back-fill failure** — rebuild with logging already added, add a show via TMDB search, check `journalctl -u flickimp -n 30`; three possible outcomes: `tmdb_id=0` (browser serving cached old app.js), `request failed` (/external_ids network or auth error), `no imdb_id in TMDB response` (show has no IMDB ID on TMDB)
- **Movie release date label — tense-aware** — if a movie has a future release date it should say "Releases" or "Coming on:" not "Released"; if no date is set yet it should say "No release date yet."

## Completed

- **systemd service unit** — `flickimp.service` ships with the project; runs as dedicated `flickimp` user; no env file; reads all config from `fi_config.json` on startup
- **flickimp-gui (`flickimp-config`)** — Qt6 service control app; manages start/stop/restart/boot-enable and exposes port + TMDB credentials (API Key and Bearer Token)
- **TMDB as primary data source** — `scraper.cpp` uses TMDB REST API for season lists and episode details; Bearer token preferred, v3 API key as fallback; IMDB scraping retained for `--check` only
- **Season/episode picker popup** — clicking "Last watched" on a show card opens a modal with colour-coded seasons (green/orange/red) and a per-season episode grid with checkboxes; checking an episode marks it watched and updates the card live
- **`fi_` config file naming convention** — all FlickImp config files use `fi_` prefix; config is `fi_config.json`; env file eliminated
- **TMDB search-on-add** — adding a show or movie by typing a name searches TMDB and shows up to 5 results (title, year, poster); selecting a result auto-populates TMDB ID, thumbnail, and metadata
- **TMDB search-on-add IMDB ID back-fill** — `POST /api/shows` calls `/tv/{id}/external_ids` to populate `imdb_id` when a show is added via search; `POST /api/movies` does the same via `/movie/{id}/external_ids` and also stores `tmdb_id` + always fetches release date on add
- **Check All web UI** — ☰ menu in the header with "Check All"; opens a live-scrolling log modal streaming from `GET /api/check/stream` (SSE); checks all active shows for new episodes and back-fills missing movie release dates; status badge shows Running → Done / N NEW; cards refresh on completion

---

## Known Issues / Pending Work

- `scraper.cpp`: IMDB episode parsing depends on IMDB's `__NEXT_DATA__` JSON structure which can change — if `--check` stops returning episode data, re-examine the path under `props.pageProps`
- No streaming availability lookup — phase 2

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

## Recording Deferred Thoughts

Use the `/postit` slash command to record an idea without acting on it. It writes to the project memory and appends to the Backlog section above. The old "Shower Thought" / "Workout Thought" preamble convention is also still recognised — treat it the same way.

---

## Phase Status

- **Phase 1 (HTTP service + web UI):** Complete and building
- **Phase 2 (Episode data):** Complete — TMDB REST API is primary source; IMDB scraping retained for `--check` new-episode detection
- **Phase 3 (Streaming availability):** Out of scope — not planned
- **Phase 4 (systemd + Qt configurator):** Complete — `flickimp-config` Qt app; `scripts/install-service.sh`
- **Phase 5 (Season/episode picker):** Complete — modal popup with colour-coded season list and per-episode watched checkboxes
- All work on `main` branch

<!-- SN: 00002 -->
