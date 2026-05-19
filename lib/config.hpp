// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include <string>

namespace FlickImp {

struct Config {
    int         port{8647};
    std::string fi_web_root;        // empty = auto-detect from binary location
    std::string fi_db_path;         // directory; empty = auto-detect via Platform::db_path()
    std::string tmdb_api_key;       // TMDB v3 API key (fallback auth)
    std::string tmdb_bearer_token;  // TMDB v4 Bearer token (preferred auth)
};

Config load_config();
void   save_config(const Config& cfg);

// Path-explicit variants — used by the Qt configurator to target the
// system config at /etc/flickimp/config.json from a regular user account.
Config load_config_from(const std::string& path);
void   save_config_to(const Config& cfg, const std::string& path);

} // namespace FlickImp

// SN: 00003
