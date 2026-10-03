// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "server.hpp"
#include "../lib/config.hpp"
#include "../lib/database.hpp"
#include "../lib/models.hpp"
#include "../lib/platform.hpp"
#include "../lib/scraper.hpp"
#include "../lib/settings.hpp"
#include "../lib/version.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdio>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string ep_label(int season, int episode) {
    std::ostringstream ss;
    ss << "s" << std::setw(3) << std::setfill('0') << season
       << "-e" << std::setw(3) << std::setfill('0') << episode;
    return ss.str();
}

static int run_check(const std::string& db_path) {
    FlickImp::Database db(db_path);
    auto shows = db.all_shows();

    int active = 0;
    for (const auto& s : shows)
        if (s.status != FlickImp::ShowStatus::Finished && !s.imdb_id.empty()) ++active;

    std::cout << "Checking " << active << " active show(s) for new episodes...\n\n";

    int checked = 0, found_new = 0;
    bool first = true;

    for (auto& show : shows) {
        if (show.status == FlickImp::ShowStatus::Finished) continue;

        if (show.imdb_id.empty()) {
            std::cout << "  " << show.title << " — no IMDB ID, skipping\n";
            continue;
        }

        // Polite delay between requests (not before the first one)
        if (!first)
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
        first = false;

        std::cout << "  " << show.title << " (" << show.imdb_id << ")..." << std::flush;

        auto info = FlickImp::Scraper::fetch_show_info(show.imdb_id);
        if (!info) {
            std::cout << " FAILED (see above)\n";
            continue;
        }
        ++checked;

        // Opportunistically update cached fields in the DB
        bool changed = false;
        if (show.tmdb_id == 0 && info->tmdb_id > 0) {
            show.tmdb_id = info->tmdb_id;
            changed = true;
        }
        if (info->total_episodes > 0 && info->total_episodes != show.total_episodes) {
            show.total_episodes = info->total_episodes;
            changed = true;
        }
        if (show.thumbnail_url.empty() && !info->image_url.empty()) {
            show.thumbnail_url = info->image_url;
            changed = true;
        }

        const auto& la = info->latest_aired;
        if (la.season == 0) {
            if (changed) db.update_show(show);
            std::cout << " no aired-episode data found\n";
            continue;
        }

        bool ahead = (la.season > show.season) ||
                     (la.season == show.season && la.episode > show.episode);

        int new_ls = ahead ? la.season   : 0;
        int new_le = ahead ? la.episode  : 0;
        if (show.latest_season != new_ls || show.latest_episode != new_le) {
            show.latest_season  = new_ls;
            show.latest_episode = new_le;
            changed = true;
        }
        if (changed) db.update_show(show);

        if (ahead) {
            ++found_new;
            std::cout << " NEW\n"
                      << "    You're on " << ep_label(show.season, show.episode)
                      << "  |  Latest aired: " << ep_label(la.season, la.episode);
            if (!la.title.empty())
                std::cout << " \"" << la.title << "\"";
            if (!la.air_date.empty())
                std::cout << "  [" << la.air_date << "]";
            std::cout << "\n";
        } else {
            std::cout << " all caught up at " << ep_label(show.season, show.episode) << "\n";
        }
    }

    std::cout << "\n" << checked << " show(s) checked, "
              << found_new << " with new episode(s).\n";
    return 0;
}

// --clear-pin QUEUE: admin reset for a forgotten queue PIN. QUEUE is a queue
// id or name (case-insensitive). Needs write access to the DB file, i.e. the
// service account — which is the point: only someone with access to the
// server itself can do it, not someone with just the web page.
static int run_clear_pin(const std::string& db_path, const std::string& target) {
#ifndef _WIN32
    if (::geteuid() == 0) {
        // As root, SQLite can leave root-owned -wal/-shm files that the
        // daemon (running as flickimp) then can't open.
        std::cerr << "Don't run --clear-pin as root; run it as the service account:\n"
                  << "  sudo -u flickimp flickimp --clear-pin \"" << target << "\"\n";
        return 1;
    }
#endif
    FlickImp::Database db(db_path);
    auto queues = db.all_queues();

    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };
    std::vector<FlickImp::Queue> matches;
    bool numeric = !target.empty() &&
                   std::all_of(target.begin(), target.end(), [](unsigned char c) { return std::isdigit(c); });
    for (const auto& q : queues) {
        if (numeric ? q.id == std::stoi(target) : lower(q.name) == lower(target))
            matches.push_back(q);
    }

    if (matches.size() != 1) {
        std::cerr << (matches.empty() ? "No queue matches \"" : "More than one queue is named \"")
                  << target << "\". Use a name or id from this list:\n";
        for (const auto& q : queues)
            std::cerr << "  " << std::setw(3) << q.id << "  " << q.name
                      << (q.pin.empty() ? "" : "  (PIN set)") << "\n";
        return 1;
    }

    FlickImp::Queue q = matches.front();
    if (q.pin.empty()) {
        std::cout << "Queue \"" << q.name << "\" has no PIN; nothing to do.\n";
        return 0;
    }
    q.pin.clear();
    db.update_queue(q);
    std::cout << "PIN cleared for queue \"" << q.name << "\" (id " << q.id << ").\n";
    return 0;
}

int main(int argc, char* argv[]) {
    // Config file provides defaults; command-line args override
    auto cfg = FlickImp::load_config();
    int cli_port = 0;
    std::string web_root = cfg.fi_web_root;
    bool check_mode = false;
    std::string log_file;
    std::string clear_pin;      // --clear-pin QUEUE

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            cli_port = std::stoi(argv[++i]);
        } else if ((arg == "--web" || arg == "-w") && i + 1 < argc) {
            web_root = argv[++i];
        } else if ((arg == "--log" || arg == "-l") && i + 1 < argc) {
            log_file = argv[++i];
        } else if (arg == "--check" || arg == "-c") {
            check_mode = true;
        } else if (arg == "--clear-pin") {
            if (i + 1 >= argc) {
                std::cerr << "--clear-pin needs a queue name or id\n";
                return 1;
            }
            clear_pin = argv[++i];
        } else if (arg == "--version" || arg == "-v") {
            std::cout << APP_NAME " " APP_VERSION "\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout <<
                "Usage: flickimp [OPTIONS]\n"
                "  --port N     HTTP port for the web interface (default: 8647)\n"
                "  --web DIR    Web assets directory\n"
                "  --log FILE   Append stdout/stderr to FILE (for launchd / Task Scheduler)\n"
                "  --check      Check IMDB for new episodes on all tracked shows, then exit\n"
                "  --clear-pin QUEUE\n"
                "               Remove a queue's PIN (queue name or id), then exit.\n"
                "               Run as the service account: sudo -u flickimp flickimp --clear-pin Kids\n"
                "  --version    Print version and exit\n";
            return 0;
        }
    }

    // Background launchers (launchd, Task Scheduler) have no terminal —
    // redirect output to a file, unbuffered so log lines appear immediately.
    if (!log_file.empty()) {
        if (!std::freopen(log_file.c_str(), "a", stdout) ||
            !std::freopen(log_file.c_str(), "a", stderr)) {
            return 1;
        }
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
    }

    std::string db_path = cfg.fi_db_path.empty()
        ? FlickImp::Platform::db_path()
        : cfg.fi_db_path + "/flickimp.db";

    if (!clear_pin.empty()) {
        try {
            return run_clear_pin(db_path, clear_pin);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    // Settings saved from the web UI live in the DB and override the config
    // file (--port still beats both). Initialise the TMDB scraper from the
    // result — must happen before any Scraper:: calls.
    int port = 8647;
    try {
        FlickImp::Database db(db_path);
        auto eff = FlickImp::Settings::resolve(cfg, db, cli_port);
        FlickImp::Scraper::init(eff.tmdb_api_key, eff.tmdb_bearer_token);
        port = eff.port;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    if (check_mode) {
        try {
            return run_check(db_path);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    // Normal mode: start the HTTP server
    if (web_root.empty()) {
        // 1. Next to the binary — dev build dir, and the Windows/macOS
        //    install layout (flickimp[.exe] + web/ in one directory)
        fs::path bin_web = fs::path(FlickImp::Platform::exe_dir()) / "web";
        if (fs::exists(bin_web))
            web_root = bin_web.string();
        // 2. Platform data dir — /var/lib/flickimp/web (system) or XDG (dev user)
        else
            web_root = (fs::path(FlickImp::Platform::data_dir()) / "web").string();
    }

    try {
        FlickImp::ServerOptions opts;
        opts.db_path  = db_path;
        opts.web_root = web_root;
        opts.port     = port;
        opts.cli_port = cli_port;
        opts.file_cfg = cfg;
        FlickImp::Server srv(opts);
        std::cout << "FlickImp " APP_VERSION
                  << " — http://localhost:" << port << "\n"
                  << "  DB:  " << db_path << "\n"
                  << "  Web: " << web_root << "\n"
                  << "  Press Ctrl+C to stop.\n";
        srv.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

// SN: 00006
