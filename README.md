# FlickImp

A self-hosted web app for tracking the TV shows and movies you're watching — where you left off, what season and episode you're on, and how many episodes are left.

FlickImp is for people who watch across multiple streaming services and want a single private list they actually control. It runs as a lightweight HTTP daemon on your local machine or home server, serves a browser-based UI, and pulls episode data from IMDB so you always know how far you are through a series.

---

## Features

- **Show tracking** — track current season/episode, streaming service, and watch status (Watching / Paused / Finished)
- **Season/episode picker** — click "Last watched" on any show card to open a popup with colour-coded seasons (green = all watched, orange = partial, red = none) and a per-episode checklist
- **Movie list** — maintain a want-to-watch / watched list alongside your shows
- **TMDB integration** — search-on-add finds the right title from The Movie Database; fetches season lists, episode titles/air-dates, poster art, and release dates automatically; IMDB ID back-filled from TMDB so the episode picker always works
- **Check All** — ☰ menu → Check All opens a live-scrolling log showing new-episode status for every active show and back-fills any missing movie release dates
- **Browser UI** — clean dark-theme single-page app; no install required on the client side
- **REST API** — JSON API backing the UI; scriptable from curl or any HTTP client
- **SQLite storage** — single database file, no separate database server required
- **Qt6 configurator** (`flickimp-config`) — desktop app for service control and TMDB credential management

---

## Requirements

**Platform:** Linux (x86_64). Primary development and testing on Alma Linux 9.x / RHEL 9.
Other Linux distributions should work but are not regularly tested. macOS and Windows are not currently supported.

**Build dependencies:**

- g++ with C++17 support (GCC 11+ recommended)
- CMake 3.16+
- libcurl development headers

Install on Alma / RHEL:

```bash
sudo dnf install cmake gcc-c++ libcurl-devel
```

**Vendored dependencies** (fetched automatically by `get-deps.sh`):

- SQLite amalgamation
- [cpp-httplib](https://github.com/yhirose/cpp-httplib)
- [nlohmann/json](https://github.com/nlohmann/json)

---

## Building from Source

```bash
git clone git@github.com:Nutball-Labs/FlickImp.git
cd FlickImp
./scripts/get-deps.sh        # fetch vendored third-party headers
./scripts/build-linux.sh     # configure + build
```

The `flickimp` daemon binary lands in `build-linux/`.

---

## Usage

Start the daemon:

```bash
./build-linux/flickimp --port 8647 --web ./web
```

Then open `http://localhost:8647` in your browser.

Configuration is read from `/etc/flickimp/fi_config.json` when running as a system service (UID < 1000), or `~/.config/flickimp/fi_config.json` for local development. Use `flickimp-config` to set the port and TMDB credentials.

---

## Project Status

**v0.2.1a — Active development.** HTTP daemon, web UI, TMDB integration, season/episode picker popup, and Qt6 configurator are all functional.

---

## Development Paradigm

FlickImp is built the same way as its sister projects
[TagGoblin](https://github.com/Nutball-Labs/TagGoblin) and
[PathMux](https://github.com/Nutball-Labs/PathMux) — a collaboration between
an experienced Linux sysadmin and Claude (Anthropic's AI), which handles the
C++ implementation under continuous human guidance and review. The architecture,
feature decisions, and real-world testing are entirely human-driven.

---

## License

GNU General Public License v3 — see [LICENSE](LICENSE).
Copyright (C) 2026 Nutball Labs / Stephen Berg

<!-- SN: 00001 -->
