// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include "models.hpp"
#include <sqlite3.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <stdexcept>

namespace FlickImp {

class DbError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    // Shows
    std::vector<Show> all_shows();
    Show              get_show(int id);
    int               add_show(const Show& s);     // returns new id
    void              update_show(const Show& s);
    void              delete_show(int id);

    // Episode watches
    std::set<int>      get_watched_episodes(int show_id, int season);
    std::map<int,int>  get_watched_counts(int show_id);   // season → watched episode count
    void               set_episode_watched(int show_id, int season, int episode, bool watched);
    std::pair<int,int> max_watched_position(int show_id);  // (season, episode) or (0,0) if none
    std::pair<int,int> compute_next_unwatched(int show_id, int season, int episode, int season_eps);

    // People cache (TMDB person ID → IMDB person ID)
    void        upsert_person(int tmdb_id, const std::string& name,
                              const std::string& imdb_id, const std::string& profile_url);
    bool        person_cached(int tmdb_person_id);
    std::string get_person_imdb_id(int tmdb_person_id);

    // Cast (show + movie)
    bool show_cast_cached(int show_id);
    void store_show_cast(int show_id, const std::vector<CastMember>& cast);
    std::vector<CastMember> get_show_cast(int show_id);

    bool movie_cast_cached(int movie_id);
    void store_movie_cast(int movie_id, const std::vector<CastMember>& cast);
    std::vector<CastMember> get_movie_cast(int movie_id);

    bool episode_cast_cached(int show_id, int season, int episode);
    void store_episode_cast(int show_id, int season, int episode, const std::vector<CastMember>& cast);
    std::vector<CastMember> get_episode_cast(int show_id, int season, int episode);

    // Episode IMDB ID cache (keyed by TMDB show ID + season + episode)
    bool        episode_imdb_cached(int tmdb_show_id, int season, int episode);
    void        cache_episode_imdb(int tmdb_show_id, int season, int episode, const std::string& imdb_id);
    std::string get_episode_imdb_id(int tmdb_show_id, int season, int episode);

    // Movies
    std::vector<Movie> all_movies();
    Movie              get_movie(int id);
    int                add_movie(const Movie& m);
    void               update_movie(const Movie& m);
    void               delete_movie(int id);

private:
    sqlite3* db_{nullptr};

    void exec(const char* sql);
    void create_schema();

    static Show  row_to_show(sqlite3_stmt* stmt);
    static Movie row_to_movie(sqlite3_stmt* stmt);
};

} // namespace FlickImp

// SN: 00004
