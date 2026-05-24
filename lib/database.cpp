// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "database.hpp"
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
    next_episode_title   TEXT    NOT NULL DEFAULT ''
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
    return sh;
}

std::vector<Show> Database::all_shows() {
    const char* sql =
        "SELECT id,title,service,season,episode,imdb_id,total_episodes,status,"
        "notes,thumbnail_url,tmdb_id,latest_season,latest_episode,season_episodes,next_season,next_episode,next_episode_title FROM shows ORDER BY title COLLATE NOCASE;";
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
        "notes,thumbnail_url,tmdb_id,latest_season,latest_episode,season_episodes,next_season,next_episode,next_episode_title FROM shows WHERE id=?;";
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
        "thumbnail_url,tmdb_id,latest_season,latest_episode,season_episodes,next_season,next_episode,next_episode_title) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
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
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::update_show(const Show& s) {
    const char* sql =
        "UPDATE shows SET title=?,service=?,season=?,episode=?,imdb_id=?,"
        "total_episodes=?,status=?,notes=?,thumbnail_url=?,tmdb_id=?,"
        "latest_season=?,latest_episode=?,season_episodes=?,next_season=?,next_episode=?,"
        "next_episode_title=? WHERE id=?;";
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
    sqlite3_bind_int (stmt, 17, s.id);
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

} // namespace FlickImp

// SN: 00004
