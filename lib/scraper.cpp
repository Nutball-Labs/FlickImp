// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "scraper.hpp"
#include <algorithm>
#include <cctype>
#include <curl/curl.h>
#include <iostream>
#include <json.hpp>
#include <string>

using json = nlohmann::json;

namespace FlickImp::Scraper {

namespace {

// ---------- State ---------------------------------------------------------

std::string g_api_key;        // TMDB v3 API key (fallback)
std::string g_bearer_token;   // TMDB v4 Bearer token (preferred)

static const std::string TMDB_BASE  = "https://api.themoviedb.org/3";
static const std::string IMG_BASE   = "https://image.tmdb.org/t/p/w185";
static const std::string IMG_SMALL  = "https://image.tmdb.org/t/p/w92";

// ---------- HTTP ----------------------------------------------------------

size_t write_cb(char* ptr, size_t size, size_t nmemb, std::string* out) {
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// g_api_key holds the TMDB v4 Bearer token; sent as Authorization header.
std::string fetch_url(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        20L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "FlickImp libcurl");
    curl_slist* hdrs = nullptr;
    if (!g_bearer_token.empty()) {
        std::string auth = "Authorization: Bearer " + g_bearer_token;
        hdrs = curl_slist_append(hdrs, auth.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    }
    CURLcode rc = curl_easy_perform(curl);
    if (hdrs) curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return (rc == CURLE_OK) ? body : std::string{};
}

// ---------- Helpers -------------------------------------------------------

// Build a TMDB API URL. Bearer token auth uses headers (no key in URL).
// Falls back to ?api_key= if only the v3 key is configured.
std::string api_url(const std::string& path, const std::string& query = {}) {
    std::string url = TMDB_BASE + path;
    if (g_bearer_token.empty() && !g_api_key.empty()) {
        url += "?api_key=" + g_api_key;
        if (!query.empty()) url += "&" + query;
    } else if (!query.empty()) {
        url += "?" + query;
    }
    return url;
}

// Fetch JSON from TMDB and parse it. Returns null json on error.
json tmdb_get(const std::string& path, const std::string& query = {}) {
    std::string resp = fetch_url(api_url(path, query));
    if (resp.empty()) return nullptr;
    try { return json::parse(resp); } catch (...) { return nullptr; }
}

// Strip full IMDB URLs to bare tt-numbers.
// Requires "tt" immediately followed by a digit (avoids matching "https").
std::string extract_tt(const std::string& raw) {
    size_t pos = 0;
    while (pos < raw.size()) {
        pos = raw.find("tt", pos);
        if (pos == std::string::npos) break;
        if (pos + 2 < raw.size() && std::isdigit(static_cast<unsigned char>(raw[pos + 2]))) {
            std::string id = raw.substr(pos);
            auto end = id.find_first_not_of("t0123456789");
            return end == std::string::npos ? id : id.substr(0, end);
        }
        pos += 2;
    }
    return {};
}

// Build poster URL from TMDB poster_path (may be null).
std::string poster_url(const json& j, const std::string& key = "poster_path") {
    if (!j.contains(key) || j[key].is_null()) return {};
    std::string path = j[key].get<std::string>();
    return path.empty() ? std::string{} : IMG_BASE + path;
}

std::string poster_url_small(const json& j, const std::string& key = "poster_path") {
    if (!j.contains(key) || j[key].is_null()) return {};
    std::string path = j[key].get<std::string>();
    return path.empty() ? std::string{} : IMG_SMALL + path;
}

// URL-encode a string for query parameters.
std::string url_encode(const std::string& s) {
    CURL* curl = curl_easy_init();
    if (!curl) return s;
    char* enc = curl_easy_escape(curl, s.c_str(), static_cast<int>(s.size()));
    std::string result = enc ? enc : s;
    if (enc) curl_free(enc);
    curl_easy_cleanup(curl);
    return result;
}

// Extract 4-digit year from "YYYY-MM-DD" dates.
std::string year_from_date(const std::string& date) {
    return date.size() >= 4 ? date.substr(0, 4) : std::string{};
}

// Find the TMDB ID for a TV show given an IMDB ID.
// Returns 0 on failure.
int find_tv_id(const std::string& imdb_id) {
    auto j = tmdb_get("/find/" + imdb_id, "external_source=imdb_id");
    if (j.is_null()) {
        std::cerr << "    [scraper] TMDB: request failed for " << imdb_id << "\n";
        return 0;
    }
    if (!j.contains("tv_results")) {
        std::cerr << "    [scraper] TMDB: unexpected response: " << j.dump() << "\n";
        return 0;
    }
    auto& results = j["tv_results"];
    if (!results.is_array() || results.empty()) return 0;
    return results[0].value("id", 0);
}

// Find the TMDB ID for a movie given an IMDB ID.
int find_movie_id(const std::string& imdb_id) {
    auto j = tmdb_get("/find/" + imdb_id, "external_source=imdb_id");
    if (j.is_null()) {
        std::cerr << "    [scraper] TMDB: request failed for " << imdb_id << "\n";
        return 0;
    }
    if (!j.contains("movie_results")) {
        std::cerr << "    [scraper] TMDB: unexpected response: " << j.dump() << "\n";
        return 0;
    }
    auto& results = j["movie_results"];
    if (!results.is_array() || results.empty()) return 0;
    return results[0].value("id", 0);
}

} // namespace

// ---------- Public API ----------------------------------------------------

void init(const std::string& api_key, const std::string& bearer_token) {
    g_api_key      = api_key;
    g_bearer_token = bearer_token;
}

std::optional<ShowInfo> fetch_show_info(const std::string& imdb_id) {
    if (g_api_key.empty() && g_bearer_token.empty()) {
        std::cerr << "    [scraper] No TMDB credentials configured\n";
        return std::nullopt;
    }
    std::string id = extract_tt(imdb_id);
    if (id.empty()) {
        std::cerr << "    [scraper] Could not extract IMDB ID from: " << imdb_id << "\n";
        return std::nullopt;
    }

    int tmdb_id = find_tv_id(id);
    if (tmdb_id == 0) {
        std::cerr << "    [scraper] TMDB: no TV result for " << id << "\n";
        return std::nullopt;
    }

    auto j = tmdb_get("/tv/" + std::to_string(tmdb_id));
    if (j.is_null()) {
        std::cerr << "    [scraper] TMDB: failed to fetch TV details for id " << tmdb_id << "\n";
        return std::nullopt;
    }

    ShowInfo info;
    info.tmdb_id        = tmdb_id;
    info.title          = j.value("name", "");
    info.image_url      = poster_url(j);
    info.total_seasons  = j.value("number_of_seasons", 0);
    info.total_episodes = j.value("number_of_episodes", 0);

    if (j.contains("last_episode_to_air") && !j["last_episode_to_air"].is_null()) {
        auto& le = j["last_episode_to_air"];
        info.latest_aired.season   = le.value("season_number", 0);
        info.latest_aired.episode  = le.value("episode_number", 0);
        info.latest_aired.title    = le.value("name", "");
        info.latest_aired.air_date = le.value("air_date", "");
    }

    return info;
}

std::vector<EpisodeEntry> fetch_season_episodes(int tmdb_show_id, int season) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_show_id <= 0) return {};

    auto j = tmdb_get("/tv/" + std::to_string(tmdb_show_id)
                    + "/season/" + std::to_string(season));
    if (j.is_null() || !j.contains("episodes")) return {};

    std::vector<EpisodeEntry> result;
    for (const auto& ep : j["episodes"]) {
        EpisodeEntry entry;
        entry.season   = ep.value("season_number", 0);
        entry.episode  = ep.value("episode_number", 0);
        entry.title    = ep.value("name", "");
        entry.air_date = ep.value("air_date", "");
        entry.episode_url =
            "https://www.themoviedb.org/tv/" + std::to_string(tmdb_show_id)
            + "/season/" + std::to_string(season)
            + "/episode/" + std::to_string(entry.episode);
        if (entry.episode > 0)
            result.push_back(std::move(entry));
    }

    std::sort(result.begin(), result.end(),
        [](const EpisodeEntry& a, const EpisodeEntry& b) {
            return a.episode < b.episode;
        });

    return result;
}

std::vector<SeasonSummary> fetch_show_seasons(int tmdb_show_id) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_show_id <= 0) return {};
    auto j = tmdb_get("/tv/" + std::to_string(tmdb_show_id));
    if (j.is_null() || !j.contains("seasons")) return {};
    std::vector<SeasonSummary> result;
    for (const auto& s : j["seasons"]) {
        int sn = s.value("season_number", 0);
        if (sn <= 0) continue;  // skip specials (season 0)
        result.push_back({sn, s.value("episode_count", 0)});
    }
    std::sort(result.begin(), result.end(),
        [](const SeasonSummary& a, const SeasonSummary& b) {
            return a.season_number < b.season_number;
        });
    return result;
}

std::optional<MovieInfo> fetch_movie_info(const std::string& imdb_id) {
    if (g_api_key.empty() && g_bearer_token.empty()) return std::nullopt;
    std::string id = extract_tt(imdb_id);
    if (id.empty()) return std::nullopt;

    int tmdb_id = find_movie_id(id);
    if (tmdb_id == 0) {
        // Fall back: maybe it's a TV movie — try TV results
        tmdb_id = find_tv_id(id);
        if (tmdb_id == 0) return std::nullopt;

        // Get TV details as best approximation
        auto j = tmdb_get("/tv/" + std::to_string(tmdb_id));
        if (j.is_null()) return std::nullopt;
        MovieInfo info;
        info.tmdb_id      = tmdb_id;
        info.title        = j.value("name", "");
        info.image_url    = poster_url(j);
        info.release_date = j.value("first_air_date", "");
        return info;
    }

    auto j = tmdb_get("/movie/" + std::to_string(tmdb_id));
    if (j.is_null()) return std::nullopt;

    MovieInfo info;
    info.tmdb_id      = tmdb_id;
    info.title        = j.value("title", "");
    info.image_url    = poster_url(j);
    info.release_date = j.value("release_date", "");
    return info;
}

std::string fetch_poster_url(const std::string& imdb_id) {
    if (g_api_key.empty() && g_bearer_token.empty()) return {};
    std::string id = extract_tt(imdb_id);
    if (id.empty()) return {};

    // Try TV first, then movie
    int tmdb_id = find_tv_id(id);
    std::string endpoint = tmdb_id > 0
        ? "/tv/"    + std::to_string(tmdb_id)
        : "/movie/" + std::to_string(find_movie_id(id));

    auto j = tmdb_get(endpoint);
    return j.is_null() ? std::string{} : poster_url(j);
}

std::string fetch_imdb_id(int tmdb_id) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_id <= 0) {
        std::cerr << "    [scraper] fetch_imdb_id: skipped (no creds or bad id=" << tmdb_id << ")\n";
        return {};
    }
    auto j = tmdb_get("/tv/" + std::to_string(tmdb_id) + "/external_ids");
    if (j.is_null()) {
        std::cerr << "    [scraper] fetch_imdb_id: request failed for tmdb_id=" << tmdb_id << "\n";
        return {};
    }
    std::string id = (!j.contains("imdb_id") || j["imdb_id"].is_null()) ? "" : j["imdb_id"].get<std::string>();
    if (id.empty())
        std::cerr << "    [scraper] fetch_imdb_id: no imdb_id in TMDB response for tmdb_id=" << tmdb_id << "\n";
    else
        std::cerr << "    [scraper] fetch_imdb_id: tmdb_id=" << tmdb_id << " -> " << id << "\n";
    return id;
}

std::string fetch_movie_imdb_id(int tmdb_id) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_id <= 0) return {};
    auto j = tmdb_get("/movie/" + std::to_string(tmdb_id) + "/external_ids");
    if (j.is_null() || !j.contains("imdb_id") || j["imdb_id"].is_null()) return {};
    return j["imdb_id"].get<std::string>();
}

std::optional<MovieInfo> fetch_movie_info_by_tmdb_id(int tmdb_id) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_id <= 0) return std::nullopt;
    auto j = tmdb_get("/movie/" + std::to_string(tmdb_id));
    if (j.is_null()) return std::nullopt;
    MovieInfo info;
    info.tmdb_id      = tmdb_id;
    info.title        = j.value("title", "");
    info.image_url    = poster_url(j);
    info.release_date = j.value("release_date", "");
    return info;
}

std::vector<SearchResult> search_shows(const std::string& query) {
    if (g_api_key.empty() && g_bearer_token.empty()) return {};
    auto j = tmdb_get("/search/tv",
        "query=" + url_encode(query) + "&language=en-US&page=1");
    if (j.is_null() || !j.contains("results")) return {};
    std::vector<SearchResult> results;
    for (const auto& r : j["results"]) {
        if (results.size() >= 5) break;
        SearchResult sr;
        sr.tmdb_id    = r.value("id", 0);
        sr.title      = r.value("name", "");
        sr.year       = year_from_date(r.value("first_air_date", ""));
        sr.poster_url = poster_url_small(r);
        if (sr.tmdb_id > 0 && !sr.title.empty())
            results.push_back(std::move(sr));
    }
    return results;
}

std::vector<SearchResult> search_movies(const std::string& query) {
    if (g_api_key.empty() && g_bearer_token.empty()) return {};
    auto j = tmdb_get("/search/movie",
        "query=" + url_encode(query) + "&language=en-US&page=1");
    if (j.is_null() || !j.contains("results")) return {};
    std::vector<SearchResult> results;
    for (const auto& r : j["results"]) {
        if (results.size() >= 5) break;
        SearchResult sr;
        sr.tmdb_id    = r.value("id", 0);
        sr.title      = r.value("title", "");
        sr.year       = year_from_date(r.value("release_date", ""));
        sr.poster_url = poster_url_small(r);
        if (sr.tmdb_id > 0 && !sr.title.empty())
            results.push_back(std::move(sr));
    }
    return results;
}

static std::vector<FlickImp::CastMember> parse_cast(const json& j, const std::string& key, int max_cast) {
    std::vector<FlickImp::CastMember> result;
    if (!j.contains(key) || !j[key].is_array()) return result;
    for (const auto& c : j[key]) {
        if ((int)result.size() >= max_cast) break;
        FlickImp::CastMember cm;
        cm.tmdb_person_id = c.value("id", 0);
        cm.name           = c.contains("name")      && !c["name"].is_null()      ? c["name"].get<std::string>()      : "";
        cm.character      = c.contains("character") && !c["character"].is_null() ? c["character"].get<std::string>() : "";
        cm.sort_order     = c.value("order", (int)result.size());
        if (c.contains("profile_path") && !c["profile_path"].is_null()) {
            std::string pp = c.value("profile_path", "");
            if (!pp.empty()) cm.profile_url = IMG_SMALL + pp;
        }
        if (cm.tmdb_person_id > 0 && !cm.name.empty())
            result.push_back(std::move(cm));
    }
    return result;
}

std::vector<FlickImp::CastMember> fetch_episode_cast(int tmdb_show_id, int season, int episode) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_show_id <= 0) return {};
    auto j = tmdb_get("/tv/" + std::to_string(tmdb_show_id)
                    + "/season/" + std::to_string(season)
                    + "/episode/" + std::to_string(episode) + "/credits");
    if (j.is_null()) return {};
    auto cast   = parse_cast(j, "cast",        30);
    auto guests = parse_cast(j, "guest_stars", 20);
    for (auto& g : guests) {
        g.sort_order += 1000;  // guests sorted after regulars
        cast.push_back(std::move(g));
    }
    return cast;
}

std::vector<FlickImp::CastMember> fetch_show_cast(int tmdb_show_id, int max_cast) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_show_id <= 0) return {};
    auto j = tmdb_get("/tv/" + std::to_string(tmdb_show_id) + "/credits");
    return j.is_null() ? std::vector<FlickImp::CastMember>{} : parse_cast(j, "cast", max_cast);
}

std::vector<FlickImp::CastMember> fetch_movie_cast(int tmdb_movie_id, int max_cast) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_movie_id <= 0) return {};
    auto j = tmdb_get("/movie/" + std::to_string(tmdb_movie_id) + "/credits");
    return j.is_null() ? std::vector<FlickImp::CastMember>{} : parse_cast(j, "cast", max_cast);
}

std::string fetch_person_imdb_id(int tmdb_person_id) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_person_id <= 0) return {};
    auto j = tmdb_get("/person/" + std::to_string(tmdb_person_id) + "/external_ids");
    if (j.is_null() || !j.contains("imdb_id") || j["imdb_id"].is_null()) return {};
    return j["imdb_id"].get<std::string>();
}

std::string fetch_episode_imdb_id(int tmdb_show_id, int season, int episode) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_show_id <= 0) return {};
    auto j = tmdb_get("/tv/" + std::to_string(tmdb_show_id)
                    + "/season/" + std::to_string(season)
                    + "/episode/" + std::to_string(episode)
                    + "/external_ids");
    if (j.is_null() || !j.contains("imdb_id") || j["imdb_id"].is_null()) return {};
    return j["imdb_id"].get<std::string>();
}

std::string fetch_episode_title(int tmdb_show_id, int season, int episode) {
    if ((g_api_key.empty() && g_bearer_token.empty()) || tmdb_show_id <= 0) return {};
    auto j = tmdb_get("/tv/" + std::to_string(tmdb_show_id)
                    + "/season/" + std::to_string(season)
                    + "/episode/" + std::to_string(episode));
    if (j.is_null() || !j.contains("name") || j["name"].is_null()) return {};
    return j["name"].get<std::string>();
}

} // namespace FlickImp::Scraper

// SN: 00004
