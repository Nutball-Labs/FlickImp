# FlickImp — Project Brief
## For use at the start of new AI sessions to restore context quickly

---

## What It Is

FlickImp is a self-hosted watchlist tracker for TV shows and movies. It runs as a lightweight HTTP daemon (`flickimp`) on a Linux server or home machine, serves a browser-based dark-theme UI, and pulls episode/season data from TMDB so you always know where you are in a series. Configuration is managed through a Qt6 desktop app (`flickimp-config`).

Target OS: Alma Linux 9.x (RHEL 9 based). System-service deployment with a dedicated `flickimp` user account.

---

## Developer Background

- Experienced Perl, Python3, bash scripting; Linux sysadmin for government entity.
- C/C++ is new territory — using AI to write C++ from workflow prompts.
- Full root on Alma Linux 9.x dev machine. VSCode + vim. Private GitHub repo.
- 40+ years sysadmin experience. Do not over-explain Linux basics.

---

## Source File Map

### Library (`lib/`) — two static libs

**`flickimpcorelib`** (platform + config only; linked by daemon and Qt app):

| File | Role |
|---|---|
| `lib/platform.cpp/.hpp` | OS paths: `config_dir()` → `/etc/flickimp/` (system) or XDG (dev); `data_dir()`; `db_path()` |
| `lib/config.cpp/.hpp` | JSON config (`fi_config.json`): port, web_root, tmdb_api_key, tmdb_bearer_token |

**`flickimplib`** (full library; links flickimpcorelib + SQLite + curl):

| File | Role |
|---|---|
| `lib/version.hpp` | Version macros; `APP_NAME`, `APP_VERSION`, license notice |
| `lib/flickimp.hpp` | Umbrella header |
| `lib/models.hpp` | POD structs: `Show`, `Movie`; `ShowStatus`, `MovieStatus` enums; both carry `tmdb_id` |
| `lib/database.cpp/.hpp` | SQLite CRUD for shows, movies, episode watches; `get_watched_counts()` for season colour coding |
| `lib/scraper.cpp/.hpp` | TMDB REST API (primary): `fetch_show_info()`, `fetch_show_seasons()`, `fetch_season_episodes()`, `fetch_movie_info()`, `fetch_movie_info_by_tmdb_id()`; IMDB ID reverse-lookup: `fetch_imdb_id()` (TV), `fetch_movie_imdb_id()` (movie); search: `search_shows()`, `search_movies()`; IMDB scraping via `--check` (secondary) |

### Service (`service/`) — compiled into `flickimp` binary (HTTP daemon)

| File | Role |
|---|---|
| `service/main.cpp` | Loads `fi_config.json`, arg parsing (`--port`, `--web`, `--check`), starts server |
| `service/server.cpp` | `httplib` HTTP server; REST API routes; JSON serialisation |
| `service/flickimp.service` | systemd unit — runs as `flickimp` user; reads config on startup; no env file |

### Qt configurator (`gui/`) — compiled into `flickimp-config` binary

| File | Role |
|---|---|
| `gui/main.cpp` | Qt application entry point |
| `gui/MainWindow.h/.cpp` | Service control (start/stop/restart/boot); port; TMDB API Key + Bearer Token |

### Web frontend (`web/`) — static assets served by the daemon

| File | Role |
|---|---|
| `web/index.html` | Single-page app shell; includes season/episode picker modal markup |
| `web/style.css` | Dark theme; modal overlay, season colour-coding, episode grid |
| `web/app.js` | Vanilla JS — show/movie cards; "Last watched / Next watch" display; popup modal with season picker and episode checkboxes; ☰ menu; Check All log modal (SSE) |

### Third-party (vendored, not committed)

| Dep | File(s) | Fetched by |
|---|---|---|
| SQLite amalgamation | `third_party/sqlite3/sqlite3.{c,h}` | `scripts/get-deps.sh` |
| cpp-httplib | `third_party/httplib.h` | `scripts/get-deps.sh` |
| nlohmann/json | `third_party/json.hpp` | `scripts/get-deps.sh` |

---

## Key Design Decisions

- **Config format: JSON** — `fi_config.json`; human-editable; no INI/TOML/YAML
- **Config location: `/etc/flickimp/fi_config.json`** — system service; Qt app writes via pkexec; dev user falls back to XDG path
- **Single config file** — port, web_root, and TMDB credentials all in `fi_config.json`; no separate env file
- **TMDB primary, IMDB secondary** — TMDB REST API for season/episode metadata; IMDB scraping only for `--check` new-episode detection
- **`fi_` file naming convention** — all FlickImp config files and CLI artifacts prefixed `fi_` (e.g. `fi_config.json`)

---

## API Surface

| Method | Path | Description |
|---|---|---|
| GET | `/api/shows` | List all shows |
| POST | `/api/shows` | Add a show (fetches TMDB poster on add) |
| PUT | `/api/shows/:id` | Update show fields |
| DELETE | `/api/shows/:id` | Remove a show |
| GET | `/api/shows/:id/seasons` | Season list with episode count and watched count per season |
| GET | `/api/shows/:id/episodes?season=N` | Episode list for a season (from TMDB, merged with local watched state) |
| PUT | `/api/shows/:id/episodes/:s/:e/watched` | Mark episode watched/unwatched; auto-advances last-watched position |
| GET | `/api/movies` | List all movies |
| POST | `/api/movies` | Add a movie (stores tmdb_id; back-fills imdb_id + release_date from TMDB) |
| PUT | `/api/movies/:id` | Update movie |
| DELETE | `/api/movies/:id` | Remove a movie |
| GET | `/api/search/shows?q=` | Search TMDB for TV shows; returns up to 5 results |
| GET | `/api/search/movies?q=` | Search TMDB for movies; returns up to 5 results |
| GET | `/api/check/stream` | SSE stream: check all active shows for new episodes + back-fill missing movie release dates; fires `event: done` on completion |

---

## SN Convention

Every source file carries a serial number at the bottom:
- `// SN: 00002` — C++ files and headers
- `# SN: 00002` — cmake files and shell scripts
- `<!-- SN: 00002 -->` — Markdown files

HWM is currently `00002`. Bump on change; increment HWM on each release.
Run: `cmake --build build-linux --target sn-audit` to audit.

---

## Build and Release Workflow

```
scripts/build-linux.sh           — configure + build (or --clean)
scripts/package-linux.sh         — CPack: RPM, DEB, TGZ → packages/
scripts/release.sh --linux       — gh CLI: upload packages to GitHub release
```

Build dir: `build-linux/`
Packages land in: `packages/`

---

## Hard Dependencies

| Dep | Install | Notes |
|---|---|---|
| g++ (C++17) | `gcc-c++` (Alma base) | GCC 11+ recommended |
| CMake 3.16+ | `cmake` (Alma base) | Build system |
| libcurl | `libcurl-devel` | TMDB + IMDB HTTP |
| Qt6 | `qt6-qtbase-devel` | flickimp-config GUI |

Run before first build: `sudo dnf install libcurl-devel qt6-qtbase-devel && ./scripts/get-deps.sh`

---

## GitHub

Repo: `git@github.com:Nutball-Labs/FlickImp.git`
All work on `main` branch.
Commit format: `"Fix/Add/Update description — FlickImp vX.Y.Z (SN NNNNN)"`

---

## Current Status

Version 0.2.1a — building. HTTP daemon, web UI, TMDB integration, and Qt configurator are all functional.

<!-- SN: 00002 -->
