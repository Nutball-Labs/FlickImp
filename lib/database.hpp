// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include "models.hpp"
#include <sqlite3.h>
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

// SN: 00001
