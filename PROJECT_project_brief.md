# FlickImp — Project Brief
## For use at the start of new AI sessions to restore context quickly

---

## What It Is

[TWO TO THREE SENTENCE DESCRIPTION of the project — what it does, target OS,
primary library/framework, and the core user interaction model.]

Target OS: Alma Linux 9.x (RHEL 9 based).

---

## Developer Background

- Experienced Perl, Python3, bash scripting; Linux sysadmin for government entity.
- C/C++ is new territory — using AI to write C++ from workflow prompts.
- Full root on Alma Linux 9.x dev machine. VSCode + vim. Private GitHub repo.
- 40+ years sysadmin experience. Do not over-explain Linux basics.
- Same development paradigm and conventions as the PathMux project.

---

## Source File Map

### Library (`lib/`) — compiled into `libwatchinglib.a`

| File | Role |
|---|---|
| `lib/platform.cpp/.hpp` | OS abstraction: config dir path, home path |
| `lib/version.hpp` | Macros: `VERSION_MAJOR`, `VERSION_MINOR`, `VERSION_PATCH`, `VERSION_SUFFIX` |
| `lib/flickimp.hpp` | Umbrella header — include this in consumers of the lib |

### CLI (`cli/`) — compiled into `flickimp` binary

| File | Role |
|---|---|
| `cli/main.cpp` | Argument parsing; core workflow; exits |

### GUI (`gui/`) — compiled into `flickimp-gui` binary (Qt6)

| File | Role |
|---|---|
| `gui/main.cpp` | Qt application entry point; creates and shows MainWindow |
| `gui/MainWindow.cpp/.h` | Main window |
| `gui/AboutDialog.cpp/.h` | About box with Nutball-Labs branding |

---

## Key Design Decisions

*(Document as architecture decisions are made.)*

---

## SN Convention

Every source file carries a serial number at the bottom:
- `// SN: 00001` — C++ files and headers
- `# SN: 00001` — cmake files and shell scripts
- `<!-- SN: 00001 -->` — Markdown files

HWM is currently `00001`. Bump on change; increment HWM on each release.
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

| Dep | Install | Required For |
|---|---|---|
| g++ (C++17) | `gcc-c++` (Alma base) | All |
| CMake 3.16+ | `cmake` (Alma base) | Build |

---

## GitHub

Repo: `git@github.com:Nutball-Labs/FlickImp.git`
All work on `main` branch.
Commit format: `"Fix/Add/Update description — FlickImp vX.Y.Z (SN NNNNN)"`

---

## Current Status

Version 0.1.0 — scaffolding only; no source files written yet.
Phase 1 (CLI) and Phase 2 (Qt6 GUI) are both in the planned state.

<!-- SN: 00001 -->
