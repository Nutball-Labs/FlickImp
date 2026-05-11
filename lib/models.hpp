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
    int        total_episodes{0}; // from IMDB; 0 = unknown
    ShowStatus  status{ShowStatus::Watching};
    std::string notes;
};

struct Movie {
    int        id{0};
    std::string title;
    std::string imdb_id;
    MovieStatus status{MovieStatus::WantToWatch};
    std::string notes;
};

} // namespace FlickImp

// SN: 00001
