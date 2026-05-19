// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include <string>

namespace FlickImp {

enum class ShowStatus  { Watching = 0, Paused = 1, Finished = 2 };
enum class MovieStatus { WantToWatch = 0, Watched = 1 };

struct Show {
    int        id{0};
    std::string title;
    std::string service;          // "Netflix", "HBO Max", etc.
    int        season{1};
    int        episode{1};
    std::string imdb_id;          // e.g. "tt0903747" (optional)
    int        tmdb_id{0};        // TMDB show ID, cached after first lookup
    int        total_episodes{0};
    ShowStatus  status{ShowStatus::Watching};
    std::string notes;
    std::string thumbnail_url;
};

struct Movie {
    int        id{0};
    std::string title;
    std::string imdb_id;
    int        tmdb_id{0};        // TMDB movie ID, cached after first lookup
    std::string release_date;
    std::string thumbnail_url;
    MovieStatus status{MovieStatus::WantToWatch};
    std::string notes;
};

} // namespace FlickImp

// SN: 00001
