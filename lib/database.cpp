// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "database.hpp"
#include "pin.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace FlickImp {

namespace {

const char* SCHEMA = R"sql(
CREATE TABLE IF NOT EXISTS shows (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    title            TEXT    NOT NULL,
    service          TEXT    NOT NULL DEFAULT '',
    season           INTEGER NOT NULL DEFAULT 1,
    episode          INTEGER NOT NULL DEFAULT 1,
    imdb_id          TEXT    NOT NULL DEFAULT '',
    total_episodes   INTEGER NOT NULL DEFAULT 0,
    status           INTEGER NOT NULL DEFAULT 0,
    notes            TEXT    NOT NULL DEFAULT '',
    thumbnail_url    TEXT    NOT NULL DEFAULT '',
    tmdb_id          INTEGER NOT NULL DEFAULT 0,
    latest_season    INTEGER NOT NULL DEFAULT 0,
    latest_episode   INTEGER NOT NULL DEFAULT 0,
    season_episodes  INTEGER NOT NULL DEFAULT 0,
    next_season          INTEGER NOT NULL DEFAULT 0,
    next_episode         INTEGER NOT NULL DEFAULT 0,
    next_episode_title   TEXT    NOT NULL DEFAULT '',
    queue                INTEGER NOT NULL DEFAULT 0,
    sort_order           INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS movies (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    title         TEXT    NOT NULL,
    imdb_id       TEXT    NOT NULL DEFAULT '',
    release_date  TEXT    NOT NULL DEFAULT '',
    thumbnail_url TEXT    NOT NULL DEFAULT '',
    status        INTEGER NOT NULL DEFAULT 0,
    notes         TEXT    NOT NULL DEFAULT '',
    sort_order    INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS episode_watches (
    show_id  INTEGER NOT NULL REFERENCES shows(id) ON DELETE CASCADE,
    season   INTEGER NOT NULL,
    episode  INTEGER NOT NULL,
    PRIMARY KEY (show_id, season, episode)
);
CREATE TABLE IF NOT EXISTS people (
    tmdb_id     INTEGER PRIMARY KEY,
    name        TEXT NOT NULL DEFAULT '',
    imdb_id     TEXT NOT NULL DEFAULT '',
    profile_url TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS show_cast (
    show_id        INTEGER NOT NULL REFERENCES shows(id) ON DELETE CASCADE,
    tmdb_person_id INTEGER NOT NULL,
    character      TEXT NOT NULL DEFAULT '',
    sort_order     INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (show_id, tmdb_person_id)
);
CREATE TABLE IF NOT EXISTS movie_cast (
    movie_id       INTEGER NOT NULL REFERENCES movies(id) ON DELETE CASCADE,
    tmdb_person_id INTEGER NOT NULL,
    character      TEXT NOT NULL DEFAULT '',
    sort_order     INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (movie_id, tmdb_person_id)
);
CREATE TABLE IF NOT EXISTS episode_cast (
    show_id        INTEGER NOT NULL,
    season         INTEGER NOT NULL,
    episode        INTEGER NOT NULL,
    tmdb_person_id INTEGER NOT NULL,
    character      TEXT NOT NULL DEFAULT '',
    sort_order     INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (show_id, season, episode, tmdb_person_id)
);
CREATE TABLE IF NOT EXISTS episode_imdb_ids (
    tmdb_show_id INTEGER NOT NULL,
    season       INTEGER NOT NULL,
    episode      INTEGER NOT NULL,
    imdb_id      TEXT    NOT NULL DEFAULT '',
    PRIMARY KEY (tmdb_show_id, season, episode)
);
CREATE TABLE IF NOT EXISTS queues (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT    NOT NULL,
    sort_order INTEGER NOT NULL DEFAULT 0,
    pin        TEXT    NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS tmdb_episodes (
    tmdb_show_id INTEGER NOT NULL,
    season       INTEGER NOT NULL,
    episode      INTEGER NOT NULL,
    title        TEXT    NOT NULL DEFAULT '',
    air_date     TEXT    NOT NULL DEFAULT '',
    PRIMARY KEY (tmdb_show_id, season, episode)
);
CREATE TABLE IF NOT EXISTS tmdb_seasons (
    tmdb_show_id INTEGER NOT NULL,
    season       INTEGER NOT NULL,
    fetched      TEXT    NOT NULL DEFAULT '',
    final        INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (tmdb_show_id, season)
);
CREATE TABLE IF NOT EXISTS show_groups (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT    NOT NULL
);
CREATE TABLE IF NOT EXISTS settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL DEFAULT ''
);
)sql";

std::string col_text(sqlite3_stmt* s, int i) {
    const unsigned char* v = sqlite3_column_text(s, i);
    return v ? reinterpret_cast<const char*>(v) : "";
}

} // namespace

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "unknown";
        sqlite3_close(db_);
        throw DbError("Cannot open database: " + err);
    }
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");
    create_schema();
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw DbError("SQL error: " + msg);
    }
}

void Database::create_schema() {
    exec(SCHEMA);
    // Migrations — safe to run repeatedly; SQLite ignores errors on existing columns.
    sqlite3_exec(db_,
        "ALTER TABLE movies ADD COLUMN release_date TEXT NOT NULL DEFAULT '';",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN thumbnail_url TEXT NOT NULL DEFAULT '';",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE movies ADD COLUMN thumbnail_url TEXT NOT NULL DEFAULT '';",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN tmdb_id INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE movies ADD COLUMN tmdb_id INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN latest_season INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN latest_episode INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN season_episodes INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN next_season INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN next_episode INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN next_episode_title TEXT NOT NULL DEFAULT '';",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN queue INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE movies ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN queue_id INTEGER NOT NULL DEFAULT 1;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE movies ADD COLUMN queue_id INTEGER NOT NULL DEFAULT 1;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN group_id INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE shows ADD COLUMN group_order INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    // Seed default queue if none exist
    {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM queues;", -1, &stmt, nullptr);
        bool empty = (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0);
        sqlite3_finalize(stmt);
        if (empty)
            sqlite3_exec(db_,
                "INSERT INTO queues (name, sort_order) VALUES ('Default', 1);",
                nullptr, nullptr, nullptr);
    }
    // PINs set before hashing existed were stored in plain text
    hash_plain_pins();
}

void Database::hash_plain_pins() {
    for (auto q : all_queues()) {
        if (q.pin.empty() || Pin::is_hashed(q.pin)) continue;
        q.pin = Pin::hash(q.pin);
        update_queue(q);
    }
}

// ---------- Shows --------------------------------------------------------

Show Database::row_to_show(sqlite3_stmt* s) {
    Show sh;
    sh.id             = sqlite3_column_int(s, 0);
    sh.title          = col_text(s, 1);
    sh.service        = col_text(s, 2);
    sh.season         = sqlite3_column_int(s, 3);
    sh.episode        = sqlite3_column_int(s, 4);
    sh.imdb_id        = col_text(s, 5);
    sh.total_episodes = sqlite3_column_int(s, 6);
    sh.status         = static_cast<ShowStatus>(sqlite3_column_int(s, 7));
    sh.notes          = col_text(s, 8);
    sh.thumbnail_url  = col_text(s, 9);
    sh.tmdb_id        = sqlite3_column_int(s, 10);
    sh.latest_season   = sqlite3_column_int(s, 11);
    sh.latest_episode  = sqlite3_column_int(s, 12);
    sh.season_episodes = sqlite3_column_int(s, 13);
    sh.next_season         = sqlite3_column_int(s, 14);
    sh.next_episode        = sqlite3_column_int(s, 15);
    sh.next_episode_title  = col_text(s, 16);
    sh.queue               = static_cast<ShowQueue>(sqlite3_column_int(s, 17));
    sh.sort_order          = sqlite3_column_int(s, 18);
    sh.queue_id            = sqlite3_column_int(s, 19);
    sh.group_id            = sqlite3_column_int(s, 20);
    sh.group_order         = sqlite3_column_int(s, 21);
    return sh;
}

std::vector<Show> Database::all_shows() {
    const char* sql =
        "SELECT id,title,service,season,episode,imdb_id,total_episodes,status,"
        "notes,thumbnail_url,tmdb_id,latest_season,latest_episode,season_episodes,"
        "next_season,next_episode,next_episode_title,queue,sort_order,queue_id,"
        "group_id,group_order "
        "FROM shows ORDER BY title COLLATE NOCASE;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    std::vector<Show> result;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        result.push_back(row_to_show(stmt));
    sqlite3_finalize(stmt);
    return result;
}

Show Database::get_show(int id) {
    const char* sql =
        "SELECT id,title,service,season,episode,imdb_id,total_episodes,status,"
        "notes,thumbnail_url,tmdb_id,latest_season,latest_episode,season_episodes,"
        "next_season,next_episode,next_episode_title,queue,sort_order,queue_id,"
        "group_id,group_order "
        "FROM shows WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        throw DbError("Show not found: " + std::to_string(id));
    }
    Show sh = row_to_show(stmt);
    sqlite3_finalize(stmt);
    return sh;
}

int Database::add_show(const Show& s) {
    const char* sql =
        "INSERT INTO shows "
        "(title,service,season,episode,imdb_id,total_episodes,status,notes,"
        "thumbnail_url,tmdb_id,latest_season,latest_episode,season_episodes,"
        "next_season,next_episode,next_episode_title,queue,sort_order,queue_id) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt,  1, s.title.c_str(),               -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  2, s.service.c_str(),              -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt,  3, s.season);
    sqlite3_bind_int (stmt,  4, s.episode);
    sqlite3_bind_text(stmt,  5, s.imdb_id.c_str(),              -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt,  6, s.total_episodes);
    sqlite3_bind_int (stmt,  7, static_cast<int>(s.status));
    sqlite3_bind_text(stmt,  8, s.notes.c_str(),                -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  9, s.thumbnail_url.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 10, s.tmdb_id);
    sqlite3_bind_int (stmt, 11, s.latest_season);
    sqlite3_bind_int (stmt, 12, s.latest_episode);
    sqlite3_bind_int (stmt, 13, s.season_episodes);
    sqlite3_bind_int (stmt, 14, s.next_season);
    sqlite3_bind_int (stmt, 15, s.next_episode);
    sqlite3_bind_text(stmt, 16, s.next_episode_title.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 17, static_cast<int>(s.queue));
    sqlite3_bind_int (stmt, 18, s.sort_order);
    sqlite3_bind_int (stmt, 19, s.queue_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::update_show(const Show& s) {
    const char* sql =
        "UPDATE shows SET title=?,service=?,season=?,episode=?,imdb_id=?,"
        "total_episodes=?,status=?,notes=?,thumbnail_url=?,tmdb_id=?,"
        "latest_season=?,latest_episode=?,season_episodes=?,next_season=?,next_episode=?,"
        "next_episode_title=?,queue=?,sort_order=?,queue_id=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt,  1, s.title.c_str(),               -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  2, s.service.c_str(),              -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt,  3, s.season);
    sqlite3_bind_int (stmt,  4, s.episode);
    sqlite3_bind_text(stmt,  5, s.imdb_id.c_str(),              -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt,  6, s.total_episodes);
    sqlite3_bind_int (stmt,  7, static_cast<int>(s.status));
    sqlite3_bind_text(stmt,  8, s.notes.c_str(),                -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  9, s.thumbnail_url.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 10, s.tmdb_id);
    sqlite3_bind_int (stmt, 11, s.latest_season);
    sqlite3_bind_int (stmt, 12, s.latest_episode);
    sqlite3_bind_int (stmt, 13, s.season_episodes);
    sqlite3_bind_int (stmt, 14, s.next_season);
    sqlite3_bind_int (stmt, 15, s.next_episode);
    sqlite3_bind_text(stmt, 16, s.next_episode_title.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 17, static_cast<int>(s.queue));
    sqlite3_bind_int (stmt, 18, s.sort_order);
    sqlite3_bind_int (stmt, 19, s.queue_id);
    sqlite3_bind_int (stmt, 20, s.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::delete_show(int id) {
    const char* sql = "DELETE FROM shows WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ---------- Movies -------------------------------------------------------

Movie Database::row_to_movie(sqlite3_stmt* s) {
    Movie m;
    m.id            = sqlite3_column_int(s, 0);
    m.title         = col_text(s, 1);
    m.imdb_id       = col_text(s, 2);
    m.release_date  = col_text(s, 3);
    m.thumbnail_url = col_text(s, 4);
    m.status        = static_cast<MovieStatus>(sqlite3_column_int(s, 5));
    m.notes         = col_text(s, 6);
    m.tmdb_id       = sqlite3_column_int(s, 7);
    m.sort_order    = sqlite3_column_int(s, 8);
    m.queue_id      = sqlite3_column_int(s, 9);
    return m;
}

// ---------- Episode IMDB ID cache -----------------------------------------

bool Database::episode_imdb_cached(int tmdb_show_id, int season, int episode) {
    const char* sql =
        "SELECT COUNT(*) FROM episode_imdb_ids WHERE tmdb_show_id=? AND season=? AND episode=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, tmdb_show_id);
    sqlite3_bind_int(stmt, 2, season);
    sqlite3_bind_int(stmt, 3, episode);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0);
    sqlite3_finalize(stmt);
    return found;
}

void Database::cache_episode_imdb(int tmdb_show_id, int season, int episode,
                                  const std::string& imdb_id) {
    const char* sql =
        "INSERT OR REPLACE INTO episode_imdb_ids (tmdb_show_id,season,episode,imdb_id) VALUES (?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int (stmt, 1, tmdb_show_id);
    sqlite3_bind_int (stmt, 2, season);
    sqlite3_bind_int (stmt, 3, episode);
    sqlite3_bind_text(stmt, 4, imdb_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::string Database::get_episode_imdb_id(int tmdb_show_id, int season, int episode) {
    const char* sql =
        "SELECT imdb_id FROM episode_imdb_ids WHERE tmdb_show_id=? AND season=? AND episode=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, tmdb_show_id);
    sqlite3_bind_int(stmt, 2, season);
    sqlite3_bind_int(stmt, 3, episode);
    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = col_text(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

// ---------- Movies --------------------------------------------------------

std::vector<Movie> Database::all_movies() {
    const char* sql =
        "SELECT id,title,imdb_id,release_date,thumbnail_url,status,notes,tmdb_id,sort_order,queue_id "
        "FROM movies ORDER BY title COLLATE NOCASE;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    std::vector<Movie> result;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        result.push_back(row_to_movie(stmt));
    sqlite3_finalize(stmt);
    return result;
}

Movie Database::get_movie(int id) {
    const char* sql =
        "SELECT id,title,imdb_id,release_date,thumbnail_url,status,notes,tmdb_id,sort_order,queue_id "
        "FROM movies WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        throw DbError("Movie not found: " + std::to_string(id));
    }
    Movie m = row_to_movie(stmt);
    sqlite3_finalize(stmt);
    return m;
}

int Database::add_movie(const Movie& m) {
    const char* sql =
        "INSERT INTO movies "
        "(title,imdb_id,release_date,thumbnail_url,status,notes,tmdb_id,sort_order,queue_id) "
        "VALUES (?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, m.title.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, m.imdb_id.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, m.release_date.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, m.thumbnail_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 5, static_cast<int>(m.status));
    sqlite3_bind_text(stmt, 6, m.notes.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 7, m.tmdb_id);
    sqlite3_bind_int (stmt, 8, m.sort_order);
    sqlite3_bind_int (stmt, 9, m.queue_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::update_movie(const Movie& m) {
    const char* sql =
        "UPDATE movies SET title=?,imdb_id=?,release_date=?,thumbnail_url=?,"
        "status=?,notes=?,tmdb_id=?,sort_order=?,queue_id=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, m.title.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, m.imdb_id.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, m.release_date.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, m.thumbnail_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 5, static_cast<int>(m.status));
    sqlite3_bind_text(stmt, 6, m.notes.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 7, m.tmdb_id);
    sqlite3_bind_int (stmt, 8, m.sort_order);
    sqlite3_bind_int (stmt, 9, m.queue_id);
    sqlite3_bind_int (stmt, 10, m.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::delete_movie(int id) {
    const char* sql = "DELETE FROM movies WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ---------- Episode watches -----------------------------------------------

std::set<int> Database::get_watched_episodes(int show_id, int season) {
    const char* sql =
        "SELECT episode FROM episode_watches WHERE show_id=? AND season=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, show_id);
    sqlite3_bind_int(stmt, 2, season);
    std::set<int> result;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        result.insert(sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);
    return result;
}

std::map<int,int> Database::get_watched_counts(int show_id) {
    const char* sql =
        "SELECT season, COUNT(*) FROM episode_watches WHERE show_id=? GROUP BY season;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, show_id);
    std::map<int,int> result;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        result[sqlite3_column_int(stmt, 0)] = sqlite3_column_int(stmt, 1);
    sqlite3_finalize(stmt);
    return result;
}

std::pair<int,int> Database::compute_next_unwatched(int show_id, int season, int episode, int season_eps) {
    if (season == 0) return {1, 1};
    auto watched = get_watched_episodes(show_id, season);
    int upper = (season_eps > 0) ? season_eps
              : std::max(episode + 1, watched.empty() ? episode + 1 : (int)*watched.rbegin());
    for (int ep = episode + 1; ep <= upper; ep++) {
        if (!watched.count(ep)) return {season, ep};
    }
    return {season + 1, 1};
}

std::pair<int,int> Database::max_watched_position(int show_id) {
    const char* sql =
        "SELECT season, episode FROM episode_watches "
        "WHERE show_id=? ORDER BY season DESC, episode DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, show_id);
    std::pair<int,int> result{0, 0};
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = {sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1)};
    sqlite3_finalize(stmt);
    return result;
}

void Database::set_episode_watched(int show_id, int season, int episode, bool watched) {
    const char* sql = watched
        ? "INSERT OR IGNORE INTO episode_watches (show_id,season,episode) VALUES (?,?,?);"
        : "DELETE FROM episode_watches WHERE show_id=? AND season=? AND episode=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, show_id);
    sqlite3_bind_int(stmt, 2, season);
    sqlite3_bind_int(stmt, 3, episode);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::clear_season_watched(int show_id, int season) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "DELETE FROM episode_watches WHERE show_id = ? AND season = ?;",
                       -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, show_id);
    sqlite3_bind_int(stmt, 2, season);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int Database::mark_watched_through(int show_id,
                                   const std::vector<std::pair<int,int>>& season_last) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT OR IGNORE INTO episode_watches (show_id,season,episode) VALUES (?,?,?);",
        -1, &stmt, nullptr);
    int added = 0;
    exec("BEGIN;");
    for (const auto& [season, last] : season_last) {
        for (int ep = 1; ep <= last; ++ep) {
            sqlite3_bind_int(stmt, 1, show_id);
            sqlite3_bind_int(stmt, 2, season);
            sqlite3_bind_int(stmt, 3, ep);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::string err = sqlite3_errmsg(db_);
                sqlite3_finalize(stmt);
                sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
                throw DbError("mark_watched_through: " + err);
            }
            added += sqlite3_changes(db_);
            sqlite3_reset(stmt);
        }
    }
    sqlite3_finalize(stmt);
    exec("COMMIT;");
    return added;
}

// ---------- People cache --------------------------------------------------

void Database::upsert_person(int tmdb_id, const std::string& name,
                             const std::string& imdb_id, const std::string& profile_url) {
    const char* sql =
        "INSERT OR REPLACE INTO people (tmdb_id,name,imdb_id,profile_url) VALUES (?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int (stmt, 1, tmdb_id);
    sqlite3_bind_text(stmt, 2, name.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, imdb_id.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, profile_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

bool Database::person_cached(int tmdb_person_id) {
    const char* sql = "SELECT COUNT(*) FROM people WHERE tmdb_id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, tmdb_person_id);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0);
    sqlite3_finalize(stmt);
    return found;
}

std::string Database::get_person_imdb_id(int tmdb_person_id) {
    const char* sql = "SELECT imdb_id FROM people WHERE tmdb_id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, tmdb_person_id);
    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = col_text(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

// ---------- Show cast -----------------------------------------------------

bool Database::show_cast_cached(int show_id) {
    const char* sql = "SELECT COUNT(*) FROM show_cast WHERE show_id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, show_id);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0);
    sqlite3_finalize(stmt);
    return found;
}

void Database::store_show_cast(int show_id, const std::vector<CastMember>& cast) {
    exec("BEGIN;");
    sqlite3_stmt* del = nullptr;
    sqlite3_prepare_v2(db_, "DELETE FROM show_cast WHERE show_id=?;", -1, &del, nullptr);
    sqlite3_bind_int(del, 1, show_id);
    sqlite3_step(del);
    sqlite3_finalize(del);

    const char* ins =
        "INSERT OR IGNORE INTO show_cast (show_id,tmdb_person_id,character,sort_order) VALUES (?,?,?,?);";
    for (const auto& cm : cast) {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_, ins, -1, &stmt, nullptr);
        sqlite3_bind_int (stmt, 1, show_id);
        sqlite3_bind_int (stmt, 2, cm.tmdb_person_id);
        sqlite3_bind_text(stmt, 3, cm.character.c_str(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_int (stmt, 4, cm.sort_order);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    exec("COMMIT;");
}

std::vector<CastMember> Database::get_show_cast(int show_id) {
    const char* sql =
        "SELECT sc.tmdb_person_id, sc.character, sc.sort_order, "
        "p.name, p.imdb_id, p.profile_url "
        "FROM show_cast sc JOIN people p ON p.tmdb_id=sc.tmdb_person_id "
        "WHERE sc.show_id=? ORDER BY sc.sort_order;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, show_id);
    std::vector<CastMember> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CastMember cm;
        cm.tmdb_person_id = sqlite3_column_int(stmt, 0);
        cm.character      = col_text(stmt, 1);
        cm.sort_order     = sqlite3_column_int(stmt, 2);
        cm.name           = col_text(stmt, 3);
        cm.imdb_id        = col_text(stmt, 4);
        cm.profile_url    = col_text(stmt, 5);
        result.push_back(std::move(cm));
    }
    sqlite3_finalize(stmt);
    return result;
}

// ---------- Movie cast ----------------------------------------------------

bool Database::movie_cast_cached(int movie_id) {
    const char* sql = "SELECT COUNT(*) FROM movie_cast WHERE movie_id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, movie_id);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0);
    sqlite3_finalize(stmt);
    return found;
}

void Database::store_movie_cast(int movie_id, const std::vector<CastMember>& cast) {
    exec("BEGIN;");
    sqlite3_stmt* del = nullptr;
    sqlite3_prepare_v2(db_, "DELETE FROM movie_cast WHERE movie_id=?;", -1, &del, nullptr);
    sqlite3_bind_int(del, 1, movie_id);
    sqlite3_step(del);
    sqlite3_finalize(del);

    const char* ins =
        "INSERT OR IGNORE INTO movie_cast (movie_id,tmdb_person_id,character,sort_order) VALUES (?,?,?,?);";
    for (const auto& cm : cast) {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_, ins, -1, &stmt, nullptr);
        sqlite3_bind_int (stmt, 1, movie_id);
        sqlite3_bind_int (stmt, 2, cm.tmdb_person_id);
        sqlite3_bind_text(stmt, 3, cm.character.c_str(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_int (stmt, 4, cm.sort_order);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    exec("COMMIT;");
}

std::vector<CastMember> Database::get_movie_cast(int movie_id) {
    const char* sql =
        "SELECT mc.tmdb_person_id, mc.character, mc.sort_order, "
        "p.name, p.imdb_id, p.profile_url "
        "FROM movie_cast mc JOIN people p ON p.tmdb_id=mc.tmdb_person_id "
        "WHERE mc.movie_id=? ORDER BY mc.sort_order;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, movie_id);
    std::vector<CastMember> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CastMember cm;
        cm.tmdb_person_id = sqlite3_column_int(stmt, 0);
        cm.character      = col_text(stmt, 1);
        cm.sort_order     = sqlite3_column_int(stmt, 2);
        cm.name           = col_text(stmt, 3);
        cm.imdb_id        = col_text(stmt, 4);
        cm.profile_url    = col_text(stmt, 5);
        result.push_back(std::move(cm));
    }
    sqlite3_finalize(stmt);
    return result;
}

// ---------- Episode cast --------------------------------------------------

bool Database::episode_cast_cached(int show_id, int season, int episode) {
    const char* sql =
        "SELECT COUNT(*) FROM episode_cast WHERE show_id=? AND season=? AND episode=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, show_id);
    sqlite3_bind_int(stmt, 2, season);
    sqlite3_bind_int(stmt, 3, episode);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0);
    sqlite3_finalize(stmt);
    return found;
}

void Database::store_episode_cast(int show_id, int season, int episode,
                                  const std::vector<CastMember>& cast) {
    exec("BEGIN;");
    const char* del =
        "DELETE FROM episode_cast WHERE show_id=? AND season=? AND episode=?;";
    sqlite3_stmt* ds = nullptr;
    sqlite3_prepare_v2(db_, del, -1, &ds, nullptr);
    sqlite3_bind_int(ds, 1, show_id);
    sqlite3_bind_int(ds, 2, season);
    sqlite3_bind_int(ds, 3, episode);
    sqlite3_step(ds);
    sqlite3_finalize(ds);

    const char* ins =
        "INSERT OR IGNORE INTO episode_cast "
        "(show_id,season,episode,tmdb_person_id,character,sort_order) VALUES (?,?,?,?,?,?);";
    for (const auto& cm : cast) {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_, ins, -1, &stmt, nullptr);
        sqlite3_bind_int (stmt, 1, show_id);
        sqlite3_bind_int (stmt, 2, season);
        sqlite3_bind_int (stmt, 3, episode);
        sqlite3_bind_int (stmt, 4, cm.tmdb_person_id);
        sqlite3_bind_text(stmt, 5, cm.character.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int (stmt, 6, cm.sort_order);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    exec("COMMIT;");
}

std::vector<CastMember> Database::get_episode_cast(int show_id, int season, int episode) {
    const char* sql =
        "SELECT ec.tmdb_person_id, ec.character, ec.sort_order, "
        "p.name, p.imdb_id, p.profile_url "
        "FROM episode_cast ec JOIN people p ON p.tmdb_id=ec.tmdb_person_id "
        "WHERE ec.show_id=? AND ec.season=? AND ec.episode=? ORDER BY ec.sort_order;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, show_id);
    sqlite3_bind_int(stmt, 2, season);
    sqlite3_bind_int(stmt, 3, episode);
    std::vector<CastMember> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CastMember cm;
        cm.tmdb_person_id = sqlite3_column_int(stmt, 0);
        cm.character      = col_text(stmt, 1);
        cm.sort_order     = sqlite3_column_int(stmt, 2);
        cm.name           = col_text(stmt, 3);
        cm.imdb_id        = col_text(stmt, 4);
        cm.profile_url    = col_text(stmt, 5);
        result.push_back(std::move(cm));
    }
    sqlite3_finalize(stmt);
    return result;
}

// ---------- Sort order ----------------------------------------------------

int Database::max_show_sort_order(ShowQueue queue, int queue_id) {
    const char* sql = "SELECT MAX(sort_order) FROM shows WHERE queue=? AND queue_id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, static_cast<int>(queue));
    sqlite3_bind_int(stmt, 2, queue_id);
    int result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

int Database::max_movie_sort_order(int queue_id) {
    const char* sql = "SELECT MAX(sort_order) FROM movies WHERE queue_id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, queue_id);
    int result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

void Database::reorder_shows(const std::vector<int>& ids) {
    exec("BEGIN;");
    const char* sql = "UPDATE shows SET sort_order=? WHERE id=?;";
    for (size_t i = 0; i < ids.size(); ++i) {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, static_cast<int>(i + 1));
        sqlite3_bind_int(stmt, 2, ids[i]);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    exec("COMMIT;");
}

void Database::reorder_movies(const std::vector<int>& ids) {
    exec("BEGIN;");
    const char* sql = "UPDATE movies SET sort_order=? WHERE id=?;";
    for (size_t i = 0; i < ids.size(); ++i) {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, static_cast<int>(i + 1));
        sqlite3_bind_int(stmt, 2, ids[i]);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    exec("COMMIT;");
}

// ---------- Queues -----------------------------------------------------------

Queue Database::row_to_queue(sqlite3_stmt* s) {
    Queue q;
    q.id         = sqlite3_column_int(s, 0);
    q.name       = col_text(s, 1);
    q.sort_order = sqlite3_column_int(s, 2);
    q.pin        = col_text(s, 3);
    return q;
}

std::vector<Queue> Database::all_queues() {
    const char* sql = "SELECT id,name,sort_order,pin FROM queues ORDER BY sort_order,id;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    std::vector<Queue> result;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        result.push_back(row_to_queue(stmt));
    sqlite3_finalize(stmt);
    return result;
}

Queue Database::get_queue(int id) {
    const char* sql = "SELECT id,name,sort_order,pin FROM queues WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        throw DbError("Queue not found: " + std::to_string(id));
    }
    Queue q = row_to_queue(stmt);
    sqlite3_finalize(stmt);
    return q;
}

int Database::add_queue(const Queue& q) {
    const char* sql = "INSERT INTO queues (name,sort_order,pin) VALUES (?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, q.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 2, q.sort_order);
    sqlite3_bind_text(stmt, 3, q.pin.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::update_queue(const Queue& q) {
    const char* sql = "UPDATE queues SET name=?,sort_order=?,pin=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, q.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 2, q.sort_order);
    sqlite3_bind_text(stmt, 3, q.pin.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, q.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::delete_queue(int id) {
    exec("BEGIN;");
    {
        const char* sql = "UPDATE shows SET queue_id=1 WHERE queue_id=?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    {
        const char* sql = "UPDATE movies SET queue_id=1 WHERE queue_id=?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    {
        const char* sql = "DELETE FROM queues WHERE id=?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    exec("COMMIT;");
}

int Database::queue_count() {
    const char* sql = "SELECT COUNT(*) FROM queues;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    int result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

// ---------- TMDB episode list cache ---------------------------------------
// Season episode lists (number, title, air date) for group views, which need
// every season of every member. A "final" season (fully aired and not the
// show's latest) is never refetched; others are refreshed at most daily.

bool Database::season_list_fresh(int tmdb_show_id, int season, const std::string& today) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT fetched, final FROM tmdb_seasons WHERE tmdb_show_id = ? AND season = ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, tmdb_show_id);
    sqlite3_bind_int(stmt, 2, season);
    bool fresh = false;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        fresh = sqlite3_column_int(stmt, 1) != 0 || col_text(stmt, 0) == today;
    sqlite3_finalize(stmt);
    return fresh;
}

void Database::store_season_list(int tmdb_show_id, int season,
                                 const std::vector<std::tuple<int, std::string, std::string>>& eps,
                                 const std::string& today, bool final) {
    exec("BEGIN;");
    try {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_, "DELETE FROM tmdb_episodes WHERE tmdb_show_id = ? AND season = ?;",
                           -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, tmdb_show_id);
        sqlite3_bind_int(stmt, 2, season);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        sqlite3_prepare_v2(db_,
            "INSERT OR REPLACE INTO tmdb_episodes (tmdb_show_id,season,episode,title,air_date) "
            "VALUES (?,?,?,?,?);", -1, &stmt, nullptr);
        for (const auto& [ep, title, air] : eps) {
            sqlite3_bind_int (stmt, 1, tmdb_show_id);
            sqlite3_bind_int (stmt, 2, season);
            sqlite3_bind_int (stmt, 3, ep);
            sqlite3_bind_text(stmt, 4, title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, air.c_str(),   -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);

        sqlite3_prepare_v2(db_,
            "INSERT OR REPLACE INTO tmdb_seasons (tmdb_show_id,season,fetched,final) VALUES (?,?,?,?);",
            -1, &stmt, nullptr);
        sqlite3_bind_int (stmt, 1, tmdb_show_id);
        sqlite3_bind_int (stmt, 2, season);
        sqlite3_bind_text(stmt, 3, today.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int (stmt, 4, final ? 1 : 0);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        exec("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;
    }
}

std::vector<std::tuple<int, std::string, std::string>>
Database::season_list(int tmdb_show_id, int season) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT episode, title, air_date FROM tmdb_episodes "
        "WHERE tmdb_show_id = ? AND season = ? ORDER BY episode;", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, tmdb_show_id);
    sqlite3_bind_int(stmt, 2, season);
    std::vector<std::tuple<int, std::string, std::string>> eps;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        eps.emplace_back(sqlite3_column_int(stmt, 0), col_text(stmt, 1), col_text(stmt, 2));
    sqlite3_finalize(stmt);
    return eps;
}

// ---------- Show groups --------------------------------------------------
// Group columns are written only here, never by add_show/update_show, so an
// ordinary show edit can't knock a show out of its group.

std::vector<ShowGroup> Database::all_groups() {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT id, name FROM show_groups ORDER BY id;", -1, &stmt, nullptr);
    std::vector<ShowGroup> result;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        result.push_back({sqlite3_column_int(stmt, 0), col_text(stmt, 1)});
    sqlite3_finalize(stmt);
    return result;
}

bool Database::group_exists(int id) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT 1 FROM show_groups WHERE id = ?;", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    bool found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

int Database::create_group(const std::string& name) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "INSERT INTO show_groups (name) VALUES (?);", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) throw DbError(std::string("create_group: ") + sqlite3_errmsg(db_));
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::rename_group(int id, const std::string& name) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE show_groups SET name = ? WHERE id = ?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::set_show_group(int show_id, int group_id, int group_order) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE shows SET group_id = ?, group_order = ? WHERE id = ?;",
                       -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, group_id ? group_order : 0);
    sqlite3_bind_int(stmt, 3, show_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int Database::max_group_order(int group_id) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT COALESCE(MAX(group_order), 0) FROM shows WHERE group_id = ?;",
                       -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, group_id);
    int n = (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int(stmt, 0) : 0;
    sqlite3_finalize(stmt);
    return n;
}

void Database::delete_group(int id) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE shows SET group_id = 0, group_order = 0 WHERE group_id = ?;",
                       -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_prepare_v2(db_, "DELETE FROM show_groups WHERE id = ?;", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::prune_groups() {
    // A group of one is just a show: release lone members, then drop empty groups
    exec("UPDATE shows SET group_id = 0, group_order = 0 WHERE group_id IN ("
         "  SELECT group_id FROM shows WHERE group_id > 0"
         "  GROUP BY group_id HAVING COUNT(*) < 2);");
    exec("UPDATE shows SET group_id = 0, group_order = 0 WHERE group_id > 0 "
         "AND group_id NOT IN (SELECT id FROM show_groups);");
    exec("DELETE FROM show_groups WHERE id NOT IN ("
         "  SELECT DISTINCT group_id FROM shows WHERE group_id > 0);");
}

// ---------- Settings -----------------------------------------------------

std::string Database::get_setting(const std::string& key) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT value FROM settings WHERE key = ?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    std::string v;
    if (sqlite3_step(stmt) == SQLITE_ROW) v = col_text(stmt, 0);
    sqlite3_finalize(stmt);
    return v;
}

void Database::set_setting(const std::string& key, const std::string& value) {
    sqlite3_stmt* stmt = nullptr;
    if (value.empty()) {
        sqlite3_prepare_v2(db_, "DELETE FROM settings WHERE key = ?;", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_prepare_v2(db_,
            "INSERT INTO settings (key, value) VALUES (?, ?) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value;",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, key.c_str(),   -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    }
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) throw DbError(std::string("set_setting: ") + sqlite3_errmsg(db_));
}

// ---------- Generic table access (backup / restore) -----------------------
// Table names are never taken from user input: callers pass names from a
// fixed list. Column names in insert_row() are filtered against the live
// schema, so a backup from an older or newer version can't inject SQL and
// simply drops (or defaults) columns that don't match.

std::vector<std::string> Database::table_columns(const std::string& table) {
    std::vector<std::string> cols;
    sqlite3_stmt* stmt = nullptr;
    std::string sql = "PRAGMA table_info(" + table + ");";
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW)
        cols.push_back(col_text(stmt, 1));
    sqlite3_finalize(stmt);
    return cols;
}

nlohmann::json Database::dump_table(const std::string& table) {
    auto rows = nlohmann::json::array();
    sqlite3_stmt* stmt = nullptr;
    std::string sql = "SELECT * FROM " + table + ";";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        throw DbError("dump_table " + table + ": " + sqlite3_errmsg(db_));
    int n = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        nlohmann::json row = nlohmann::json::object();
        for (int i = 0; i < n; ++i) {
            const char* name = sqlite3_column_name(stmt, i);
            switch (sqlite3_column_type(stmt, i)) {
                case SQLITE_INTEGER: row[name] = sqlite3_column_int64(stmt, i);  break;
                case SQLITE_FLOAT:   row[name] = sqlite3_column_double(stmt, i); break;
                case SQLITE_NULL:    row[name] = nullptr;                        break;
                default:             row[name] = col_text(stmt, i);              break;
            }
        }
        rows.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return rows;
}

long long Database::insert_row(const std::string& table, const nlohmann::json& row,
                               const char* verb) {
    std::vector<std::string> cols;
    for (const auto& c : table_columns(table))
        if (row.contains(c)) cols.push_back(c);
    if (cols.empty()) return 0;

    std::string sql = std::string(verb) + " INTO " + table + " (";
    std::string marks;
    for (size_t i = 0; i < cols.size(); ++i) {
        if (i) { sql += ", "; marks += ", "; }
        sql += cols[i];
        marks += "?";
    }
    sql += ") VALUES (" + marks + ");";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        throw DbError("insert_row " + table + ": " + sqlite3_errmsg(db_));
    for (size_t i = 0; i < cols.size(); ++i) {
        const auto& v = row[cols[i]];
        int idx = static_cast<int>(i) + 1;
        if      (v.is_null())            sqlite3_bind_null(stmt, idx);
        else if (v.is_boolean())         sqlite3_bind_int(stmt, idx, v.get<bool>() ? 1 : 0);
        else if (v.is_number_integer())  sqlite3_bind_int64(stmt, idx, v.get<long long>());
        else if (v.is_number())          sqlite3_bind_double(stmt, idx, v.get<double>());
        else if (v.is_string())          sqlite3_bind_text(stmt, idx, v.get<std::string>().c_str(), -1, SQLITE_TRANSIENT);
        else                             sqlite3_bind_text(stmt, idx, v.dump().c_str(), -1, SQLITE_TRANSIENT);
    }
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        throw DbError("insert_row " + table + ": " + sqlite3_errmsg(db_));
    return sqlite3_changes(db_) ? sqlite3_last_insert_rowid(db_) : 0;
}

void Database::clear_table(const std::string& table) {
    exec(("DELETE FROM " + table + ";").c_str());
}

void Database::begin()    { exec("BEGIN IMMEDIATE;"); }
void Database::commit()   { exec("COMMIT;"); }
void Database::rollback() { sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); }

void Database::snapshot_to(const std::string& path) {
    // VACUUM INTO gives a consistent copy of a live WAL-mode DB; it refuses
    // to overwrite, so clear any previous snapshot first.
    std::error_code ec;
    std::filesystem::remove(path, ec);
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "VACUUM INTO ?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        throw DbError("snapshot to " + path + " failed: " + sqlite3_errmsg(db_));
}

std::string Database::path() const {
    const char* p = sqlite3_db_filename(db_, "main");
    return p ? p : "";
}

} // namespace FlickImp

// SN: 00006
