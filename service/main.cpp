// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "server.hpp"
#include "../lib/database.hpp"
#include "../lib/models.hpp"
#include "../lib/platform.hpp"
#include "../lib/scraper.hpp"
#include "../lib/version.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

static std::string ep_label(int season, int episode) {
    std::ostringstream ss;
    ss << "S" << std::setw(2) << std::setfill('0') << season
       << "E" << std::setw(2) << std::setfill('0') << episode;
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

        // Opportunistically update total_episodes in the DB
        if (info->total_episodes > 0 && info->total_episodes != show.total_episodes) {
            show.total_episodes = info->total_episodes;
            db.update_show(show);
        }

        const auto& la = info->latest_aired;
        if (la.season == 0) {
            std::cout << " no aired-episode data found\n";
            continue;
        }

        bool ahead = (la.season > show.season) ||
                     (la.season == show.season && la.episode > show.episode);

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

int main(int argc, char* argv[]) {
    int port = 8647;
    std::string web_root;
    bool check_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if ((arg == "--web" || arg == "-w") && i + 1 < argc) {
            web_root = argv[++i];
        } else if (arg == "--check" || arg == "-c") {
            check_mode = true;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << APP_NAME " " APP_VERSION "\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout <<
                "Usage: flickimp [OPTIONS]\n"
                "  --port N     HTTP port for the web interface (default: 8647)\n"
                "  --web DIR    Web assets directory\n"
                "  --check      Check IMDB for new episodes on all tracked shows, then exit\n"
                "  --version    Print version and exit\n";
            return 0;
        }
    }

    if (check_mode) {
        try {
            return run_check(FlickImp::Platform::db_path());
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    // Normal mode: start the HTTP server
    if (web_root.empty()) {
        std::string bin_dir = fs::weakly_canonical(fs::path(argv[0])).parent_path().string();
        if (fs::exists(bin_dir + "/web"))
            web_root = bin_dir + "/web";
        else
            web_root = FlickImp::Platform::data_dir() + "/web";
    }

    try {
        FlickImp::Server srv(FlickImp::Platform::db_path(), web_root, port);
        std::cout << "FlickImp " APP_VERSION
                  << " — http://localhost:" << port << "\n"
                  << "  DB:  " << FlickImp::Platform::db_path() << "\n"
                  << "  Web: " << web_root << "\n"
                  << "  Press Ctrl+C to stop.\n";
        srv.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

// SN: 00001
