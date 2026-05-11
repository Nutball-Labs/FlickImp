// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "platform.hpp"
#include <cstdlib>
#include <filesystem>

namespace FlickImp::Platform {

namespace fs = std::filesystem;

static std::string xdg_or(const char* env, const std::string& fallback) {
    const char* v = std::getenv(env);
    return (v && *v) ? std::string(v) : fallback;
}

static std::string home_dir() {
    const char* h = std::getenv("HOME");
    return h ? h : ".";
}

std::string config_dir() {
    auto d = xdg_or("XDG_CONFIG_HOME", home_dir() + "/.config") + "/flickimp";
    fs::create_directories(d);
    return d;
}

std::string data_dir() {
    auto d = xdg_or("XDG_DATA_HOME", home_dir() + "/.local/share") + "/flickimp";
    fs::create_directories(d);
    return d;
}

std::string db_path() {
    return data_dir() + "/flickimp.db";
}

} // namespace FlickImp::Platform

// SN: 00001
