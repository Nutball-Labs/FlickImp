// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "server.hpp"
#include "../lib/database.hpp"
#include "../lib/models.hpp"
#include "../lib/scraper.hpp"
#include <httplib.h>
#include <json.hpp>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>

using json = nlohmann::json;

namespace FlickImp {

static std::string ep_label(int season, int episode) {
    std::ostringstream ss;
    ss << "s" << std::setw(3) << std::setfill('0') << season
       << "-e" << std::setw(3) << std::setfill('0') << episode;
    return ss.str();
}

// Extract bare tt-number from either "tt0306414" or a full IMDB URL.
static std::string clean_imdb_id(const std::string& raw) {
    auto pos = raw.find("tt");
    if (pos == std::string::npos) return raw;
    std::string id = raw.substr(pos);
    // Trim anything after the tt-number (slash, query string, etc.)
    auto end = id.find_first_not_of("tt0123456789");
    if (end != std::string::npos) id = id.substr(0, end);
    return id;
}

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
        {"tmdb_id",        s.tmdb_id},
        {"total_episodes", s.total_episodes},
        {"status",         status_str[static_cast<int>(s.status)]},
        {"notes",          s.notes},
        {"thumbnail_url",  s.thumbnail_url},
    };
}

static json movie_to_json(const Movie& m) {
    static const char* status_str[] = {"want_to_watch", "watched"};
    return {
        {"id",            m.id},
        {"title",         m.title},
        {"imdb_id",       m.imdb_id},
        {"release_date",  m.release_date},
        {"thumbnail_url", m.thumbnail_url},
        {"status",        status_str[static_cast<int>(m.status)]},
        {"notes",         m.notes},
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

static json search_result_to_json(const Scraper::SearchResult& r) {
    return {
        {"tmdb_id",    r.tmdb_id},
        {"title",      r.title},
        {"year",       r.year},
        {"poster_url", r.poster_url},
    };
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
    if (!srv.set_mount_point("/", web_root))
        throw std::runtime_error("Web root not found or inaccessible: " + web_root);

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
            s.imdb_id        = clean_imdb_id(j.value("imdb_id", ""));
            s.tmdb_id        = j.value("tmdb_id", 0);
            std::cerr << "    [server] POST /api/shows: title=\"" << s.title
                      << "\" tmdb_id=" << s.tmdb_id
                      << " imdb_id=\"" << s.imdb_id << "\"\n";
            if (s.imdb_id.empty() && s.tmdb_id != 0)
                s.imdb_id = Scraper::fetch_imdb_id(s.tmdb_id);
            s.total_episodes = j.value("total_episodes", 0);
            s.status         = show_status_from(j.value("status", "watching"));
            s.notes          = j.value("notes", "");
            if (s.title.empty()) { error_response(res, "title required"); return; }
            std::string req_thumb = j.value("thumbnail_url", "");
            if (!req_thumb.empty())
                s.thumbnail_url = req_thumb;
            else if (!s.imdb_id.empty())
                s.thumbnail_url = Scraper::fetch_poster_url(s.imdb_id);
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
            if (j.contains("imdb_id")) {
                s.imdb_id = clean_imdb_id(j["imdb_id"]);
                if (!s.imdb_id.empty() && s.thumbnail_url.empty())
                    s.thumbnail_url = Scraper::fetch_poster_url(s.imdb_id);
            }
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
            m.imdb_id = clean_imdb_id(j.value("imdb_id", ""));
            m.tmdb_id = j.value("tmdb_id", 0);
            m.status  = movie_status_from(j.value("status", "want_to_watch"));
            m.notes   = j.value("notes", "");
            if (m.title.empty()) { error_response(res, "title required"); return; }
            if (m.imdb_id.empty() && m.tmdb_id != 0)
                m.imdb_id = Scraper::fetch_movie_imdb_id(m.tmdb_id);
            std::string req_thumb = j.value("thumbnail_url", "");
            if (!req_thumb.empty())
                m.thumbnail_url = req_thumb;
            {
                std::optional<Scraper::MovieInfo> info;
                if (m.tmdb_id > 0)
                    info = Scraper::fetch_movie_info_by_tmdb_id(m.tmdb_id);
                else if (!m.imdb_id.empty())
                    info = Scraper::fetch_movie_info(m.imdb_id);
                if (info) {
                    if (!info->release_date.empty()) m.release_date = info->release_date;
                    if (m.thumbnail_url.empty() && !info->image_url.empty())
                        m.thumbnail_url = info->image_url;
                }
            }
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
            if (j.contains("status"))  m.status  = movie_status_from(j["status"]);
            if (j.contains("notes"))   m.notes   = j["notes"];
            if (j.contains("imdb_id")) {
                m.imdb_id = clean_imdb_id(j["imdb_id"]);
                // Re-fetch date if IMDB ID changed or date is missing
                if (!m.imdb_id.empty() && m.release_date.empty()) {
                    auto info = Scraper::fetch_movie_info(m.imdb_id);
                    if (info && !info->release_date.empty())
                        m.release_date = info->release_date;
                }
            }
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

    // --- GET /api/search/shows?q=<query> ------------------------------------
    srv.Get("/api/search/shows", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string q = req.has_param("q") ? req.get_param_value("q") : "";
            if (q.empty()) { error_response(res, "q is required"); return; }
            json arr = json::array();
            for (const auto& r : Scraper::search_shows(q))
                arr.push_back(search_result_to_json(r));
            json_response(res, arr);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/search/movies?q=<query> -----------------------------------
    srv.Get("/api/search/movies", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string q = req.has_param("q") ? req.get_param_value("q") : "";
            if (q.empty()) { error_response(res, "q is required"); return; }
            json arr = json::array();
            for (const auto& r : Scraper::search_movies(q))
                arr.push_back(search_result_to_json(r));
            json_response(res, arr);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/shows/:id/seasons ---------------------------------------
    // Returns season list with per-season episode and watched counts.
    srv.Get(R"(/api/shows/(\d+)/seasons)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            Show show = db.get_show(id);
            if (show.imdb_id.empty() && show.tmdb_id == 0) {
                error_response(res, "No IMDB ID or TMDB ID set for this show", 422);
                return;
            }
            if (show.tmdb_id == 0) {
                auto info = Scraper::fetch_show_info(show.imdb_id);
                if (!info || info->tmdb_id == 0) {
                    error_response(res, "Could not find show on TMDB", 422);
                    return;
                }
                show.tmdb_id = info->tmdb_id;
                if (show.thumbnail_url.empty()) show.thumbnail_url = info->image_url;
                db.update_show(show);
            }
            auto seasons      = Scraper::fetch_show_seasons(show.tmdb_id);
            auto watched_map  = db.get_watched_counts(id);
            json arr = json::array();
            for (const auto& s : seasons) {
                int wc = 0;
                auto it = watched_map.find(s.season_number);
                if (it != watched_map.end()) wc = it->second;
                arr.push_back({
                    {"season",          s.season_number},
                    {"total_episodes",  s.episode_count},
                    {"watched_count",   wc},
                });
            }
            json_response(res, arr);
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/shows/:id/episodes?season=N ----------------------------
    // Fetches episode list from TMDB and merges local watched status.
    srv.Get(R"(/api/shows/(\d+)/episodes)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            Show show = db.get_show(id);
            if (show.imdb_id.empty() && show.tmdb_id == 0) {
                error_response(res, "No IMDB ID or TMDB ID set for this show", 422);
                return;
            }
            int season = show.season;
            if (req.has_param("season"))
                season = std::stoi(req.get_param_value("season"));

            // Ensure we have a TMDB ID (cached after first lookup)
            if (show.tmdb_id == 0) {
                auto info = Scraper::fetch_show_info(show.imdb_id);
                if (!info || info->tmdb_id == 0) {
                    error_response(res, "Could not find show on TMDB", 422);
                    return;
                }
                show.tmdb_id = info->tmdb_id;
                if (show.thumbnail_url.empty()) show.thumbnail_url = info->image_url;
                db.update_show(show);
            }

            auto entries = Scraper::fetch_season_episodes(show.tmdb_id, season);
            auto watched = db.get_watched_episodes(id, season);

            json arr = json::array();
            for (const auto& ep : entries) {
                arr.push_back({
                    {"season",       ep.season},
                    {"episode",      ep.episode},
                    {"title",        ep.title},
                    {"episode_url",  ep.episode_url},
                    {"air_date",     ep.air_date},
                    {"watched",      watched.count(ep.episode) > 0},
                });
            }
            json_response(res, arr);
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- PUT /api/shows/:id/episodes/:season/:episode/watched ------------
    // Toggles watched state; advances show's last-watched position if ahead.
    srv.Put(R"(/api/shows/(\d+)/episodes/(\d+)/(\d+)/watched)",
        [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int show_id = std::stoi(req.matches[1]);
            int season  = std::stoi(req.matches[2]);
            int episode = std::stoi(req.matches[3]);
            auto j = json::parse(req.body);
            bool watched = j.value("watched", true);

            db.set_episode_watched(show_id, season, episode, watched);

            Show show = db.get_show(show_id);
            if (watched) {
                if (season > show.season ||
                    (season == show.season && episode > show.episode)) {
                    show.season  = season;
                    show.episode = episode;
                    db.update_show(show);
                }
            }
            json_response(res, {{"ok", true}, {"show", show_to_json(show)}});
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/check/stream -----------------------------------------------
    // SSE endpoint: runs the same logic as --check and streams each output
    // line as a server-sent event so the browser sees live progress.
    srv.Get("/api/check/stream", [&](const httplib::Request&, httplib::Response& res) {
        res.set_header("Cache-Control", "no-cache");
        res.set_header("X-Accel-Buffering", "no");
        res.set_chunked_content_provider("text/event-stream",
            [&db](size_t off, httplib::DataSink& sink) -> bool {
                if (off > 0) { sink.done(); return true; }
                auto send = [&](const std::string& line) {
                    std::string msg = "data: " + line + "\n\n";
                    sink.write(msg.c_str(), msg.size());
                };
                try {
                    // --- Shows ---
                    auto shows = db.all_shows();
                    int active = 0;
                    for (const auto& s : shows)
                        if (s.status != ShowStatus::Finished && !s.imdb_id.empty()) ++active;

                    send("Checking " + std::to_string(active)
                        + " active show(s) for new episodes...");
                    send("");

                    int checked = 0, found_new = 0;
                    bool first = true;

                    for (auto& show : shows) {
                        if (show.status == ShowStatus::Finished) continue;
                        if (show.imdb_id.empty()) {
                            send("  " + show.title + " -- no IMDB ID, skipping");
                            continue;
                        }
                        if (!first)
                            std::this_thread::sleep_for(std::chrono::milliseconds(600));
                        first = false;

                        send("  " + show.title + " (" + show.imdb_id + ")...");

                        auto info = Scraper::fetch_show_info(show.imdb_id);
                        if (!info) { send("    FAILED"); continue; }
                        ++checked;

                        bool changed = false;
                        if (show.tmdb_id == 0 && info->tmdb_id > 0)
                            { show.tmdb_id = info->tmdb_id; changed = true; }
                        if (info->total_episodes > 0 &&
                                info->total_episodes != show.total_episodes)
                            { show.total_episodes = info->total_episodes; changed = true; }
                        if (show.thumbnail_url.empty() && !info->image_url.empty())
                            { show.thumbnail_url = info->image_url; changed = true; }
                        if (changed) db.update_show(show);

                        const auto& la = info->latest_aired;
                        if (la.season == 0) {
                            send("    no aired-episode data found");
                            continue;
                        }
                        bool ahead = (la.season > show.season) ||
                                     (la.season == show.season && la.episode > show.episode);
                        if (ahead) {
                            ++found_new;
                            std::string detail = "    You're on "
                                + ep_label(show.season, show.episode)
                                + "  |  Latest aired: "
                                + ep_label(la.season, la.episode);
                            if (!la.title.empty()) detail += " \"" + la.title + "\"";
                            if (!la.air_date.empty()) detail += "  [" + la.air_date + "]";
                            send("    -> NEW");
                            send(detail);
                        } else {
                            send("    -> all caught up at "
                                + ep_label(show.season, show.episode));
                        }
                    }

                    send("");
                    send(std::to_string(checked) + " show(s) checked, "
                        + std::to_string(found_new) + " with new episode(s).");

                    // --- Movies: back-fill missing release dates ---
                    auto movies = db.all_movies();
                    int movies_to_check = 0;
                    for (const auto& m : movies)
                        if (m.release_date.empty() &&
                                (!m.imdb_id.empty() || m.tmdb_id != 0))
                            ++movies_to_check;

                    if (movies_to_check > 0) {
                        send("");
                        send("Checking " + std::to_string(movies_to_check)
                            + " movie(s) for missing release dates...");
                        send("");

                        int movies_updated = 0;
                        first = true;

                        for (auto& movie : movies) {
                            if (!movie.release_date.empty()) continue;
                            if (movie.imdb_id.empty() && movie.tmdb_id == 0) continue;
                            if (!first)
                                std::this_thread::sleep_for(std::chrono::milliseconds(600));
                            first = false;

                            send("  " + movie.title + "...");

                            std::optional<Scraper::MovieInfo> info;
                            if (movie.tmdb_id > 0)
                                info = Scraper::fetch_movie_info_by_tmdb_id(movie.tmdb_id);
                            else
                                info = Scraper::fetch_movie_info(movie.imdb_id);

                            if (!info || info->release_date.empty()) {
                                send("    -> no release date found");
                                continue;
                            }
                            movie.release_date = info->release_date;
                            if (movie.imdb_id.empty() && movie.tmdb_id > 0) {
                                std::string iid =
                                    Scraper::fetch_movie_imdb_id(movie.tmdb_id);
                                if (!iid.empty()) movie.imdb_id = iid;
                            }
                            if (movie.thumbnail_url.empty() && !info->image_url.empty())
                                movie.thumbnail_url = info->image_url;
                            db.update_movie(movie);
                            ++movies_updated;
                            send("    -> " + info->release_date);
                        }

                        send("");
                        send(std::to_string(movies_updated)
                            + " movie(s) updated with release dates.");
                    }
                } catch (const std::exception& e) {
                    send("Error: " + std::string(e.what()));
                }
                std::string done_evt = "event: done\ndata: \n\n";
                sink.write(done_evt.c_str(), done_evt.size());
                sink.done();
                return true;
            }
        );
    });
}

Server::~Server() {
    delete impl_;
}

void Server::run() {
    // Stop cleanly on SIGINT/SIGTERM (SIGTERM is POSIX-only; not available on Windows)
    static Server::Impl* g_impl = impl_;
    signal(SIGINT,  [](int) { g_impl->srv.stop(); });
#ifndef _WIN32
    signal(SIGTERM, [](int) { g_impl->srv.stop(); });
#endif

    if (!impl_->srv.listen("0.0.0.0", impl_->port)) {
        throw std::runtime_error("Failed to bind port " + std::to_string(impl_->port));
    }
}

} // namespace FlickImp

// SN: 00003
