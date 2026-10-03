// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "server.hpp"
#include "../lib/backup.hpp"
#include "../lib/database.hpp"
#include "../lib/models.hpp"
#include "../lib/pin.hpp"
#include "../lib/scraper.hpp"
#include "../lib/settings.hpp"
#include "../lib/version.hpp"
#include <httplib.h>
#include <json.hpp>
#include <chrono>
#include <csignal>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>

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
        {"total_episodes",  s.total_episodes},
        {"latest_season",   s.latest_season},
        {"latest_episode",  s.latest_episode},
        {"season_episodes", s.season_episodes},
        {"next_season",         s.next_season},
        {"next_episode",        s.next_episode},
        {"next_episode_title",  s.next_episode_title},
        {"status",         status_str[static_cast<int>(s.status)]},
        {"queue",          s.queue == ShowQueue::Queued ? "queued" : "current"},
        {"queue_id",       s.queue_id},
        {"group_id",       s.group_id},
        {"group_order",    s.group_order},
        {"sort_order",     s.sort_order},
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
        {"tmdb_id",       m.tmdb_id},
        {"release_date",  m.release_date},
        {"thumbnail_url", m.thumbnail_url},
        {"status",        status_str[static_cast<int>(m.status)]},
        {"queue_id",      m.queue_id},
        {"sort_order",    m.sort_order},
        {"notes",         m.notes},
    };
}

static ShowStatus show_status_from(const std::string& s) {
    if (s == "paused")   return ShowStatus::Paused;
    if (s == "finished") return ShowStatus::Finished;
    return ShowStatus::Watching;
}

static ShowQueue show_queue_from(const std::string& s) {
    if (s == "queued") return ShowQueue::Queued;
    return ShowQueue::Current;
}

static MovieStatus movie_status_from(const std::string& s) {
    if (s == "watched") return MovieStatus::Watched;
    return MovieStatus::WantToWatch;
}

static json cast_member_to_json(const CastMember& cm) {
    return {
        {"name",           cm.name},
        {"character",      cm.character},
        {"imdb_id",        cm.imdb_id},
        {"profile_url",    cm.profile_url},
        {"tmdb_person_id", cm.tmdb_person_id}
    };
}

static json queue_to_json(const Queue& q) {
    return {
        {"id",         q.id},
        {"name",       q.name},
        {"sort_order", q.sort_order},
        // Only whether a PIN exists — the PIN itself never goes to the browser
        {"has_pin",    !q.pin.empty()},
    };
}

static json search_result_to_json(const Scraper::SearchResult& r) {
    return {
        {"tmdb_id",    r.tmdb_id},
        {"title",      r.title},
        {"year",           r.year},
        {"first_air_date", r.first_air_date},
        {"poster_url",     r.poster_url},
    };
}

// Today's date as "YYYY-MM-DD" (UTC), for comparing with TMDB air dates
static std::string today_iso() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", &tm);
    return buf;
}

static void json_response(httplib::Response& res, const json& body, int code = 200) {
    res.status = code;
    res.set_content(body.dump(), "application/json");
}

static void error_response(httplib::Response& res, const std::string& msg, int code = 400) {
    json_response(res, {{"error", msg}}, code);
}

// ---------- Show group helpers --------------------------------------------
// File-scope functions (not constructor-local lambdas): route handlers run
// long after the Server constructor returns, so anything they call must not
// live on the constructor's stack.

// Default group name: first member's title without a trailing " (1963)"
static std::string default_group_name(const std::string& title) {
    static const std::regex year_suffix(R"(\s*\(\d{4}\)\s*$)");
    std::string n = std::regex_replace(title, year_suffix, "");
    return n.empty() ? title : n;
}

// Move a show into a group at the end, matching the group's queue
static void join_group(Database& db, Show s, int group_id, const Show& anchor) {
    if (s.queue != anchor.queue || s.queue_id != anchor.queue_id) {
        s.queue    = anchor.queue;
        s.queue_id = anchor.queue_id;
        db.update_show(s);
    }
    db.set_show_group(s.id, group_id, db.max_group_order(group_id) + 1);
}

// Any current member of the group (used for its queue), if it has one
static std::optional<Show> group_anchor(Database& db, int group_id) {
    for (const auto& s : db.all_shows())
        if (s.group_id == group_id) return s;
    return std::nullopt;
}

// Members of a group in group_order, with TMDB ids resolved
static std::vector<Show> group_members(Database& db, int gid) {
    std::vector<Show> members;
    for (auto s : db.all_shows()) {
        if (s.group_id != gid) continue;
        if (s.tmdb_id == 0 && !s.imdb_id.empty()) {
            if (auto info = Scraper::fetch_show_info(s.imdb_id)) {
                s.tmdb_id = info->tmdb_id;
                db.update_show(s);
            }
        }
        members.push_back(s);
    }
    std::sort(members.begin(), members.end(),
              [](const Show& a, const Show& b) { return a.group_order < b.group_order; });
    return members;
}

// Cached episode list for one season; fetched from TMDB when stale.
// last_season: the show's newest season, which is never marked final.
static std::vector<std::tuple<int, std::string, std::string>>
season_eps(Database& db, int tmdb_id, int season, bool last_season) {
    const std::string today = today_iso();
    if (!db.season_list_fresh(tmdb_id, season, today)) {
        auto fetched = Scraper::fetch_season_episodes(tmdb_id, season);
        if (!fetched.empty()) {
            std::vector<std::tuple<int, std::string, std::string>> eps;
            bool all_aired = true;
            for (const auto& e : fetched) {
                eps.emplace_back(e.episode, e.title, e.air_date);
                if (e.air_date.empty() || e.air_date > today) all_aired = false;
            }
            db.store_season_list(tmdb_id, season, eps, today, all_aired && !last_season);
        }
    }
    return db.season_list(tmdb_id, season);
}

struct GroupEp {
    int         show_id{0};
    int         season{0};
    int         episode{0};
    std::string title;
    std::string air_date;
    int         season_total{0};
};

static json group_ep_json(const GroupEp& e, const Show& s, bool watched) {
    return json{
        {"show_id",      e.show_id},
        {"show_title",   s.title},
        {"season",       e.season},
        {"episode",      e.episode},
        {"title",        e.title},
        {"air_date",     e.air_date},
        {"season_total", e.season_total},
        {"watched",      watched},
        {"episode_url",  "https://www.themoviedb.org/tv/" + std::to_string(s.tmdb_id)
                         + "/season/" + std::to_string(e.season)
                         + "/episode/" + std::to_string(e.episode)},
    };
}

// Every episode of every member (fills the cache; slow only the first time)
static std::vector<GroupEp> all_group_eps(Database& db, const std::vector<Show>& members) {
    std::vector<GroupEp> all;
    for (const auto& s : members) {
        if (s.tmdb_id == 0) continue;
        auto seasons = Scraper::fetch_show_seasons(s.tmdb_id);
        int last = seasons.empty() ? 0 : seasons.back().season_number;
        for (const auto& se : seasons) {
            auto eps = season_eps(db, s.tmdb_id, se.season_number, se.season_number == last);
            int total = static_cast<int>(eps.size());
            for (const auto& [ep, title, air] : eps)
                all.push_back({s.id, se.season_number, ep, title, air, total});
        }
    }
    return all;
}

// Watched lookup cache for one request: (show, season) -> watched episodes
struct WatchedCache {
    Database& db;
    std::map<std::pair<int,int>, std::set<int>> m;
    bool operator()(int show, int season, int ep) {
        auto key = std::make_pair(show, season);
        auto it = m.find(key);
        if (it == m.end()) it = m.emplace(key, db.get_watched_episodes(show, season)).first;
        return it->second.count(ep) > 0;
    }
};

// ---------- Impl ----------------------------------------------------------

struct Server::Impl {
    Database         db;
    httplib::Server  srv;
    std::string      web_root;
    int              port;
    int              cli_port;
    Config           file_cfg;

    explicit Impl(const ServerOptions& o)
        : db(o.db_path), web_root(o.web_root), port(o.port),
          cli_port(o.cli_port), file_cfg(o.file_cfg)
    {}

    // Queue PIN unlocks: token -> queue id, in memory only (a restart re-locks).
    // The browser sends its tokens in the X-Queue-Tokens header (comma list).
    std::mutex                 tok_mutex;
    std::map<std::string, int> tokens;

    std::string issue_token(int queue_id) {
        std::string t = Pin::random_hex(16);
        std::lock_guard<std::mutex> lock(tok_mutex);
        tokens[t] = queue_id;
        return t;
    }

    std::set<int> unlocked(const httplib::Request& req) {
        std::set<int> ids;
        std::string hdr = req.get_header_value("X-Queue-Tokens");
        std::lock_guard<std::mutex> lock(tok_mutex);
        std::size_t start = 0;
        while (start <= hdr.size()) {
            std::size_t end = hdr.find(',', start);
            if (end == std::string::npos) end = hdr.size();
            auto it = tokens.find(hdr.substr(start, end - start));
            if (it != tokens.end()) ids.insert(it->second);
            start = end + 1;
        }
        return ids;
    }

    // Queues whose content this request may not see
    std::set<int> hidden_queues(const httplib::Request& req) {
        auto open = unlocked(req);
        std::set<int> hidden;
        for (const auto& q : db.all_queues())
            if (!q.pin.empty() && !open.count(q.id)) hidden.insert(q.id);
        return hidden;
    }

    // Changing a PIN or deleting a PIN-protected queue needs an unlock token
    // for it, or its current PIN in the body as "current_pin"
    bool may_manage(const httplib::Request& req, const Queue& q, const json& body) {
        if (q.pin.empty() || unlocked(req).count(q.id)) return true;
        return body.contains("current_pin") && body["current_pin"].is_string() &&
               Pin::verify(body["current_pin"].get<std::string>(), q.pin);
    }

    Settings::Effective settings() { return Settings::resolve(file_cfg, db, cli_port); }

    // Re-read settings and push TMDB credentials to the scraper
    Settings::Effective apply_settings() {
        auto eff = settings();
        Scraper::init(eff.tmdb_api_key, eff.tmdb_bearer_token);
        return eff;
    }
};

// Settings as sent to the browser. Secrets are never returned in full —
// only whether they are set, their last 4 characters, and where they came from.
static json secret_to_json(const std::string& value, const std::string& src) {
    std::string hint;
    if (value.size() > 8) hint = value.substr(value.size() - 4);
    return {{"set", !value.empty()}, {"hint", hint}, {"source", src}};
}

static json settings_to_json(const Settings::Effective& e, int running_port) {
    return {
        {"tmdb_bearer_token", secret_to_json(e.tmdb_bearer_token, e.bearer_src)},
        {"tmdb_api_key",      secret_to_json(e.tmdb_api_key,      e.api_key_src)},
        {"port", {
            {"value",   e.port},
            {"source",  e.port_src},
            {"running", running_port},
            {"locked",  e.port_src == Settings::SRC_CLI},
        }},
    };
}

// ---------- Server --------------------------------------------------------

Server::Server(const ServerOptions& opts)
    : impl_(new Impl(opts))
{
    auto& srv  = impl_->srv;
    auto& db   = impl_->db;
    auto& impl = *impl_;
    const auto& web_root = opts.web_root;

    // Drop groups left empty or with one member (e.g. by an interrupted link)
    db.prune_groups();

    // --- Static web assets ------------------------------------------------
    if (!srv.set_mount_point("/", web_root))
        throw std::runtime_error("Web root not found or inaccessible: " + web_root);

    // --- GET /api/settings -----------------------------------------------
    srv.Get("/api/settings", [&](const httplib::Request&, httplib::Response& res) {
        try {
            json_response(res, settings_to_json(impl.settings(), impl.port));
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- PUT /api/settings -----------------------------------------------
    // Body fields are all optional. A string sets the DB override; an empty
    // string clears it (falls back to fi_config.json). port: 0 or null clears.
    srv.Put("/api/settings", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            for (const char* key : {Settings::KEY_BEARER, Settings::KEY_API_KEY}) {
                if (j.contains(key) && j[key].is_string())
                    db.set_setting(key, j[key].get<std::string>());
            }
            if (j.contains(Settings::KEY_PORT)) {
                const auto& p = j[Settings::KEY_PORT];
                int port = p.is_number_integer() ? p.get<int>() : 0;
                if (port < 0 || port > 65535) {
                    error_response(res, "Port must be between 1 and 65535");
                    return;
                }
                db.set_setting(Settings::KEY_PORT, port > 0 ? std::to_string(port) : "");
            }
            json_response(res, settings_to_json(impl.apply_settings(), impl.port));
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- POST /api/settings/test-tmdb ------------------------------------
    // Tests the credentials in the body; omitted fields use the saved values.
    srv.Post("/api/settings/test-tmdb", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto eff = impl.settings();
            auto j   = req.body.empty() ? json::object() : json::parse(req.body);
            std::string bearer = eff.tmdb_bearer_token, key = eff.tmdb_api_key;
            if (j.contains(Settings::KEY_BEARER)  && j[Settings::KEY_BEARER].is_string())
                bearer = j[Settings::KEY_BEARER].get<std::string>();
            if (j.contains(Settings::KEY_API_KEY) && j[Settings::KEY_API_KEY].is_string())
                key = j[Settings::KEY_API_KEY].get<std::string>();
            std::string err = Scraper::test_credentials(key, bearer);
            json_response(res, {{"ok", err.empty()},
                                {"message", err.empty() ? "TMDB accepted the credentials" : err}});
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/backup?caches=1 ----------------------------------------
    srv.Get("/api/backup", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            bool caches = req.has_param("caches") && req.get_param_value("caches") == "1";
            // PIN-protected queues this request hasn't unlocked are always left
            // out; ?exclude=2,5 leaves out more (e.g. unlocked but not wanted)
            auto exclude = impl.hidden_queues(req);
            if (req.has_param("exclude")) {
                std::string list = req.get_param_value("exclude");
                std::size_t start = 0;
                while (start < list.size()) {
                    std::size_t end = list.find(',', start);
                    if (end == std::string::npos) end = list.size();
                    try { exclude.insert(std::stoi(list.substr(start, end - start))); } catch (...) {}
                    start = end + 1;
                }
            }
            json_response(res, Backup::export_all(db, impl.settings(), caches, exclude));
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- POST /api/restore?mode=replace|merge ----------------------------
    // Body is the (already decompressed) backup JSON. The current DB is
    // first copied to <db>.pre-restore so a bad restore can be undone by hand.
    srv.Post("/api/restore", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string m = req.has_param("mode") ? req.get_param_value("mode") : "";
            if (m != "replace" && m != "merge") {
                error_response(res, "mode must be replace or merge");
                return;
            }
            if (m == "replace" && !impl.hidden_queues(req).empty()) {
                error_response(res, "Unlock every PIN-protected queue before a Replace restore — it deletes them", 403);
                return;
            }
            json backup;
            try { backup = json::parse(req.body); }
            catch (const std::exception&) { error_response(res, "Backup is not valid JSON"); return; }

            std::string snapshot = db.path().empty() ? "" : db.path() + ".pre-restore";
            if (!snapshot.empty()) db.snapshot_to(snapshot);

            auto result = Backup::restore(db, backup,
                m == "replace" ? Backup::Mode::Replace : Backup::Mode::Merge,
                impl.settings());
            impl.apply_settings();
            result["snapshot"] = snapshot;
            std::cout << "[restore] " << m << " restore complete; previous DB saved to "
                      << snapshot << "\n";
            json_response(res, result);
        } catch (const DbError& e) {
            error_response(res, e.what(), 500);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 400);
        }
    });

    // --- GET /api/about ---------------------------------------------------
    srv.Get("/api/about", [](const httplib::Request&, httplib::Response& res) {
        json_response(res, {
            {"name",      APP_NAME},
            {"version",   APP_VERSION},
            {"copyright", "Copyright © 2026 Nutball Labs / Stephen Berg"},
            {"license",   "GNU General Public License v3 or later"},
            {"repo",      "https://github.com/Nutball-Labs/FlickImp"}
        });
    });

    // --- Show groups -------------------------------------------------------
    // Several shows shown as one card (Doctor Who 1963 / 2005 / 2023). Members
    // keep their own progress; the group only carries a name. Members share
    // one queue: whoever joins is moved into the queue of the existing members.

    // --- Group episode views ------------------------------------------------
    // A group can be browsed by season (every member's seasons, in premiere
    // order) or by year (every member's episodes, in air-date order). Episode
    // lists come from the tmdb_episodes cache; per-episode IMDB IDs are NOT
    // prefetched here (see GET .../imdb below).

    // --- GET /api/groups/:id/seasons -----------------------------------------
    srv.Get(R"(/api/groups/(\d+)/seasons)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int gid = std::stoi(req.matches[1]);
            auto members = group_members(db, gid);
            struct Row { json j; std::string air; int order; int season; };
            std::vector<Row> rows;
            for (const auto& s : members) {
                if (s.tmdb_id == 0) continue;
                auto counts = db.get_watched_counts(s.id);
                for (const auto& se : Scraper::fetch_show_seasons(s.tmdb_id)) {
                    auto it = counts.find(se.season_number);
                    rows.push_back({json{
                        {"show_id",        s.id},
                        {"show_title",     s.title},
                        {"season",         se.season_number},
                        {"total_episodes", se.episode_count},
                        {"watched_count",  it != counts.end() ? it->second : 0},
                        {"air_date",       se.air_date},
                    }, se.air_date, s.group_order, se.season_number});
                }
            }
            // Premiere order; undated seasons last, then member order
            std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
                if (a.air.empty() != b.air.empty()) return b.air.empty();
                if (a.air != b.air) return a.air < b.air;
                if (a.order != b.order) return a.order < b.order;
                return a.season < b.season;
            });
            json arr = json::array();
            for (auto& r : rows) arr.push_back(std::move(r.j));
            json_response(res, arr);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/groups/:id/years ---------------------------------------------
    // year 0 = episodes with no air date yet
    srv.Get(R"(/api/groups/(\d+)/years)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto members = group_members(db, std::stoi(req.matches[1]));
            WatchedCache watched{db, {}};
            std::map<int, std::pair<int,int>> years;   // year -> (total, watched)
            for (const auto& e : all_group_eps(db, members)) {
                int y = e.air_date.size() >= 4 ? std::atoi(e.air_date.substr(0, 4).c_str()) : 0;
                auto& [total, w] = years[y];
                ++total;
                if (watched(e.show_id, e.season, e.episode)) ++w;
            }
            json arr = json::array();
            for (const auto& [y, tw] : years)
                arr.push_back({{"year", y}, {"total_episodes", tw.first}, {"watched_count", tw.second}});
            json_response(res, arr);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/groups/:id/episodes?show_id=&season=  |  ?year= ---------------
    srv.Get(R"(/api/groups/(\d+)/episodes)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto members = group_members(db, std::stoi(req.matches[1]));
            std::map<int, Show> by_id;
            for (const auto& s : members) by_id[s.id] = s;
            WatchedCache watched{db, {}};
            json arr = json::array();

            if (req.has_param("year")) {
                std::string y = req.get_param_value("year");
                std::vector<GroupEp> eps;
                for (const auto& e : all_group_eps(db, members)) {
                    bool undated = e.air_date.size() < 4;
                    if (y == "0" ? undated : (!undated && e.air_date.compare(0, 4, y) == 0))
                        eps.push_back(e);
                }
                std::map<int, int> order;
                for (const auto& s : members) order[s.id] = s.group_order;
                std::stable_sort(eps.begin(), eps.end(), [&](const GroupEp& a, const GroupEp& b) {
                    if (a.air_date != b.air_date) return a.air_date < b.air_date;
                    if (order[a.show_id] != order[b.show_id]) return order[a.show_id] < order[b.show_id];
                    if (a.season != b.season) return a.season < b.season;
                    return a.episode < b.episode;
                });
                for (const auto& e : eps)
                    arr.push_back(group_ep_json(e, by_id[e.show_id], watched(e.show_id, e.season, e.episode)));
            } else {
                int show_id = std::stoi(req.get_param_value("show_id"));
                int season  = std::stoi(req.get_param_value("season"));
                auto it = by_id.find(show_id);
                if (it == by_id.end() || it->second.tmdb_id == 0) {
                    error_response(res, "Show is not in this group", 404);
                    return;
                }
                const Show& s = it->second;
                auto seasons = Scraper::fetch_show_seasons(s.tmdb_id);
                bool last = !seasons.empty() && seasons.back().season_number == season;
                auto eps = season_eps(db, s.tmdb_id, season, last);
                int total = static_cast<int>(eps.size());
                for (const auto& [ep, title, air] : eps)
                    arr.push_back(group_ep_json({s.id, season, ep, title, air, total}, s,
                                                watched(s.id, season, ep)));
            }
            json_response(res, arr);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/shows/:id/episodes/:season/:episode/imdb ----------------------
    // Single-episode IMDB ID on demand (cached), for views that don't prefetch.
    srv.Get(R"(/api/shows/(\d+)/episodes/(\d+)/(\d+)/imdb)",
        [&](const httplib::Request& req, httplib::Response& res) {
        try {
            Show s = db.get_show(std::stoi(req.matches[1]));
            int season = std::stoi(req.matches[2]), ep = std::stoi(req.matches[3]);
            std::string imdb;
            if (s.tmdb_id > 0) {
                if (db.episode_imdb_cached(s.tmdb_id, season, ep)) {
                    imdb = db.get_episode_imdb_id(s.tmdb_id, season, ep);
                } else {
                    imdb = Scraper::fetch_episode_imdb_id(s.tmdb_id, season, ep);
                    db.cache_episode_imdb(s.tmdb_id, season, ep, imdb);
                }
            }
            json_response(res, {{"imdb_id", imdb}});
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/groups ------------------------------------------------
    // Each group carries its "last" and "next" episode across members in
    // air-date order: next = the earliest-airing next-unwatched episode of any
    // member; last = the latest-airing current position of any member.
    srv.Get("/api/groups", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const std::string today = today_iso();
            auto hidden = impl.hidden_queues(req);
            // (episode title, air date) of one episode via the season cache
            auto lookup = [&](const Show& s, int season, int ep) -> std::pair<std::string, std::string> {
                if (s.tmdb_id == 0 || season <= 0) return {};
                bool last = season >= std::max(s.latest_season, s.season);
                for (const auto& [n, title, air] : season_eps(db, s.tmdb_id, season, last))
                    if (n == ep) return {title, air};
                return {};
            };
            auto shows = db.all_shows();
            json arr = json::array();
            for (const auto& g : db.all_groups()) {
                json next = nullptr, last = nullptr;
                std::string next_air, last_air;
                bool visible = false;
                for (const auto& s : shows) {
                    if (s.group_id != g.id || hidden.count(s.queue_id)) continue;
                    visible = true;
                    if (s.status != ShowStatus::Finished) {
                        int ns = s.season == 0 ? 1 : s.next_season;
                        int ne = s.season == 0 ? 1 : s.next_episode;
                        if (ns > 0) {
                            auto [title, air] = lookup(s, ns, ne);
                            // Prefer aired episodes; among them the earliest
                            std::string key = (air.empty() || air > today ? "9" : "0") + air;
                            if (next.is_null() || key < next_air) {
                                next_air = key;
                                next = {{"show_id", s.id}, {"show_title", s.title}, {"season", ns},
                                        {"episode", ne}, {"title", title}, {"air_date", air}};
                            }
                        }
                    }
                    if (s.season > 0) {
                        auto [title, air] = lookup(s, s.season, s.episode);
                        if (last.is_null() || air > last_air) {
                            last_air = air;
                            last = {{"show_id", s.id}, {"show_title", s.title}, {"season", s.season},
                                    {"episode", s.episode}, {"title", title}, {"air_date", air}};
                        }
                    }
                }
                if (visible)
                    arr.push_back({{"id", g.id}, {"name", g.name}, {"next", next}, {"last", last}});
            }
            json_response(res, arr);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- POST /api/groups  {show_ids: [first, second, ...], name?} --------
    srv.Post("/api/groups", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::vector<int> ids;
            if (j.contains("show_ids") && j["show_ids"].is_array())
                for (const auto& v : j["show_ids"])
                    if (v.is_number_integer()) ids.push_back(v.get<int>());
            if (ids.size() < 2) { error_response(res, "A group needs at least two shows"); return; }

            Show first = db.get_show(ids[0]);
            std::string name = j.value("name", "");
            if (name.empty()) name = default_group_name(first.title);
            int gid = db.create_group(name);
            for (int id : ids) {
                Show s = db.get_show(id);
                if (s.group_id && s.group_id != gid) db.set_show_group(s.id, 0, 0);
                join_group(db, s, gid, first);
            }
            db.prune_groups();   // a member may have left an old group of two
            json_response(res, {{"id", gid}, {"name", name}}, 201);
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- PUT /api/groups/:id  {name?, order?: [show ids]} ------------------
    srv.Put(R"(/api/groups/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int gid = std::stoi(req.matches[1]);
            if (!db.group_exists(gid)) { error_response(res, "No such group", 404); return; }
            auto j = json::parse(req.body);
            if (j.contains("name") && j["name"].is_string()) {
                std::string name = j["name"].get<std::string>();
                if (name.empty()) { error_response(res, "Group name can't be empty"); return; }
                db.rename_group(gid, name);
            }
            if (j.contains("order") && j["order"].is_array()) {
                int n = 0;
                for (const auto& v : j["order"]) {
                    if (!v.is_number_integer()) continue;
                    Show s = db.get_show(v.get<int>());
                    if (s.group_id == gid) db.set_show_group(s.id, gid, ++n);
                }
            }
            json_response(res, {{"ok", true}});
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- DELETE /api/groups/:id  (ungroup; shows are kept) -----------------
    srv.Delete(R"(/api/groups/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            db.delete_group(std::stoi(req.matches[1]));
            json_response(res, {{"ok", true}});
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- POST /api/groups/:id/members  {show_id} ---------------------------
    srv.Post(R"(/api/groups/(\d+)/members)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int gid = std::stoi(req.matches[1]);
            auto anchor = group_anchor(db, gid);
            if (!anchor) { error_response(res, "No such group", 404); return; }
            Show s = db.get_show(json::parse(req.body).value("show_id", 0));
            if (s.group_id == gid) { json_response(res, {{"ok", true}}); return; }
            if (s.group_id) db.set_show_group(s.id, 0, 0);
            join_group(db, s, gid, *anchor);
            db.prune_groups();
            json_response(res, {{"ok", true}});
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- DELETE /api/groups/:id/members/:show_id ---------------------------
    srv.Delete(R"(/api/groups/(\d+)/members/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int gid = std::stoi(req.matches[1]);
            Show s  = db.get_show(std::stoi(req.matches[2]));
            if (s.group_id == gid) db.set_show_group(s.id, 0, 0);
            db.prune_groups();   // the last remaining member becomes a plain show
            json_response(res, {{"ok", true}});
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- POST /api/queues/:id/unlock  {pin} ------------------------------
    // Correct PIN -> a token the browser sends back in X-Queue-Tokens. A wrong
    // PIN waits a second before answering, to slow down guessing.
    srv.Post(R"(/api/queues/(\d+)/unlock)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            Queue q = db.get_queue(std::stoi(req.matches[1]));
            auto j = json::parse(req.body);
            std::string pin = (j.contains("pin") && j["pin"].is_string()) ? j["pin"].get<std::string>() : "";
            if (q.pin.empty()) { json_response(res, {{"ok", true}, {"token", ""}}); return; }
            if (!Pin::verify(pin, q.pin)) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                error_response(res, "Wrong PIN", 403);
                return;
            }
            json_response(res, {{"ok", true}, {"token", impl.issue_token(q.id)}});
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/queues -------------------------------------------------
    srv.Get("/api/queues", [&](const httplib::Request&, httplib::Response& res) {
        try {
            json arr = json::array();
            for (const auto& q : db.all_queues())
                arr.push_back(queue_to_json(q));
            json_response(res, arr);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- POST /api/queues ------------------------------------------------
    srv.Post("/api/queues", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            Queue q;
            q.name = j.value("name", "");
            if (q.name.empty()) { error_response(res, "name required"); return; }
            q.sort_order = static_cast<int>(db.all_queues().size()) + 1;
            q.id = db.add_queue(q);
            json_response(res, queue_to_json(q), 201);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- PUT /api/queues/:id ---------------------------------------------
    srv.Put(R"(/api/queues/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            Queue q = db.get_queue(id);
            auto j = json::parse(req.body);
            if (j.contains("name"))       q.name       = j["name"];
            if (j.contains("sort_order")) q.sort_order = j["sort_order"];
            if (j.contains("pin")) {
                if (!impl.may_manage(req, q, j)) {
                    error_response(res, "This queue's current PIN is required", 403);
                    return;
                }
                std::string pin = j["pin"].is_string() ? j["pin"].get<std::string>() : "";
                q.pin = pin.empty() ? "" : Pin::hash(pin);
            }
            db.update_queue(q);
            json_response(res, queue_to_json(q));
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- DELETE /api/queues/:id ------------------------------------------
    srv.Delete(R"(/api/queues/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            if (db.queue_count() <= 1) {
                error_response(res, "Cannot delete the last queue");
                return;
            }
            if (!impl.may_manage(req, db.get_queue(id), json::object())) {
                error_response(res, "Unlock this queue before deleting it", 403);
                return;
            }
            db.delete_queue(id);
            json_response(res, {{"ok", true}});
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/shows ---------------------------------------------------
    srv.Get("/api/shows", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int qid = 0;
            if (req.has_param("queue_id"))
                qid = std::stoi(req.get_param_value("queue_id"));
            auto hidden = impl.hidden_queues(req);
            if (hidden.count(qid)) {
                json_response(res, {{"error", "This queue is locked"}, {"locked", true}}, 403);
                return;
            }
            json arr = json::array();
            for (auto s : db.all_shows()) {
                if (qid > 0 && s.queue_id != qid) continue;
                if (hidden.count(s.queue_id)) continue;
                // Backfill next_episode_title for shows that pre-date the field.
                if (s.next_season > 0 && s.tmdb_id > 0 && s.next_episode_title.empty()) {
                    s.next_episode_title =
                        Scraper::fetch_episode_title(s.tmdb_id, s.next_season, s.next_episode);
                    db.update_show(s);
                }
                arr.push_back(show_to_json(s));
            }
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
            s.queue          = show_queue_from(j.value("queue", "current"));
            s.queue_id       = j.value("queue_id", 1);
            s.notes          = j.value("notes", "");
            if (s.title.empty()) { error_response(res, "title required"); return; }
            s.sort_order = db.max_show_sort_order(s.queue, s.queue_id) + 1;
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
            if (j.contains("queue_id")) {
                int new_qid = j["queue_id"];
                if (new_qid != s.queue_id) {
                    s.queue_id = new_qid;
                    s.sort_order = db.max_show_sort_order(s.queue, s.queue_id) + 1;
                }
            }
            if (j.contains("queue")) {
                auto new_queue = show_queue_from(j["queue"]);
                if (new_queue != s.queue) {
                    s.queue = new_queue;
                    s.sort_order = db.max_show_sort_order(s.queue, s.queue_id) + 1;
                }
            }
            if (j.contains("sort_order"))     s.sort_order     = j["sort_order"];
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
            db.prune_groups();   // removing a member may leave a group of one
            json_response(res, {{"ok", true}});
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- PUT /api/shows/reorder -------------------------------------------
    srv.Put("/api/shows/reorder", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            if (!j.contains("order") || !j["order"].is_array()) {
                error_response(res, "order array required");
                return;
            }
            std::vector<int> ids;
            for (const auto& id : j["order"])
                ids.push_back(id.get<int>());
            db.reorder_shows(ids);
            json_response(res, {{"ok", true}});
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- GET /api/movies --------------------------------------------------
    srv.Get("/api/movies", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int qid = 0;
            if (req.has_param("queue_id"))
                qid = std::stoi(req.get_param_value("queue_id"));
            auto hidden = impl.hidden_queues(req);
            if (hidden.count(qid)) {
                json_response(res, {{"error", "This queue is locked"}, {"locked", true}}, 403);
                return;
            }
            json arr = json::array();
            for (auto m : db.all_movies()) {
                if (qid > 0 && m.queue_id != qid) continue;
                if (hidden.count(m.queue_id)) continue;
                // Backfill tmdb_id for movies added before it was tracked.
                if (m.tmdb_id == 0 && !m.imdb_id.empty()) {
                    auto info = Scraper::fetch_movie_info(m.imdb_id);
                    if (info && info->tmdb_id > 0) {
                        m.tmdb_id = info->tmdb_id;
                        db.update_movie(m);
                    }
                }
                arr.push_back(movie_to_json(m));
            }
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
            m.status   = movie_status_from(j.value("status", "want_to_watch"));
            m.queue_id = j.value("queue_id", 1);
            m.notes    = j.value("notes", "");
            if (m.title.empty()) { error_response(res, "title required"); return; }
            m.sort_order = db.max_movie_sort_order(m.queue_id) + 1;
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

    // --- PUT /api/movies/reorder -------------------------------------------
    srv.Put("/api/movies/reorder", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            if (!j.contains("order") || !j["order"].is_array()) {
                error_response(res, "order array required");
                return;
            }
            std::vector<int> ids;
            for (const auto& id : j["order"])
                ids.push_back(id.get<int>());
            db.reorder_movies(ids);
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
                std::string ep_imdb;
                if (db.episode_imdb_cached(show.tmdb_id, ep.season, ep.episode)) {
                    ep_imdb = db.get_episode_imdb_id(show.tmdb_id, ep.season, ep.episode);
                } else {
                    ep_imdb = Scraper::fetch_episode_imdb_id(show.tmdb_id, ep.season, ep.episode);
                    db.cache_episode_imdb(show.tmdb_id, ep.season, ep.episode, ep_imdb);
                }
                arr.push_back({
                    {"season",       ep.season},
                    {"episode",      ep.episode},
                    {"title",        ep.title},
                    {"episode_url",  ep.episode_url},
                    {"air_date",     ep.air_date},
                    {"watched",      watched.count(ep.episode) > 0},
                    {"imdb_id",      ep_imdb},
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
    // --- PUT /api/shows/:id/seasons/:season/watched  {watched: bool} ----------
    // Whole-season tick/untick (right-click on a season in the episode browser).
    // Watched = every *aired* episode of the season, via the cached episode
    // list (one TMDB call at most, no per-episode IMDB lookups). The position
    // only moves forward, so back-filling an old season doesn't rewind it;
    // unmarking the season you're in falls back to the latest still-ticked one.
    srv.Put(R"(/api/shows/(\d+)/seasons/(\d+)/watched)",
        [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id     = std::stoi(req.matches[1]);
            int season = std::stoi(req.matches[2]);
            bool watched = json::parse(req.body).value("watched", true);
            Show show = db.get_show(id);
            if (show.tmdb_id == 0) {
                error_response(res, "Show isn't linked to TMDB", 422);
                return;
            }

            int marked = 0;
            if (watched) {
                bool last_season = season >= std::max(show.latest_season, show.season);
                auto eps = season_eps(db, show.tmdb_id, season, last_season);
                if (eps.empty()) {
                    error_response(res, "Could not load this season's episodes from TMDB", 502);
                    return;
                }
                const std::string today = today_iso();
                int last_aired = 0;
                for (const auto& [ep, title, air] : eps)
                    if (!air.empty() && air <= today) last_aired = std::max(last_aired, ep);
                if (last_aired == 0) {
                    error_response(res, "No episodes of this season have aired yet", 422);
                    return;
                }
                marked = db.mark_watched_through(id, {{season, last_aired}});
                bool forward = season > show.season ||
                               (season == show.season && last_aired > show.episode);
                if (forward) {
                    show.season          = season;
                    show.episode         = last_aired;
                    show.season_episodes = static_cast<int>(eps.size());
                }
            } else {
                db.clear_season_watched(id, season);
                if (season == show.season) {
                    auto [max_s, max_e] = db.max_watched_position(id);
                    if (max_s != show.season) show.season_episodes = 0;
                    show.season  = max_s;
                    show.episode = max_e;
                }
            }

            auto [ns, ne] = db.compute_next_unwatched(id, show.season, show.episode, show.season_episodes);
            if (ns != show.next_season || ne != show.next_episode) {
                show.next_season  = ns;
                show.next_episode = ne;
                show.next_episode_title = (ns > 0)
                    ? Scraper::fetch_episode_title(show.tmdb_id, ns, ne) : std::string{};
            }
            db.update_show(show);
            json_response(res, {{"ok", true}, {"marked", marked}, {"show", show_to_json(show)}});
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    // --- POST /api/shows/:id/mark-aired-watched -----------------------------
    // "I'm caught up": marks every aired episode watched using one TMDB call
    // (season episode counts + last aired episode). Unlike clicking through
    // seasons, this fetches no episode lists or per-episode IMDB IDs.
    srv.Post(R"(/api/shows/(\d+)/mark-aired-watched)",
        [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            Show show = db.get_show(id);
            if (show.tmdb_id == 0 && !show.imdb_id.empty()) {
                if (auto info = Scraper::fetch_show_info(show.imdb_id)) {
                    show.tmdb_id = info->tmdb_id;
                    if (show.thumbnail_url.empty()) show.thumbnail_url = info->image_url;
                }
            }
            if (show.tmdb_id == 0) {
                error_response(res, "Show isn't linked to TMDB (needs a TMDB search match or IMDB ID)", 422);
                return;
            }
            auto aired = Scraper::fetch_aired_summary(show.tmdb_id);
            if (!aired) {
                error_response(res, "Could not fetch the show from TMDB", 502);
                return;
            }
            if (aired->first_air_date.empty() || aired->first_air_date > today_iso() ||
                    aired->last_aired.season <= 0) {
                error_response(res, "This show hasn't aired yet, so there is nothing to mark", 422);
                return;
            }

            const int ls = aired->last_aired.season, le = aired->last_aired.episode;
            std::vector<std::pair<int,int>> through;
            int last_season_eps = le;
            for (const auto& se : aired->seasons) {
                if (se.season_number > ls) break;
                int last = (se.season_number == ls) ? le : se.episode_count;
                if (se.season_number == ls) last_season_eps = std::max(le, se.episode_count);
                if (last > 0) through.push_back({se.season_number, last});
            }
            int added = db.mark_watched_through(id, through);

            show.season          = ls;
            show.episode         = le;
            show.season_episodes = last_season_eps;
            show.latest_season   = ls;
            show.latest_episode  = le;
            if (aired->next_to_air.season > 0) {
                show.next_season        = aired->next_to_air.season;
                show.next_episode       = aired->next_to_air.episode;
                show.next_episode_title = aired->next_to_air.title;
            } else {
                // Nothing scheduled: no "Next" episode to point at
                show.next_season        = 0;
                show.next_episode       = 0;
                show.next_episode_title.clear();
                if (aired->ended) show.status = ShowStatus::Finished;
            }
            db.update_show(show);
            json_response(res, {{"ok", true}, {"marked", added}, {"show", show_to_json(show)}});
        } catch (const DbError& e) {
            error_response(res, e.what(), 404);
        } catch (const std::exception& e) {
            error_response(res, e.what(), 500);
        }
    });

    srv.Put(R"(/api/shows/(\d+)/episodes/(\d+)/(\d+)/watched)",
        [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int show_id = std::stoi(req.matches[1]);
            int season  = std::stoi(req.matches[2]);
            int episode = std::stoi(req.matches[3]);
            auto j = json::parse(req.body);
            bool watched     = j.value("watched", true);
            int season_total = j.value("season_total", 0);

            db.set_episode_watched(show_id, season, episode, watched);

            Show show = db.get_show(show_id);
            bool changed = false;
            if (watched) {
                // Always set position to the episode just checked (last-watched semantics)
                if (show.season != season || show.episode != episode) {
                    if (show.season != season) show.season_episodes = 0;
                    show.season  = season;
                    show.episode = episode;
                    changed = true;
                }
            } else {
                // Only recalculate if we unchecked the current position episode
                if (season == show.season && episode == show.episode) {
                    auto [max_s, max_e] = db.max_watched_position(show_id);
                    if (show.season != max_s || show.episode != max_e) {
                        if (show.season != max_s) show.season_episodes = 0;
                        show.season  = max_s;
                        show.episode = max_e;
                        changed = true;
                    }
                }
            }
            if (season_total > 0 && season == show.season &&
                    show.season_episodes != season_total) {
                show.season_episodes = season_total;
                changed = true;
            }
            // Recompute next unwatched after any position change
            auto [ns, ne] = db.compute_next_unwatched(show_id, show.season, show.episode, show.season_episodes);
            if (show.next_season != ns || show.next_episode != ne) {
                show.next_season  = ns;
                show.next_episode = ne;
                show.next_episode_title = (ns > 0 && show.tmdb_id > 0)
                    ? Scraper::fetch_episode_title(show.tmdb_id, ns, ne)
                    : std::string{};
                changed = true;
            }
            if (changed) db.update_show(show);
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

                        const auto& la = info->latest_aired;
                        if (la.season == 0) {
                            if (changed) db.update_show(show);
                            send("    no aired-episode data found");
                            continue;
                        }
                        bool ahead = (la.season > show.season) ||
                                     (la.season == show.season && la.episode > show.episode);
                        int new_ls = ahead ? la.season  : 0;
                        int new_le = ahead ? la.episode : 0;
                        if (show.latest_season != new_ls || show.latest_episode != new_le)
                            { show.latest_season = new_ls; show.latest_episode = new_le; changed = true; }
                        if (changed) db.update_show(show);
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

    // --- GET /api/shows/:id/cast ------------------------------------------
    // First call fetches from TMDB and caches in people + show_cast tables.
    // Subsequent calls are served entirely from the DB.
    srv.Get(R"(/api/shows/(\d+)/cast)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int show_id = std::stoi(req.matches[1]);
            Show show = db.get_show(show_id);
            if (show.tmdb_id == 0) { json_response(res, json::array()); return; }
            if (!db.show_cast_cached(show_id)) {
                auto cast = Scraper::fetch_show_cast(show.tmdb_id);
                for (auto& cm : cast) {
                    if (!db.person_cached(cm.tmdb_person_id)) {
                        cm.imdb_id = Scraper::fetch_person_imdb_id(cm.tmdb_person_id);
                        db.upsert_person(cm.tmdb_person_id, cm.name, cm.imdb_id, cm.profile_url);
                    } else {
                        cm.imdb_id = db.get_person_imdb_id(cm.tmdb_person_id);
                    }
                }
                db.store_show_cast(show_id, cast);
            }
            json arr = json::array();
            for (const auto& cm : db.get_show_cast(show_id))
                arr.push_back(cast_member_to_json(cm));
            json_response(res, arr);
        } catch (const std::exception& e) { error_response(res, e.what(), 500); }
    });

    // --- GET /api/shows/:id/episodes/:season/:episode/cast ----------------
    srv.Get(R"(/api/shows/(\d+)/episodes/(\d+)/(\d+)/cast)",
        [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int show_id = std::stoi(req.matches[1]);
            int season  = std::stoi(req.matches[2]);
            int episode = std::stoi(req.matches[3]);
            Show show = db.get_show(show_id);
            if (show.tmdb_id == 0) { json_response(res, json::array()); return; }
            if (!db.episode_cast_cached(show_id, season, episode)) {
                auto cast = Scraper::fetch_episode_cast(show.tmdb_id, season, episode);
                for (auto& cm : cast) {
                    if (!db.person_cached(cm.tmdb_person_id)) {
                        cm.imdb_id = Scraper::fetch_person_imdb_id(cm.tmdb_person_id);
                        db.upsert_person(cm.tmdb_person_id, cm.name, cm.imdb_id, cm.profile_url);
                    } else {
                        cm.imdb_id = db.get_person_imdb_id(cm.tmdb_person_id);
                    }
                }
                db.store_episode_cast(show_id, season, episode, cast);
            }
            json arr = json::array();
            for (const auto& cm : db.get_episode_cast(show_id, season, episode))
                arr.push_back(cast_member_to_json(cm));
            json_response(res, arr);
        } catch (const std::exception& e) { error_response(res, e.what(), 500); }
    });

    // --- GET /api/movies/:id/cast -----------------------------------------
    srv.Get(R"(/api/movies/(\d+)/cast)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int movie_id = std::stoi(req.matches[1]);
            Movie movie = db.get_movie(movie_id);
            if (movie.tmdb_id == 0 && !movie.imdb_id.empty()) {
                auto info = Scraper::fetch_movie_info(movie.imdb_id);
                if (info && info->tmdb_id > 0) {
                    movie.tmdb_id = info->tmdb_id;
                    db.update_movie(movie);
                }
            }
            if (movie.tmdb_id == 0) { json_response(res, json::array()); return; }
            if (!db.movie_cast_cached(movie_id)) {
                auto cast = Scraper::fetch_movie_cast(movie.tmdb_id);
                for (auto& cm : cast) {
                    if (!db.person_cached(cm.tmdb_person_id)) {
                        cm.imdb_id = Scraper::fetch_person_imdb_id(cm.tmdb_person_id);
                        db.upsert_person(cm.tmdb_person_id, cm.name, cm.imdb_id, cm.profile_url);
                    } else {
                        cm.imdb_id = db.get_person_imdb_id(cm.tmdb_person_id);
                    }
                }
                db.store_movie_cast(movie_id, cast);
            }
            json arr = json::array();
            for (const auto& cm : db.get_movie_cast(movie_id))
                arr.push_back(cast_member_to_json(cm));
            json_response(res, arr);
        } catch (const std::exception& e) { error_response(res, e.what(), 500); }
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

// SN: 00006
