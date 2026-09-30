// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include <string>

namespace FlickImp {

enum class ShowStatus  { Watching = 0, Paused = 1, Finished = 2 };
enum class MovieStatus { WantToWatch = 0, Watched = 1 };
enum class ShowQueue   { Current = 0, Queued = 1 };

struct Queue {
    int         id{0};
    std::string name;
    int         sort_order{0};
    std::string pin;
};

struct Show {
    int        id{0};
    std::string title;
    std::string service;          // "Netflix", "HBO Max", etc.
    int        season{1};
    int        episode{1};
    std::string imdb_id;          // e.g. "tt0903747" (optional)
    int        tmdb_id{0};        // TMDB show ID, cached after first lookup
    int        total_episodes{0};
    int        latest_season{0};   // set by --check; 0 = unknown/caught up
    int        latest_episode{0};
    int        season_episodes{0}; // episode count for current season; 0 = unknown
    int        next_season{0};    // first unwatched episode after position; 0 = use fallback
    int        next_episode{0};
    std::string next_episode_title;
    ShowStatus  status{ShowStatus::Watching};
    ShowQueue   queue{ShowQueue::Current};
    int        sort_order{0};
    int        queue_id{1};
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
    int        sort_order{0};
    int        queue_id{1};
    std::string notes;
};

struct CastMember {
    int         tmdb_person_id{0};
    std::string name;
    std::string character;
    std::string profile_url;
    std::string imdb_id;       // populated from people cache; empty = unknown
    int         sort_order{0};
};

} // namespace FlickImp

// SN: 00004
