// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "config.hpp"
#include "platform.hpp"
#include <filesystem>
#include <fstream>
#include <json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace FlickImp {

static std::string config_file_path() {
    return (fs::path(Platform::config_dir()) / "fi_config.json").string();
}

static Config parse_config(std::istream& f) {
    Config cfg;
    try {
        auto j = json::parse(f);
        if (j.contains("port")               && j["port"].is_number_integer())
            cfg.port               = j["port"].get<int>();
        if (j.contains("fi_web_root")         && j["fi_web_root"].is_string())
            cfg.fi_web_root        = j["fi_web_root"].get<std::string>();
        if (j.contains("fi_db_path")         && j["fi_db_path"].is_string())
            cfg.fi_db_path         = j["fi_db_path"].get<std::string>();
        if (j.contains("tmdb_api_key")       && j["tmdb_api_key"].is_string())
            cfg.tmdb_api_key       = j["tmdb_api_key"].get<std::string>();
        if (j.contains("tmdb_bearer_token")  && j["tmdb_bearer_token"].is_string())
            cfg.tmdb_bearer_token  = j["tmdb_bearer_token"].get<std::string>();
    } catch (...) {}
    return cfg;
}

Config load_config() {
    std::ifstream f(config_file_path());
    if (!f) return {};
    return parse_config(f);
}

void save_config(const Config& cfg) {
    save_config_to(cfg, config_file_path());
}

Config load_config_from(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    return parse_config(f);
}

void save_config_to(const Config& cfg, const std::string& path) {
    json j;
    j["port"]               = cfg.port;
    j["fi_web_root"]        = cfg.fi_web_root;
    j["fi_db_path"]         = cfg.fi_db_path;
    j["tmdb_api_key"]       = cfg.tmdb_api_key;
    j["tmdb_bearer_token"]  = cfg.tmdb_bearer_token;
    std::ofstream f(path);
    if (f) f << j.dump(2) << '\n';
}

} // namespace FlickImp

// SN: 00003
