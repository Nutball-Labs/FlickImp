// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include <optional>
#include <string>

namespace FlickImp::Scraper {

struct EpisodeInfo {
    int         season{0};
    int         episode{0};
    std::string title;
    std::string air_date;  // "YYYY-MM-DD", empty if unknown
};

struct ShowInfo {
    std::string title;
    int         total_seasons{0};
    int         total_episodes{0};
    EpisodeInfo latest_aired;  // season/episode == 0 if no aired ep found
};

// Fetch show metadata + latest aired episode from IMDB.
// Returns nullopt on network error, parse failure, or non-TV-series ID.
std::optional<ShowInfo> fetch_show_info(const std::string& imdb_id);

} // namespace FlickImp::Scraper

// SN: 00001
