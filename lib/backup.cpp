// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "backup.hpp"
#include "version.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <map>
#include <stdexcept>

using json = nlohmann::json;

namespace FlickImp::Backup {

namespace {

const char* FORMAT_NAME    = "flickimp-backup";
const int   FORMAT_VERSION = 1;

// Content tables, in insert order (parents before children)
const std::vector<std::string> CONTENT_TABLES = {
    "queues", "show_groups", "shows", "movies", "episode_watches"
};
// TMDB-derived caches. people, episode_imdb_ids and tmdb_* are keyed by TMDB
// ids and stay valid across databases; the *_cast tables use local ids.
const std::vector<std::string> CACHE_TABLES = {
    "people", "episode_imdb_ids", "tmdb_episodes", "tmdb_seasons",
    "show_cast", "movie_cast", "episode_cast"
};
const std::vector<std::string> LOCAL_ID_CACHES = {
    "show_cast", "movie_cast", "episode_cast"
};

std::string utc_now_iso() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

const json& rows_of(const json& section, const std::string& table) {
    static const json empty = json::array();
    if (!section.is_object() || !section.contains(table) || !section[table].is_array())
        return empty;
    return section[table];
}

long long id_of(const json& row, const char* col) {
    auto it = row.find(col);
    return (it != row.end() && it->is_number_integer()) ? it->get<long long>() : 0;
}

std::string str_of(const json& row, const char* col) {
    auto it = row.find(col);
    return (it != row.end() && it->is_string()) ? it->get<std::string>() : std::string{};
}

// Rows sorted by sort_order so merged items keep the backup's relative order
std::vector<json> by_sort_order(const json& rows) {
    std::vector<json> v(rows.begin(), rows.end());
    std::stable_sort(v.begin(), v.end(), [](const json& a, const json& b) {
        return id_of(a, "sort_order") < id_of(b, "sort_order");
    });
    return v;
}

// Identity used to decide whether a show/movie already exists in a queue:
// TMDB id, else IMDB id, else lower-cased title.
std::string identity(long long tmdb_id, const std::string& imdb_id, const std::string& title) {
    if (tmdb_id > 0)       return "tmdb:" + std::to_string(tmdb_id);
    if (!imdb_id.empty())  return "imdb:" + imdb_id;
    return "title:" + lower(title);
}

void validate(const json& b) {
    if (!b.is_object() || b.value("format", "") != FORMAT_NAME)
        throw std::runtime_error("Not a FlickImp backup file");
    int v = b.value("format_version", 0);
    if (v < 1 || v > FORMAT_VERSION)
        throw std::runtime_error("Backup format version " + std::to_string(v) +
                                 " is not supported by this version of FlickImp");
    if (!b.contains("tables") || !b["tables"].is_object())
        throw std::runtime_error("Backup has no tables section");
}

// Apply backup settings. Replace: every key in the backup wins.
// Merge: only fill settings that currently have no value.
json apply_settings(Database& db, const json& s, Mode mode, const Settings::Effective& cur) {
    json applied = json::array();
    if (!s.is_object()) return applied;

    auto apply = [&](const char* key, const std::string& val, bool currently_set) {
        if (val.empty()) return;
        if (mode == Mode::Merge && currently_set) return;
        db.set_setting(key, val);
        applied.push_back(key);
    };
    apply(Settings::KEY_BEARER,  str_of(s, Settings::KEY_BEARER),  !cur.tmdb_bearer_token.empty());
    apply(Settings::KEY_API_KEY, str_of(s, Settings::KEY_API_KEY), !cur.tmdb_api_key.empty());
    long long port = id_of(s, Settings::KEY_PORT);
    if (port > 0 && port < 65536)
        apply(Settings::KEY_PORT, std::to_string(port), !db.get_setting(Settings::KEY_PORT).empty());
    return applied;
}

json restore_replace(Database& db, const json& b) {
    const json& tables = b["tables"];
    const bool  caches = b.contains("caches") && b["caches"].is_object();
    json counts = json::object();

    // Children first. people / episode_imdb_ids are TMDB-keyed and still
    // valid, so they are kept (and topped up if the backup carries caches).
    for (const auto& t : {"episode_watches", "show_cast", "movie_cast", "episode_cast",
                          "shows", "show_groups", "movies", "queues"})
        db.clear_table(t);

    for (const auto& t : CONTENT_TABLES) {
        int n = 0;
        for (const auto& row : rows_of(tables, t)) { db.insert_row(t, row); ++n; }
        counts[t] = n;
    }
    if (caches) {
        for (const auto& t : CACHE_TABLES) {
            int n = 0;
            for (const auto& row : rows_of(b["caches"], t))
                if (db.insert_row(t, row, "INSERT OR REPLACE")) ++n;
            counts[t] = n;
        }
    }
    if (db.queue_count() == 0) {
        Queue q;
        q.name = "Default";
        q.sort_order = 1;
        db.add_queue(q);
    }
    db.prune_groups();
    return counts;
}

json restore_merge(Database& db, const json& b) {
    const json& tables = b["tables"];
    const bool  caches = b.contains("caches") && b["caches"].is_object();
    json counts = json::object();

    // Queues: matched by name (case-insensitive)
    std::map<std::string, int> queue_by_name;
    for (const auto& q : db.all_queues()) queue_by_name[lower(q.name)] = q.id;
    std::map<long long, long long> queue_map;   // backup id -> local id
    int queues_added = 0;
    for (auto row : by_sort_order(rows_of(tables, "queues"))) {
        long long old_id = id_of(row, "id");
        auto it = queue_by_name.find(lower(str_of(row, "name")));
        if (it != queue_by_name.end()) { queue_map[old_id] = it->second; continue; }
        row.erase("id");
        row["sort_order"] = db.queue_count() + 1;
        long long new_id = db.insert_row("queues", row);
        queue_map[old_id] = new_id;
        queue_by_name[lower(str_of(row, "name"))] = static_cast<int>(new_id);
        ++queues_added;
    }
    // Items whose queue isn't in the backup land in the first local queue
    auto local_queue = [&](const json& row) -> long long {
        auto it = queue_map.find(id_of(row, "queue_id"));
        if (it != queue_map.end()) return it->second;
        auto qs = db.all_queues();
        return qs.empty() ? 1 : qs.front().id;
    };

    // Groups: a new local group is created the first time an added show
    // needs it. Already-present shows keep whatever grouping they have.
    std::map<long long, std::string> group_names;
    for (const auto& g : rows_of(tables, "show_groups"))
        group_names[id_of(g, "id")] = str_of(g, "name");
    std::map<long long, long long> group_map;
    auto local_group = [&](const json& row) -> long long {
        long long old = id_of(row, "group_id");
        if (old <= 0 || !group_names.count(old)) return 0;
        auto it = group_map.find(old);
        if (it != group_map.end()) return it->second;
        return group_map[old] = db.create_group(group_names[old]);
    };

    // Shows: skip if the same show is already in the target queue
    std::map<long long, std::set<std::string>> shows_in_queue;
    for (const auto& s : db.all_shows())
        shows_in_queue[s.queue_id].insert(identity(s.tmdb_id, s.imdb_id, s.title));
    std::map<long long, long long> show_map;
    int shows_added = 0, shows_skipped = 0;
    for (auto row : by_sort_order(rows_of(tables, "shows"))) {
        long long qid = local_queue(row);
        auto key = identity(id_of(row, "tmdb_id"), str_of(row, "imdb_id"), str_of(row, "title"));
        if (!shows_in_queue[qid].insert(key).second) { ++shows_skipped; continue; }
        long long old_id = id_of(row, "id");
        row.erase("id");
        row["queue_id"]   = qid;
        row["group_id"]   = local_group(row);
        row["sort_order"] = db.max_show_sort_order(
            static_cast<ShowQueue>(id_of(row, "queue")), static_cast<int>(qid)) + 1;
        show_map[old_id] = db.insert_row("shows", row);
        ++shows_added;
    }

    // Movies: same rule
    std::map<long long, std::set<std::string>> movies_in_queue;
    for (const auto& m : db.all_movies())
        movies_in_queue[m.queue_id].insert(identity(m.tmdb_id, m.imdb_id, m.title));
    std::map<long long, long long> movie_map;
    int movies_added = 0, movies_skipped = 0;
    for (auto row : by_sort_order(rows_of(tables, "movies"))) {
        long long qid = local_queue(row);
        auto key = identity(id_of(row, "tmdb_id"), str_of(row, "imdb_id"), str_of(row, "title"));
        if (!movies_in_queue[qid].insert(key).second) { ++movies_skipped; continue; }
        long long old_id = id_of(row, "id");
        row.erase("id");
        row["queue_id"]   = qid;
        row["sort_order"] = db.max_movie_sort_order(static_cast<int>(qid)) + 1;
        movie_map[old_id] = db.insert_row("movies", row);
        ++movies_added;
    }

    // Child rows follow their newly added parent; rows for skipped
    // (already present) shows/movies are dropped so existing progress stands.
    auto remap_children = [&](const json& section, const std::string& table,
                              const char* fk, const std::map<long long, long long>& map,
                              const char* verb) {
        int n = 0;
        for (auto row : rows_of(section, table)) {
            auto it = map.find(id_of(row, fk));
            if (it == map.end()) continue;
            row[fk] = it->second;
            if (db.insert_row(table, row, verb)) ++n;
        }
        return n;
    };
    counts["episode_watches"] = remap_children(tables, "episode_watches", "show_id", show_map, "INSERT OR IGNORE");

    if (caches) {
        const json& c = b["caches"];
        for (const auto& t : {"people", "episode_imdb_ids", "tmdb_episodes", "tmdb_seasons"}) {
            int n = 0;
            for (const auto& row : rows_of(c, t))
                if (db.insert_row(t, row, "INSERT OR IGNORE")) ++n;
            counts[t] = n;
        }
        counts["show_cast"]    = remap_children(c, "show_cast",    "show_id",  show_map,  "INSERT OR IGNORE");
        counts["episode_cast"] = remap_children(c, "episode_cast", "show_id",  show_map,  "INSERT OR IGNORE");
        counts["movie_cast"]   = remap_children(c, "movie_cast",   "movie_id", movie_map, "INSERT OR IGNORE");
    }

    db.prune_groups();   // a group whose other members were skipped is just a show
    counts["queues"]         = queues_added;
    counts["shows"]          = shows_added;
    counts["shows_skipped"]  = shows_skipped;
    counts["movies"]         = movies_added;
    counts["movies_skipped"] = movies_skipped;
    return counts;
}

} // namespace

// Drop rows whose `col` is in `ids`
static void drop_rows(json& rows, const char* col, const std::set<long long>& ids) {
    json kept = json::array();
    for (auto& r : rows)
        if (!ids.count(id_of(r, col))) kept.push_back(std::move(r));
    rows = std::move(kept);
}

json export_all(Database& db, const Settings::Effective& eff, bool include_caches,
                const std::set<int>& exclude_queues) {
    json settings = json::object();
    if (!eff.tmdb_bearer_token.empty()) settings[Settings::KEY_BEARER]  = eff.tmdb_bearer_token;
    if (!eff.tmdb_api_key.empty())      settings[Settings::KEY_API_KEY] = eff.tmdb_api_key;
    // Port only if the user chose one in Settings — a port that came from the
    // command line or this machine's config file isn't portable.
    try {
        int p = std::stoi(db.get_setting(Settings::KEY_PORT));
        if (p > 0) settings[Settings::KEY_PORT] = p;
    } catch (...) {}

    json tables = json::object();
    for (const auto& t : CONTENT_TABLES) tables[t] = db.dump_table(t);

    // Leave out excluded queues and everything that belongs to them
    json excluded = json::array();
    std::set<long long> gone_shows, gone_movies, gone_tmdb, kept_tmdb, kept_groups;
    if (!exclude_queues.empty()) {
        for (const auto& q : tables["queues"])
            if (exclude_queues.count(static_cast<int>(id_of(q, "id"))))
                excluded.push_back({{"id", id_of(q, "id")}, {"name", str_of(q, "name")}});
        for (const auto& s : tables["shows"]) {
            long long tmdb = id_of(s, "tmdb_id");
            if (exclude_queues.count(static_cast<int>(id_of(s, "queue_id")))) {
                gone_shows.insert(id_of(s, "id"));
                if (tmdb > 0) gone_tmdb.insert(tmdb);
            } else {
                kept_groups.insert(id_of(s, "group_id"));   // groups with a member left
                if (tmdb > 0) kept_tmdb.insert(tmdb);
            }
        }
        for (const auto& m : tables["movies"])
            if (exclude_queues.count(static_cast<int>(id_of(m, "queue_id"))))
                gone_movies.insert(id_of(m, "id"));
        // TMDB ids still used by an included show stay in the caches
        for (auto id : kept_tmdb) gone_tmdb.erase(id);

        std::set<long long> gone_queue_ids(exclude_queues.begin(), exclude_queues.end());
        drop_rows(tables["queues"],          "id",       gone_queue_ids);
        drop_rows(tables["shows"],           "id",       gone_shows);
        drop_rows(tables["movies"],          "id",       gone_movies);
        drop_rows(tables["episode_watches"], "show_id",  gone_shows);
        json groups = json::array();
        for (auto& g : tables["show_groups"])
            if (kept_groups.count(id_of(g, "id"))) groups.push_back(std::move(g));
        tables["show_groups"] = std::move(groups);
    }

    json b = {
        {"format",         FORMAT_NAME},
        {"format_version", FORMAT_VERSION},
        {"app_version",    APP_VERSION},
        {"created",        utc_now_iso()},
        {"settings",       settings},
        {"tables",         tables},
    };
    if (include_caches) {
        json caches = json::object();
        for (const auto& t : CACHE_TABLES) caches[t] = db.dump_table(t);
        if (!exclude_queues.empty()) {
            drop_rows(caches["show_cast"],        "show_id",      gone_shows);
            drop_rows(caches["episode_cast"],     "show_id",      gone_shows);
            drop_rows(caches["movie_cast"],       "movie_id",     gone_movies);
            drop_rows(caches["episode_imdb_ids"], "tmdb_show_id", gone_tmdb);
            drop_rows(caches["tmdb_episodes"],    "tmdb_show_id", gone_tmdb);
            drop_rows(caches["tmdb_seasons"],     "tmdb_show_id", gone_tmdb);
        }
        b["caches"] = caches;
    }
    if (!excluded.empty()) b["excluded_queues"] = excluded;
    return b;
}

json restore(Database& db, const json& backup, Mode mode, const Settings::Effective& current) {
    validate(backup);
    db.begin();
    try {
        json result = {
            {"mode",   mode == Mode::Replace ? "replace" : "merge"},
            {"counts", mode == Mode::Replace ? restore_replace(db, backup)
                                             : restore_merge(db, backup)},
        };
        result["settings_applied"] =
            apply_settings(db, backup.value("settings", json::object()), mode, current);
        db.hash_plain_pins();   // backups from before PIN hashing hold plain text
        db.commit();
        return result;
    } catch (...) {
        db.rollback();
        throw;
    }
}

} // namespace FlickImp::Backup

// SN: 00006
