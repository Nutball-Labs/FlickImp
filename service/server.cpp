// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "server.hpp"
#include "../lib/database.hpp"
#include "../lib/models.hpp"
#include <httplib.h>
#include <json.hpp>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>

using json = nlohmann::json;

namespace FlickImp {

// ---------- JSON helpers --------------------------------------------------

static json show_to_json(const Show& s) {
    static const char* status_str[] = {"watching", "paused", "finished"};
    return {
        {"id",             s.id},
        {"title",          s.title},
        {"service",        s.service},
        {"season",         s.season},
        {"episode",        s.episode},
        {"imdb_id",        s.imdb_id},
        {"total_episodes", s.total_episodes},
        {"status",         status_str[static_cast<int>(s.status)]},
        {"notes",          s.notes},
    };
}

static json movie_to_json(const Movie& m) {
    static const char* status_str[] = {"want_to_watch", "watched"};
    return {
        {"id",      m.id},
        {"title",   m.title},
        {"imdb_id", m.imdb_id},
        {"status",  status_str[static_cast<int>(m.status)]},
        {"notes",   m.notes},
    };
}

static ShowStatus show_status_from(const std::string& s) {
    if (s == "paused")   return ShowStatus::Paused;
    if (s == "finished") return ShowStatus::Finished;
    return ShowStatus::Watching;
}

static MovieStatus movie_status_from(const std::string& s) {
    if (s == "watched") return MovieStatus::Watched;
    return MovieStatus::WantToWatch;
}

static void json_response(httplib::Response& res, const json& body, int code = 200) {
    res.status = code;
    res.set_content(body.dump(), "application/json");
}

static void error_response(httplib::Response& res, const std::string& msg, int code = 400) {
    json_response(res, {{"error", msg}}, code);
}

// ---------- Impl ----------------------------------------------------------

struct Server::Impl {
    Database         db;
    httplib::Server  srv;
    std::string      web_root;
    int              port;

    explicit Impl(const std::string& db_path, const std::string& wr, int p)
        : db(db_path), web_root(wr), port(p)
    {}
};

// ---------- Server --------------------------------------------------------

Server::Server(const std::string& db_path, const std::string& web_root, int port)
    : impl_(new Impl(db_path, web_root, port))
{
    auto& srv = impl_->srv;
    auto& db  = impl_->db;

    // --- Static web assets ------------------------------------------------
    srv.set_mount_point("/", web_root);

    // --- GET /api/shows ---------------------------------------------------
    srv.Get("/api/shows", [&](const httplib::Request&, httplib::Response& res) {
        try {
            json arr = json::array();
            for (const auto& s : db.all_shows()) arr.push_back(show_to_json(s));
            json_response(res, arr);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- POST /api/shows --------------------------------------------------
    srv.Post("/api/shows", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            Show s;
            s.title          = j.value("title", "");
            s.service        = j.value("service", "");
            s.season         = j.value("season", 1);
            s.episode        = j.value("episode", 1);
            s.imdb_id        = j.value("imdb_id", "");
            s.total_episodes = j.value("total_episodes", 0);
            s.status         = show_status_from(j.value("status", "watching"));
            s.notes          = j.value("notes", "");
            if (s.title.empty()) { error_response(res, "title required"); return; }
            s.id = db.add_show(s);
            json_response(res, show_to_json(s), 201);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- PUT /api/shows/:id -----------------------------------------------
    srv.Put(R"(/api/shows/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            Show s = db.get_show(id);
            auto j = json::parse(req.body);
            if (j.contains("title"))          s.title          = j["title"];
            if (j.contains("service"))        s.service        = j["service"];
            if (j.contains("season"))         s.season         = j["season"];
            if (j.contains("episode"))        s.episode        = j["episode"];
            if (j.contains("imdb_id"))        s.imdb_id        = j["imdb_id"];
            if (j.contains("total_episodes")) s.total_episodes = j["total_episodes"];
            if (j.contains("status"))         s.status         = show_status_from(j["status"]);
            if (j.contains("notes"))          s.notes          = j["notes"];
            db.update_show(s);
            json_response(res, show_to_json(s));
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- DELETE /api/shows/:id --------------------------------------------
    srv.Delete(R"(/api/shows/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            db.delete_show(id);
            json_response(res, {{"ok", true}});
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/movies --------------------------------------------------
    srv.Get("/api/movies", [&](const httplib::Request&, httplib::Response& res) {
        try {
            json arr = json::array();
            for (const auto& m : db.all_movies()) arr.push_back(movie_to_json(m));
            json_response(res, arr);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- POST /api/movies -------------------------------------------------
    srv.Post("/api/movies", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            Movie m;
            m.title   = j.value("title", "");
            m.imdb_id = j.value("imdb_id", "");
            m.status  = movie_status_from(j.value("status", "want_to_watch"));
            m.notes   = j.value("notes", "");
            if (m.title.empty()) { error_response(res, "title required"); return; }
            m.id = db.add_movie(m);
            json_response(res, movie_to_json(m), 201);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- PUT /api/movies/:id ----------------------------------------------
    srv.Put(R"(/api/movies/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            Movie m = db.get_movie(id);
            auto j = json::parse(req.body);
            if (j.contains("title"))   m.title   = j["title"];
            if (j.contains("imdb_id")) m.imdb_id = j["imdb_id"];
            if (j.contains("status"))  m.status  = movie_status_from(j["status"]);
            if (j.contains("notes"))   m.notes   = j["notes"];
            db.update_movie(m);
            json_response(res, movie_to_json(m));
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- DELETE /api/movies/:id -------------------------------------------
    srv.Delete(R"(/api/movies/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            db.delete_movie(id);
            json_response(res, {{"ok", true}});
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });
}

Server::~Server() {
    delete impl_;
}

void Server::run() {
    // Stop cleanly on SIGINT/SIGTERM
    static Server::Impl* g_impl = impl_;
    signal(SIGINT,  [](int) { g_impl->srv.stop(); });
    signal(SIGTERM, [](int) { g_impl->srv.stop(); });

    if (!impl_->srv.listen("0.0.0.0", impl_->port)) {
        throw std::runtime_error("Failed to bind port " + std::to_string(impl_->port));
    }
}

} // namespace FlickImp

// SN: 00001
