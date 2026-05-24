# FlickImp — Claude Code Project Context

This file is read automatically by Claude Code at session startup.
It contains standing instructions, architecture decisions, and conventions
for working on the FlickImp project. Read this before touching any code.

---

## Project Overview

**FlickImp** is [BRIEF DESCRIPTION — one sentence].
Private GitHub repo at https://github.com/Nutball-Labs/FlickImp — all work on `main` branch.

**Current version:** 1.0.0 (SN 00004)
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
| `lib/scraper.cpp/.hpp` | TMDB REST API only: show info, season list, episode list, `--check` new-episode detection; IMDB IDs used as lookup keys into TMDB, not scraped directly |

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
| `gui/resources.qrc` | Qt resource bundle — embeds `FlickImp_icon.png` for the identity header |

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
- There is one project-wide **high-water mark** SN, currently `00004`
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
| libcurl | `libcurl-devel` | HTTP requests to TMDB API |
| SQLite3 amalgamation | `scripts/get-deps.sh` | Vendored in `third_party/` |
| cpp-httplib | `scripts/get-deps.sh` | Vendored in `third_party/` |
| nlohmann/json | `scripts/get-deps.sh` | Vendored in `third_party/` |

Run before first build: `sudo dnf install libcurl-devel && ./scripts/get-deps.sh`

---

## Backlog / Shower Thoughts

- **Next episode air date on show card** — show the air date alongside the "Next:" line; two distinct cases: (1) caught-up shows — use `next_episode_to_air` from TMDB `/tv/{id}` response (already in `fetch_show_info` payload, zero extra API calls) to show when the upcoming episode airs; (2) behind shows — need the specific historic air date of `show.episode+1`, requires storing per-episode data or a targeted `fetch_season_episodes` call during `--check`; implement case 1 first (most useful for currently-airing shows); requires new `next_air_date TEXT` field on Show + DB migration
- **IMDB clipboard import/export** — copy IMDB URL for a show, movie, or specific episode to/from clipboard; useful for quick lookup or pasting into browser
- **named queues / watchlists** — configurable named queues (e.g. "Patsy", "Steve", "Together") that appear as tabs in both the web UI and the Qt GUI; each queue is an independent list of shows and movies; allows isolating solo watching from shared watching without mixing entries
- **per-queue PIN protection** — DEFERRED / LOW PRIORITY; optional PIN on individual queues to keep adult content away from kids; intentionally not implementing full security — no desire to maintain an auth system; if pursued, keep it minimal (simple PIN, no sessions, no crypto beyond basic hashing)
- **My Services** — user-configurable list of streaming services they actually subscribe to; used to filter out availability results for services they don't have; stored in config, editable from the UI; the service field on shows should draw from this list as a dropdown
- **Mark watched and advance** — one-click from the card to increment episode (or roll to next season) without opening the popup; most common action deserves the shortest path
- **Last-watched timestamp** — date field on show/movie records; surfaces stale entries and shows what's actively in progress
- **Cancelled/abandoned status** — additional show status beyond Watching/Finished for shows that died mid-run or that the user gave up on; keeps them out of the active list without deleting history (note: Paused status removed from UI in v1.0; DB enum still has the value)
- **On deck view** — web UI view filtered to shows where TMDB says unwatched episodes exist ahead of current position
- **Bulk CSV import** — import an initial watchlist from a simple CSV; saves manual entry on first run
- **Multi-daemon / multi-instance support** — user wants to run more than one daemon on the same host; open design question: one `fi_config.json` with per-instance sections, or a separate config file path per instance baked into each systemd service unit file; resolve before finalising config/service architecture
- **[ROADMAP — way down the road]** User-level (non-system-service) deployment mode — run flickimp as a regular user without a dedicated service account; defer until core application is stable and the system-service path is well-exercised
- **Add lightweight user+PIN auth to CLI and web UI so non-root users can run --check without sudo** — pin access for CLI and web UI. This will facilitate at least a small version of parental controls into the app.
- **Add Help page to hamburger menu** — Help page with screenshots; screenshots now available in `screenshots/`; page not yet built
- **Fix episode browser logo crop on initial load** — seasons pane height equals season count before a season is selected, cropping the background logo watermark; selecting a season expands the pane and fixes it; ensure minimum height covers the logo regardless of selection state
- **Main page hero + Shows/Movies tabs** — show a hero thumbnail of the last-watched item at top of main page; split shows and movies into separate tabs below; tab bar is extensible for future user profiles and PIN-protected queues

## Completed

- **systemd service unit** — `flickimp.service` ships with the project; runs as dedicated `flickimp` user; no env file; reads all config from `fi_config.json` on startup
- **flickimp-gui (`flickimp-config`)** — Qt6 service control app; manages start/stop/restart/boot-enable and exposes port + TMDB credentials (API Key and Bearer Token)
- **TMDB as sole data source** — `scraper.cpp` uses TMDB REST API for all data: show info, season lists, episode details, and `--check` new-episode detection; IMDB IDs are used as lookup keys into TMDB (`/find/{tt}?external_source=imdb_id`), not scraped directly; Bearer token preferred, v3 API key as fallback
- **`--check` credential guard bug** — `fetch_show_info`, `fetch_movie_info`, `fetch_poster_url`, `fetch_season_episodes`, `fetch_show_seasons` were guarding on `g_api_key.empty()` alone, causing all shows to report FAILED when only a Bearer token was configured; fixed to `g_api_key.empty() && g_bearer_token.empty()` throughout
- **Season/episode picker popup** — clicking "Last watched" on a show card opens a modal with colour-coded seasons (green/orange/red) and a per-season episode grid with checkboxes; checking an episode marks it watched and updates the card live
- **`fi_` config file naming convention** — all FlickImp config files use `fi_` prefix; config is `fi_config.json`; env file eliminated
- **TMDB search-on-add** — adding a show or movie by typing a name searches TMDB and shows up to 5 results (title, year, poster); selecting a result auto-populates TMDB ID, thumbnail, and metadata
- **TMDB search-on-add IMDB ID back-fill** — `POST /api/shows` calls `/tv/{id}/external_ids` to populate `imdb_id` when a show is added via search; `POST /api/movies` does the same via `/movie/{id}/external_ids` and also stores `tmdb_id` + always fetches release date on add
- **Check All web UI** — ☰ menu in the header with "Check All"; opens a live-scrolling log modal streaming from `GET /api/check/stream` (SSE); checks all active shows for new episodes and back-fills missing movie release dates; status badge shows Running → Done / N NEW; cards refresh on completion
- **flickimp-config identity header** — logo (64px, from Qt resource) + bold app name + version string shown at the top of the config window; version drawn from `APP_NAME` / `APP_VERSION` in `version.hpp`
- **Package filename format + arch** — `CPACK_RPM_FILE_NAME RPM-DEFAULT` / `CPACK_DEBIAN_FILE_NAME DEB-DEFAULT` gives native `name-version-release.arch.rpm` and `name_version_arch.deb` format; `x86_64` arch included automatically; VERSION_SUFFIX flows through correctly
- **Movie release date label — tense-aware** — future date → "Releases: DATE"; past → "Released: DATE"; no date → "No release date yet." (italic/muted via `.release-date-unknown`)
- **Season select-all checkbox** — moved from per-season button to top of episode list pane; shows checked/indeterminate/unchecked state; syncs with individual episode toggles
- **Last-watched position tracks last-clicked episode** — CHECK always sets position to the checked episode; UNCHECK only recalculates when unchecking the current position; `next_season`/`next_episode` fields store the first unwatched episode after position
- **New shows default to Not started (S000-E000)** — "Last watched: Not started"; "Next:" hidden until first episode checked
- **Add Show / Add Movie as modal overlays** — forms now appear as centred modal overlays instead of inline below the section header
- **NEW badge + card sort** — shows with unwatched episodes (per `--check`) float to top with a green NEW badge and highlighted "Next:" line; backed by `latest_season`/`latest_episode` DB fields
- **Thumbnail IMDB links** — show/movie poster thumbnails link to their IMDB title page in a new tab
- **Episode browser view** — clicking a show title opens a full-page episode list: seasons pane (left), episode cards with Watched checkbox and Cast button (right); "Last watched" click still opens the quick picker modal
- **Cast modal** — show, movie, and episode Cast buttons open a scrollable cast list; actor names link to their IMDB person page; backed by a `people` table caching TMDB→IMDB person ID mappings cross-show
- **`--check` credential guard bug** — fixed; Bearer-only configs no longer report all shows FAILED
- **Config JSON resilience** — trailing commas stripped before parsing; parse errors now logged to stderr
- **Config file permissions 640→644** — prevents silent credential wipe when Qt app runs without read access
- **Package filename format** — RPM-DEFAULT / DEB-DEFAULT; proper arch in filename; VERSION_SUFFIX flows through
- **Dual watermarks** — Nutball-Labs logo on main page (`#main-view::before`, `z-index: -1`); FlickImp icon in episode browser (`.ev-episodes-pane` background); never overlap; scoped to their view
- **Semi-transparent show cards** — cards use `rgba(34,34,34,0.82)`; section background transparent; logo watermark visible in gaps and subtly through cards
- **Movie title IMDB links** — movie card titles are clickable links to the IMDB title page; `.movie-title-link` CSS class; white text, red on hover; falls back to plain text if no `imdb_id`
- **About modal** — ☰ → About; FlickImp icon (128px) + name + version + copyright + license + repo link + Nutball-Labs logo footer; backed by `GET /api/about` which reads version from `version.hpp` macros
- **TMDB null `imdb_id` robustness** — `fetch_person_imdb_id`, `fetch_movie_imdb_id`, `fetch_imdb_id`, and `parse_cast` now guard `is_null()` before string extraction; TMDB returns explicit null (not absent) for actors/movies with no IMDB entry, which previously threw `type_error.302` and broke the entire cast request
- **Movie cast `tmdb_id` back-fill on demand** — movies stored with `tmdb_id=0` have it resolved via `fetch_movie_info` on first Cast click and saved back to DB
- **Episode TMDB / IMDB popup** — clicking an episode title in the episode browser opens a floating popup with branded TMDB (blue) and IMDB (yellow) buttons; per-episode IMDB IDs fetched from TMDB and cached in new `episode_imdb_ids` DB table; disabled gracefully when an ID is unavailable
- **Movie title TMDB / IMDB popup** — movie card titles open the same TMDB/IMDB popup; `movie_to_json` now includes `tmdb_id` (was missing, causing TMDB links to point to current page); lazy backfill in `GET /api/movies` resolves TMDB ID from IMDB ID for older records
- **Next episode title on show cards** — "Next:" line shows `S3 − E5 — Episode Title`; new `next_episode_title` column on `shows` table; `fetch_episode_title()` scraper function; lazy backfill in `GET /api/shows`
- **Leading zeros removed from S/E display** — show cards display `S3 − E5` not `S003 − E005`
- **Responsive multi-column card grid** — 3 columns at ≥ 1200 px viewport (main widens to 1400 px); 4 columns at ≥ 1600 px (main widens to 1850 px)
- **FlickImp title as back navigation** — ← Back button removed from episode browser; "FlickImp" header title dims and becomes clickable in episode view, returning to main page on click
- **Section header renames** — "Now FlickImp" → "Shows"; "Movies Watchlist" → "Movies"
- **Pause/Resume removed** — status toggle button removed from show cards; `paused` value remains in `ShowStatus` enum and DB for backward compat but is no longer settable from the UI
- **Screenshots** — `screenshots/` directory with 8 annotated screenshots and `Screenshots.md` documentation

---

## Known Issues / Pending Work

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
- **Phase 2 (Episode data):** Complete — TMDB REST API is the sole data source; IMDB IDs used as lookup keys into TMDB, not scraped directly
- **Phase 3 (Streaming availability):** Out of scope — not planned
- **Phase 4 (systemd + Qt configurator):** Complete — `flickimp-config` Qt app; `scripts/install-service.sh`
- **Phase 5 (Season/episode picker):** Complete — modal popup with colour-coded season list and per-episode watched checkboxes
- All work on `main` branch

<!-- SN: 00002 -->
