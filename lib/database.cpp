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
    notes          TEXT    NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS movies (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    title   TEXT    NOT NULL,
    imdb_id TEXT    NOT NULL DEFAULT '',
    status  INTEGER NOT NULL DEFAULT 0,
    notes   TEXT    NOT NULL DEFAULT ''
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
    return sh;
}

std::vector<Show> Database::all_shows() {
    const char* sql =
        "SELECT id,title,service,season,episode,imdb_id,total_episodes,status,notes "
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
        "SELECT id,title,service,season,episode,imdb_id,total_episodes,status,notes "
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
        "INSERT INTO shows (title,service,season,episode,imdb_id,total_episodes,status,notes) "
        "VALUES (?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, s.title.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s.service.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, s.season);
    sqlite3_bind_int (stmt, 4, s.episode);
    sqlite3_bind_text(stmt, 5, s.imdb_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 6, s.total_episodes);
    sqlite3_bind_int (stmt, 7, static_cast<int>(s.status));
    sqlite3_bind_text(stmt, 8, s.notes.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::update_show(const Show& s) {
    const char* sql =
        "UPDATE shows SET title=?,service=?,season=?,episode=?,"
        "imdb_id=?,total_episodes=?,status=?,notes=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, s.title.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s.service.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, s.season);
    sqlite3_bind_int (stmt, 4, s.episode);
    sqlite3_bind_text(stmt, 5, s.imdb_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 6, s.total_episodes);
    sqlite3_bind_int (stmt, 7, static_cast<int>(s.status));
    sqlite3_bind_text(stmt, 8, s.notes.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 9, s.id);
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
    m.id     = sqlite3_column_int(s, 0);
    m.title  = col_text(s, 1);
    m.imdb_id = col_text(s, 2);
    m.status = static_cast<MovieStatus>(sqlite3_column_int(s, 3));
    m.notes  = col_text(s, 4);
    return m;
}

std::vector<Movie> Database::all_movies() {
    const char* sql =
        "SELECT id,title,imdb_id,status,notes FROM movies ORDER BY title COLLATE NOCASE;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    std::vector<Movie> result;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        result.push_back(row_to_movie(stmt));
    sqlite3_finalize(stmt);
    return result;
}

Movie Database::get_movie(int id) {
    const char* sql = "SELECT id,title,imdb_id,status,notes FROM movies WHERE id=?;";
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
        "INSERT INTO movies (title,imdb_id,status,notes) VALUES (?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, m.title.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, m.imdb_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, static_cast<int>(m.status));
    sqlite3_bind_text(stmt, 4, m.notes.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::update_movie(const Movie& m) {
    const char* sql =
        "UPDATE movies SET title=?,imdb_id=?,status=?,notes=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, m.title.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, m.imdb_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, static_cast<int>(m.status));
    sqlite3_bind_text(stmt, 4, m.notes.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 5, m.id);
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

} // namespace FlickImp

// SN: 00001
