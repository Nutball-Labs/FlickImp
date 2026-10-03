// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include "models.hpp"
#include <json.hpp>
#include <sqlite3.h>
#include <map>
#include <set>
#include <string>
#include <tuple>
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
    // Mark episodes 1..last of each (season, last) watched in one transaction.
    // Returns the number of rows newly added.
    void               clear_season_watched(int show_id, int season);
    int                mark_watched_through(int show_id,
                                            const std::vector<std::pair<int,int>>& season_last);
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

    // Queues
    std::vector<Queue> all_queues();
    Queue              get_queue(int id);
    int                add_queue(const Queue& q);
    void               update_queue(const Queue& q);
    void               delete_queue(int id);
    int                queue_count();
    void               hash_plain_pins();   // upgrade any plain-text queue PINs to hashes

    // Sort order
    int  max_show_sort_order(ShowQueue queue, int queue_id);
    int  max_movie_sort_order(int queue_id);
    void reorder_shows(const std::vector<int>& ids);
    void reorder_movies(const std::vector<int>& ids);

    // Movies
    std::vector<Movie> all_movies();
    Movie              get_movie(int id);
    int                add_movie(const Movie& m);
    void               update_movie(const Movie& m);
    void               delete_movie(int id);

    // TMDB season episode-list cache: (episode, title, air_date) per season
    bool season_list_fresh(int tmdb_show_id, int season, const std::string& today);
    void store_season_list(int tmdb_show_id, int season,
                           const std::vector<std::tuple<int, std::string, std::string>>& eps,
                           const std::string& today, bool final);
    std::vector<std::tuple<int, std::string, std::string>> season_list(int tmdb_show_id, int season);

    // Show groups
    std::vector<ShowGroup> all_groups();
    bool group_exists(int id);
    int  create_group(const std::string& name);              // returns new id
    void rename_group(int id, const std::string& name);
    void set_show_group(int show_id, int group_id, int group_order);  // group_id 0 = ungroup
    int  max_group_order(int group_id);
    void delete_group(int id);                               // ungroups all members
    void prune_groups();                                     // dissolve groups with < 2 members

    // Settings (key/value; empty value deletes the key)
    std::string get_setting(const std::string& key);
    void        set_setting(const std::string& key, const std::string& value);

    // Generic table access for backup / restore. `table` must come from a
    // fixed list, never from user input.
    std::vector<std::string> table_columns(const std::string& table);
    nlohmann::json           dump_table(const std::string& table);
    // verb: "INSERT", "INSERT OR IGNORE" or "INSERT OR REPLACE".
    // Returns the new rowid, or 0 if the row was ignored.
    long long                insert_row(const std::string& table, const nlohmann::json& row,
                                        const char* verb = "INSERT");
    void                     clear_table(const std::string& table);
    void begin();
    void commit();
    void rollback();
    void snapshot_to(const std::string& path);   // consistent copy via VACUUM INTO
    std::string path() const;                    // main DB file path

private:
    sqlite3* db_{nullptr};

    void exec(const char* sql);
    void create_schema();

    static Show  row_to_show(sqlite3_stmt* stmt);
    static Movie row_to_movie(sqlite3_stmt* stmt);
    static Queue row_to_queue(sqlite3_stmt* stmt);
};

} // namespace FlickImp

// SN: 00006
