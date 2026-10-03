// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include "config.hpp"
#include "database.hpp"
#include <string>

// User-changeable settings. Values saved from the web Settings dialog live in
// the DB `settings` table (always writable, even when fi_config.json is the
// read-only /etc/flickimp copy). Precedence, highest first:
//   command line  >  DB settings table  >  fi_config.json  >  built-in default
namespace FlickImp::Settings {

inline constexpr const char* KEY_BEARER  = "tmdb_bearer_token";
inline constexpr const char* KEY_API_KEY = "tmdb_api_key";
inline constexpr const char* KEY_PORT    = "port";

// Where an effective value came from — shown in the Settings dialog
inline constexpr const char* SRC_CLI     = "command line";
inline constexpr const char* SRC_DB      = "settings";
inline constexpr const char* SRC_FILE    = "config file";
inline constexpr const char* SRC_DEFAULT = "default";

struct Effective {
    std::string tmdb_bearer_token;
    std::string tmdb_api_key;
    int         port{8647};
    std::string bearer_src;
    std::string api_key_src;
    std::string port_src;
};

// Resolve DB + file values. cli_port > 0 means --port was given.
Effective resolve(const Config& file_cfg, Database& db, int cli_port = 0);

} // namespace FlickImp::Settings

// SN: 00006
