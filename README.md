# Watching

[ONE-LINE SUMMARY — what the tool does in plain terms.]

[SECOND PARAGRAPH — who it is for, what problem it solves, how it fits into
a typical workflow.]

---

## Features

- **[Feature]** — [description]
- **[Feature]** — [description]
- **[Feature]** — [description]

---

## Requirements

**Platform:** Linux (x86_64). Primary development and testing on Alma Linux 9.x / RHEL 9.

**Build dependencies:**

- g++ with C++17 support (GCC 11+ recommended)
- CMake 3.16+
- [ADDITIONAL DEPS]

Install on Alma / RHEL:

```bash
sudo dnf install cmake gcc-c++ [ADDITIONAL_PACKAGES]
```

**Runtime:**

- [RUNTIME DEPS, if any]

---

## Building from Source

```bash
git clone git@github.com:Nutball-Labs/Watching.git
cd Watching
./scripts/build-linux.sh
```

Binaries land in `build-linux/`.

---

## Usage

*(Fill in when implemented.)*

---

## Project Status

**v0.1.0 — Scaffolding phase.** Source files not yet written.

---

## Development Paradigm

Watching is built the same way as its sister projects
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
