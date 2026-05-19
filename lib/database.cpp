// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "database.hpp"
#include <stdexcept>

namespace FlickImp {

namespace {

const char* SCHEMA = R"sql(
CREATE TABLE IF NOT EXISTS shows (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    title          TEXT    NOT NULL,
    service        TEXT    NOT NULL DEFAULT '',
    season         INTEGER NOT NULL DEFAULT 1,
    episode        INTEGER NOT NULL DEFAULT 1,
    imdb_id        TEXT    NOT NULL DEFAULT '',
    total_episodes INTEGER NOT NULL DEFAULT 0,
    status         INTEGER NOT NULL DEFAULT 0,
    notes          TEXT    NOT NULL DEFAULT '',
    thumbnail_url  TEXT    NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS movies (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    title         TEXT    NOT NULL,
    imdb_id       TEXT    NOT NULL DEFAULT '',
    release_date  TEXT    NOT NULL DEFAULT '',
    thumbnail_url TEXT    NOT NULL DEFAULT '',
    status        INTEGER NOT NULL DEFAULT 0,
    notes         TEXT    NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS episode_watches (
    show_id  INTEGER NOT NULL REFERENCES shows(id) ON DELETE CASCADE,
    season   INTEGER NOT NULL,
    episode  INTEGER NOT NULL,
    PRIMARY KEY (show_id, season, episode)
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
    return sh;
}

std::vector<Show> Database::all_shows() {
    const char* sql =
        "SELECT id,title,service,season,episode,imdb_id,total_episodes,status,"
        "notes,thumbnail_url,tmdb_id FROM shows ORDER BY title COLLATE NOCASE;";
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
        "notes,thumbnail_url,tmdb_id FROM shows WHERE id=?;";
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
        "thumbnail_url,tmdb_id) VALUES (?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt,  1, s.title.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  2, s.service.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt,  3, s.season);
    sqlite3_bind_int (stmt,  4, s.episode);
    sqlite3_bind_text(stmt,  5, s.imdb_id.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt,  6, s.total_episodes);
    sqlite3_bind_int (stmt,  7, static_cast<int>(s.status));
    sqlite3_bind_text(stmt,  8, s.notes.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  9, s.thumbnail_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 10, s.tmdb_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::update_show(const Show& s) {
    const char* sql =
        "UPDATE shows SET title=?,service=?,season=?,episode=?,imdb_id=?,"
        "total_episodes=?,status=?,notes=?,thumbnail_url=?,tmdb_id=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt,  1, s.title.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  2, s.service.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt,  3, s.season);
    sqlite3_bind_int (stmt,  4, s.episode);
    sqlite3_bind_text(stmt,  5, s.imdb_id.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt,  6, s.total_episodes);
    sqlite3_bind_int (stmt,  7, static_cast<int>(s.status));
    sqlite3_bind_text(stmt,  8, s.notes.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  9, s.thumbnail_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 10, s.tmdb_id);
    sqlite3_bind_int (stmt, 11, s.id);
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
    return m;
}

std::vector<Movie> Database::all_movies() {
    const char* sql =
        "SELECT id,title,imdb_id,release_date,thumbnail_url,status,notes,tmdb_id "
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
        "SELECT id,title,imdb_id,release_date,thumbnail_url,status,notes,tmdb_id "
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
        "(title,imdb_id,release_date,thumbnail_url,status,notes,tmdb_id) "
        "VALUES (?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, m.title.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, m.imdb_id.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, m.release_date.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, m.thumbnail_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 5, static_cast<int>(m.status));
    sqlite3_bind_text(stmt, 6, m.notes.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 7, m.tmdb_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::update_movie(const Movie& m) {
    const char* sql =
        "UPDATE movies SET title=?,imdb_id=?,release_date=?,thumbnail_url=?,"
        "status=?,notes=?,tmdb_id=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, m.title.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, m.imdb_id.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, m.release_date.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, m.thumbnail_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 5, static_cast<int>(m.status));
    sqlite3_bind_text(stmt, 6, m.notes.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 7, m.tmdb_id);
    sqlite3_bind_int (stmt, 8, m.id);
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

} // namespace FlickImp

// SN: 00002
