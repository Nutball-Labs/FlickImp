// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "settings.hpp"

namespace FlickImp::Settings {

static void pick(const std::string& db_val, const std::string& file_val,
                 std::string& out, std::string& src) {
    if (!db_val.empty())        { out = db_val;   src = SRC_DB; }
    else if (!file_val.empty()) { out = file_val; src = SRC_FILE; }
    else                        { out.clear();    src = SRC_DEFAULT; }
}

Effective resolve(const Config& file_cfg, Database& db, int cli_port) {
    Effective e;
    pick(db.get_setting(KEY_BEARER),  file_cfg.tmdb_bearer_token, e.tmdb_bearer_token, e.bearer_src);
    pick(db.get_setting(KEY_API_KEY), file_cfg.tmdb_api_key,      e.tmdb_api_key,      e.api_key_src);

    int db_port = 0;
    try { db_port = std::stoi(db.get_setting(KEY_PORT)); } catch (...) {}

    if (cli_port > 0)                  { e.port = cli_port;      e.port_src = SRC_CLI; }
    else if (db_port > 0)              { e.port = db_port;       e.port_src = SRC_DB; }
    else if (file_cfg.port != 8647)    { e.port = file_cfg.port; e.port_src = SRC_FILE; }
    else                               { e.port = 8647;          e.port_src = SRC_DEFAULT; }
    return e;
}

} // namespace FlickImp::Settings

// SN: 00006
