# FlickImp — Changelog

All notable user-visible changes to this project are documented here.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

---

## [Unreleased]

### Added
- **Check All web UI** — ☰ menu in the header contains a "Check All" item; clicking it opens a live-scrolling log modal that streams progress from the new `GET /api/check/stream` SSE endpoint; shows new-episode status for every active tracked show and back-fills missing movie release dates in one pass
- **`GET /api/check/stream`** — Server-Sent Events endpoint that runs the same logic as `flickimp --check` and additionally looks up release dates for any movie that doesn't have one; fires a `done` event on completion so the browser can refresh cards

### Fixed
- **TMDB search-on-add missing IMDB ID (shows)** — `POST /api/shows` now calls TMDB `/tv/{id}/external_ids` to back-fill `imdb_id` when a show is added via search (TMDB ID present, IMDB ID absent); previously the season/episode picker popup failed with "No IMDB ID set for this show"
- **`GET /api/shows/:id/episodes` guard** — endpoint no longer rejects shows that have a TMDB ID but no IMDB ID (aligns with the already-correct `/seasons` endpoint behaviour)
- **`POST /api/movies` TMDB ID ignored** — movie TMDB ID sent by the search UI was silently discarded; the endpoint now stores it, back-fills the IMDB ID from TMDB (`/movie/{id}/external_ids`), and always fetches the release date (previously skipped when a thumbnail URL was already provided)

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

<!-- SN: 00002 -->
