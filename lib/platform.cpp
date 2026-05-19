// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "platform.hpp"
#include <cstdlib>
#include <filesystem>
#if !defined(_WIN32) && !defined(__APPLE__)
#  include <unistd.h>
#endif

namespace FlickImp::Platform {

namespace fs = std::filesystem;

// -------------------------------------------------------------------------
// Windows: %APPDATA%\flickimp  (e.g. C:\Users\you\AppData\Roaming\flickimp)
// Config and data share one directory — Windows has no XDG equivalent.
// -------------------------------------------------------------------------
#if defined(_WIN32)

static fs::path base_dir() {
    const char* v = std::getenv("APPDATA");
    return fs::path(v ? v : ".") / "flickimp";
}

std::string config_dir() {
    fs::path d = base_dir();
    fs::create_directories(d);
    return d.string();
}

std::string data_dir() { return config_dir(); }

std::string db_path() {
    return (fs::path(data_dir()) / "flickimp.db").string();
}

// -------------------------------------------------------------------------
// macOS: ~/Library/Application Support/flickimp
// Config and data share one directory — XDG does not apply on macOS.
// -------------------------------------------------------------------------
#elif defined(__APPLE__)

static fs::path base_dir() {
    const char* h = std::getenv("HOME");
    return fs::path(h ? h : ".") / "Library" / "Application Support" / "flickimp";
}

std::string config_dir() {
    fs::path d = base_dir();
    fs::create_directories(d);
    return d.string();
}

std::string data_dir() { return config_dir(); }

std::string db_path() {
    return (fs::path(data_dir()) / "flickimp.db").string();
}

// -------------------------------------------------------------------------
// Linux — two modes depending on who is running the binary:
//
// System service user (UID < 1000, i.e. the dedicated "flickimp" account):
//   config  → /etc/flickimp          (read-only at runtime; Qt app writes via pkexec)
//   data    → /var/lib/flickimp      (read-write; DB lives here)
//
// Regular user (UID >= 1000, i.e. developer running from the build dir):
//   config  → XDG_CONFIG_HOME/flickimp   (~/.config/flickimp)
//   data    → XDG_DATA_HOME/flickimp     (~/.local/share/flickimp)
// -------------------------------------------------------------------------
#else

static bool is_system_user() { return ::getuid() < 1000; }

static std::string xdg_or(const char* env, const std::string& fallback) {
    const char* v = std::getenv(env);
    return (v && *v) ? std::string(v) : fallback;
}

static std::string home_dir() {
    const char* h = std::getenv("HOME");
    return h ? h : ".";
}

std::string config_dir() {
    if (is_system_user()) {
        fs::create_directories("/etc/flickimp");
        return "/etc/flickimp";
    }
    auto d = xdg_or("XDG_CONFIG_HOME", home_dir() + "/.config") + "/flickimp";
    fs::create_directories(d);
    return d;
}

std::string data_dir() {
    if (is_system_user()) {
        fs::create_directories("/var/lib/flickimp");
        return "/var/lib/flickimp";
    }
    auto d = xdg_or("XDG_DATA_HOME", home_dir() + "/.local/share") + "/flickimp";
    fs::create_directories(d);
    return d;
}

std::string db_path() {
    std::string db_dir = data_dir() + "/db";
    fs::create_directories(db_dir);
    return db_dir + "/flickimp.db";
}

#endif

} // namespace FlickImp::Platform

// SN: 00003
