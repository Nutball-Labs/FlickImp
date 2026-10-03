# FlickImp — Changelog

All notable user-visible changes to this project are documented here.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

---

## [1.7.0] — 2026-10-03

First release since 1.5.0 (1.6.x were local test builds only). The release
calendar and Docker support started as pull requests #2 and #1 from
[@tomcannan](https://github.com/tomcannan), reworked during review. Thanks!

### Added
- **Release calendar** — a Calendar tab showing a month of air dates for your current shows and release dates for your movies; Prev / Next / Today navigation; show entries open the episode browser; on a phone the grid collapses to a list of days with releases
- **Docker support** — multi-stage `Dockerfile` (AlmaLinux 9) and `docker-compose.yml` with config and database in `./data/`; see the new Docker section in the README
- **Settings dialog** — ☰ → Settings: TMDB token and API key (shown masked, with a Test connection button), web port, and per-browser display options (zoom, which tab opens at startup, which queue to start in). Values saved here override `fi_config.json`, so they work even when the config file is read-only (Linux service); `--port` on the command line still wins
- **Backup / Restore** — ☰ → Backup / Restore downloads everything as a compressed `.json.gz`, with or without the TMDB cache (both sizes shown). Restore can **Merge** (add what's missing) or **Replace** (load the backup exactly); before a restore the current database is copied to `flickimp.db.pre-restore`
- **Show groups** — link several shows so they appear as one (Doctor Who 1963 / 2005 / 2023, the Jeopardy family). Link from a show's Edit panel; the group card shows Last / Next across all members by air date; rename, reorder or ungroup from the group's Edit panel
- **Group episode browser** — browse a group **By season** (every member's seasons in premiere order) or **By year** (every member's episodes in air-date order); the choice is remembered per group
- **Mark all aired episodes as watched** — checkbox when adding a show, and in a show's Edit panel; one TMDB request, so long runs like Jeopardy don't need 40-odd seasons clicked through
- **Whole-season watched / unwatched** — right-click a season (long-press on iPad / iPhone) in the episode browser or the Last watched picker; plus a **Watched All** checkbox above each season's episode list. Only aired episodes are marked, and your position only ever moves forward
- **Queue PINs** — a PIN-protected queue now asks for its PIN before showing its shows and movies; queue tabs show 🔒 / 🔓 and the PIN button in Manage Queues is red (PIN set) or green (none). A reload or daemon restart locks it again. Speed bump for parental controls, not real security
- **`--clear-pin QUEUE`** — reset a forgotten queue PIN from the server: `sudo -u flickimp flickimp --clear-pin Kids`
- **Build version prompt** — `scripts/build-linux.sh` shows the version and asks you to confirm it or enter a new one before building (`-y` skips)
- **Debian package scripts** — the `.deb` now creates the service account and directories like the RPM does, and `apt purge` asks before deleting the configuration and database (default: keep)

### Changed
- **Episode browser layout** — the season list and the episode list scroll independently, so picking a season far down a long list shows its episodes right beside it
- **Upgrades restart the service** — RPM and DEB upgrades restart FlickImp if it's running, so the new version takes effect straight away
- **Backups and PINs** — PIN-protected queues are left out of a backup unless you enter their PIN; a Replace restore requires every PIN-protected queue to be unlocked first
- **Queue PINs are stored hashed** — existing PINs are converted automatically; the API no longer sends PINs to the browser
- **Database** — new tables `settings`, `show_groups`, `tmdb_episodes`, `tmdb_seasons` and columns `shows.group_id` / `group_order`, all added automatically on first start

### Fixed
- **Seasons with unannounced episodes failed to load** — TMDB sends `null` for a missing episode title or air date; these are now treated as blank instead of failing the whole season
- **Changing TMDB credentials while requests are running** — credentials are now swapped safely, so saving them in Settings can't interfere with requests already in flight

---

## [1.5.0] — 2026-09-30

### Added
- **Windows package** — `flickimp-X.Y.Z-win64.zip`; `install.cmd` installs per-user to `%LOCALAPPDATA%\Programs\FlickImp` with no admin rights, prompts for TMDB token and port on first install, registers a hidden Task Scheduler logon task, and adds a Start Menu shortcut; `uninstall.cmd` removes it and keeps data unless `-Purge` is passed
- **macOS package** — `flickimp-X.Y.Z-macOS.{zip,tar.gz}`, universal (Apple Silicon + Intel), macOS 11+; `install.sh` installs per-user to `~/Library/Application Support/flickimp/app` with no sudo, clears the Gatekeeper quarantine flag, and registers a launchd LaunchAgent; `uninstall.sh [--purge]`
- **`--log FILE`** — appends daemon output to a file, unbuffered; used by the Windows and macOS autostart
- **Windows and macOS build scripts** — `build-/package-/Go-` scripts for both platforms; Windows gets libcurl from vcpkg manifest mode (`vcpkg.json`, static triplet), and the build scripts fetch `third_party/` automatically if it's missing

### Changed
- **Web root discovery** — the daemon locates `web/` via the real executable path (`/proc/self/exe`, `_NSGetExecutablePath`, `GetModuleFileName`) instead of `argv[0]`, which was unreliable under PATH lookup, launchd and Task Scheduler
- **CMake** — MSVC static CRT, `/utf-8 /bigobj`; macOS deployment target 11.0; install layout and CPack generators chosen per platform (RPM/DEB/TGZ, ZIP, TGZ+ZIP)
- **`.gitattributes`** — enforces LF for `.sh` and CRLF for `.ps1`/`.cmd`, so checkouts work on every platform

---

## [1.1.0 – 1.4.3] — 2026-05 to 2026-09

Shipped as Linux packages only. The source was committed together at 1.4.3.

### Added
- **Named queues** — `queues` table, `/api/queues` CRUD, a ☰ → Manage Queues modal, and queue tabs across the top of the main page
- **Shows / Movies tabs** — separate main tabs; shows also have **Current / Queued** sub-tabs
- **Drag-to-reorder** — cards can be dragged into a custom order; persisted through `sort_order` (`PUT /api/shows/reorder`, `PUT /api/movies/reorder`)
- **Zoom control** — − / % / + in the ☰ menu
- **PWA manifest + icons** — FlickImp can be added to a phone or tablet home screen

---

## [1.0.0] — 2026-05-24

### Added
- **Episode TMDB / IMDB popup** — clicking an episode title in the episode browser opens a floating popup with branded TMDB (blue) and IMDB (yellow) buttons; per-episode IMDB IDs are fetched from TMDB and cached in a new `episode_imdb_ids` DB table so the first season load is the only API hit
- **Movie title TMDB / IMDB popup** — clicking a movie card title opens the same popup; replaces the previous IMDB-only link
- **Next episode title on show cards** — "Next:" line now reads `Next: S3 − E5 — Episode Title`; title is fetched from TMDB when the watched position changes and cached in the `shows` table
- **Responsive multi-column card grid** — 3 columns at ≥ 1200 px viewport (content area widens to 1400 px); 4 columns at ≥ 1600 px (content area widens to 1850 px)
- **Episode browser view** — clicking a show title opens a full-page episode browser with a season list (left) and episode cards (right); each episode card shows episode number, title, air date, a Watched checkbox, and a Cast button
- **Cast modal** — show cards, movie cards, and episode cards each have a Cast button that opens a scrollable cast list with headshots, actor name (linked to their IMDB person page), and character name; backed by a `people` table that caches TMDB → IMDB person ID mappings cross-show
- **NEW badge and sort** — shows with unwatched aired episodes float to the top with a green NEW badge; the "Next:" line highlights in green; populated by `--check`
- **Check All web UI** — ☰ → Check All opens a live-scrolling log modal streaming from `GET /api/check/stream` (SSE); checks all active shows for new episodes and back-fills missing movie release dates; status badge shows Running → Done / N NEW
- **`GET /api/check/stream`** — SSE endpoint; fires `event: done` on completion so the browser can refresh cards
- **Thumbnail IMDB links** — show and movie poster thumbnails link to their IMDB title page
- **Add Show / Add Movie modals** — add forms are centred modal overlays with TMDB search-on-type returning up to 5 results with poster, title, and year
- **Dual watermarks** — Nutball-Labs logo on main page; FlickImp icon on episode browser; scoped so they never overlap
- **`flickimp-config` identity header** — Qt configurator shows FlickImp logo (64 px) and version string at top of window
- **About modal** — ☰ → About shows icon, version, copyright, license, repo link, and Nutball-Labs footer; version served live from `GET /api/about`
- **New API endpoints** — `GET /api/shows/:id/cast`, `GET /api/movies/:id/cast`, `GET /api/shows/:id/episodes/:s/:e/cast`, `GET /api/about`
- **Screenshots** — `screenshots/` directory with annotated `Screenshots.md`

### Changed
- **Season/episode numbers no longer zero-padded** — show cards display `S3 − E5` instead of `S003 − E005`
- **"Now FlickImp" → "Shows"** and **"Movies Watchlist" → "Movies"** — section headers simplified
- **"FlickImp" title as back navigation** — the header title dims and becomes clickable while in the episode browser; replaces the dedicated ← Back button
- **Pause/Resume removed** — the status-toggle button is gone from show cards; the paused status value is preserved in the DB but is no longer settable from the UI
- **Last-watched position semantics** — position tracks the last episode explicitly checked, not the highest ever checked; unchecking only resets position if it was the current position episode
- **New shows default to Not started** — S000-E000; "Last watched" shows "Not started"; "Next:" is hidden until the first episode is checked
- **Season select-all** — moved to top of episode list pane; shows checked / indeterminate / unchecked state in sync with individual toggles
- **Movie release date label** — tense-aware: "Releases:" (future), "Released:" (past), "No release date yet." (none)
- **"Next watch:" → "Next:"** — label shortened
- **Show card visual style** — semi-transparent cards (18% pass-through); transparent section backgrounds; watermark visible through gaps
- **Package filename format** — RPM: `name-version-release.arch.rpm`; DEB: `name_version_arch.deb`; VERSION_SUFFIX flows through correctly
- **Config file permissions** — `fi_config.json` written as 644 (was 640)
- **`movie_to_json` now includes `tmdb_id`** — field was missing from the API response; TMDB movie links now work

### Fixed
- **`--check` always failing with Bearer-only credentials** — scraper functions guarded `g_api_key.empty()` alone; fixed to `g_api_key.empty() && g_bearer_token.empty()`
- **Cast crash on actors with no IMDB entry** — TMDB returns explicit `null` (not absent) for `imdb_id`; all fetch functions now guard `is_null()` before string extraction
- **Config JSON trailing comma / parse errors** — trailing commas stripped before parsing; parse errors logged to stderr instead of silently swallowed
- **TMDB search-on-add missing IMDB ID** — `POST /api/shows` and `POST /api/movies` back-fill IMDB ID via `/external_ids` on add
- **`POST /api/movies` TMDB ID silently discarded** — now stored and used to back-fill release date on add
- **Movie `tmdb_id` missing from API response** — lazy backfill in `GET /api/movies` resolves TMDB ID for movies with only an IMDB ID; persisted to DB

---

## [0.2.1a] — 2026-05-18

### Added
- **Season/episode picker popup** — clicking "Last watched" on a show card opens a modal with a colour-coded season list (green = all watched, orange = partial, red = unwatched) and a per-season episode grid with checkboxes; checking an episode marks it watched and updates the card in place
- **`GET /api/shows/:id/seasons`** endpoint — returns season list with per-season episode and watched counts; used by the popup for colour coding
- **TMDB Bearer token support** — `fi_config.json` now stores both `tmdb_api_key` (v3) and `tmdb_bearer_token` (v4); Bearer token is preferred when present, v3 API key is the fallback
- **flickimp-config credentials panel** — the Qt configurator now exposes both TMDB credential fields (API Key and Bearer Token) with a single "Save Credentials" button

### Changed
- **TMDB is now the primary episode data source** — season lists and episode details are fetched from TMDB's REST API; IMDB scraping remains available via `--check` but is no longer the primary episode lookup path
- **Config file renamed `fi_config.json`** — all FlickImp config files follow the `fi_` naming convention; existing `config.json` files must be renamed on upgrade
- **Env file eliminated** — `flickimp.env` is gone; port and all settings live in `fi_config.json`; the systemd unit now runs `flickimp` with no arguments
- **Show card episode display** — the ◀/▶ episode increment buttons are replaced by "Last watched: SXXX − EXXX" (clickable) and "Next watch: SXXX − EXXX" (computed) lines
- **Episodes panel removed** — the inline episode panel below show cards is replaced by the new popup modal

---

## [0.1.0] — 2026-05-14 (initial)

### Added
- HTTP daemon (`flickimp`) with REST API for shows and movies
- Browser-based dark-theme single-page UI
- SQLite storage via vendored amalgamation
- IMDB scraping for episode counts (`--check` mode)
- TMDB poster/metadata lookup (API key stored in config)
- systemd service unit and install script
- Qt6 configurator (`flickimp-config`) for service control and port/API-key settings

<!-- SN: 00006 -->
