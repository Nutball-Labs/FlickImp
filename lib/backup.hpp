// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include "database.hpp"
#include "settings.hpp"
#include <json.hpp>
#include <set>

// Backup / restore of all user content + settings as one JSON document.
// Compression (gzip) is done by the browser, so the daemon only sees JSON.
//
// Format (format_version 1):
//   { "format": "flickimp-backup", "format_version": 1,
//     "app_version": "1.5.0", "created": "2026-10-03T14:00:00Z",
//     "settings": { "tmdb_bearer_token": "...", "tmdb_api_key": "...", "port": 8647 },
//     "tables":   { "queues": [...], "shows": [...], "movies": [...], "episode_watches": [...] },
//     "caches":   { "people": [...], "show_cast": [...], ... }      // optional
//     "excluded_queues": [ {"id": 2, "name": "Kids"} ],              // left out (PIN)
//     "client":   { ... }                                            // browser prefs, ignored here
//   }
// Each table is an array of row objects keyed by column name.
namespace FlickImp::Backup {

enum class Mode {
    Replace,   // wipe content, load the backup exactly (ids preserved)
    Merge      // add queues/shows/movies not already present; leave existing ones alone
};

// exclude_queues: queue ids left out entirely (PIN-protected queues the
// requester hasn't unlocked, or chose to skip) — with their shows, movies,
// watch history, groups and TMDB cache rows.
nlohmann::json export_all(Database& db, const Settings::Effective& eff, bool include_caches,
                          const std::set<int>& exclude_queues = {});

// Throws std::runtime_error on an invalid backup; DbError on SQL failure
// (the restore runs in one transaction and is rolled back on any error).
// Returns a summary of what was restored.
nlohmann::json restore(Database& db, const nlohmann::json& backup, Mode mode,
                       const Settings::Effective& current);

} // namespace FlickImp::Backup

// SN: 00006
