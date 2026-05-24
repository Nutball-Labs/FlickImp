# FlickImp — Changelog

All notable user-visible changes to this project are documented here.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

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

<!-- SN: 00004 -->
