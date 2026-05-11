// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "scraper.hpp"
#include <chrono>
#include <ctime>
#include <curl/curl.h>
#include <iostream>
#include <json.hpp>
#include <string>

using json = nlohmann::json;

namespace FlickImp::Scraper {

namespace {

// ---------- HTTP fetch ---------------------------------------------------

size_t write_cb(char* ptr, size_t size, size_t nmemb, std::string* out) {
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string fetch_url(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &body);
    // IMDB requires a real-ish UA; also request English to avoid locale surprises
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, []{
        static curl_slist* hdrs = nullptr;
        if (!hdrs) {
            hdrs = curl_slist_append(hdrs, "Accept-Language: en-US,en;q=0.9");
            hdrs = curl_slist_append(hdrs, "Accept: text/html,application/xhtml+xml");
        }
        return hdrs;
    }());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        20L);
    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK) return {};
    return body;
}

// ---------- HTML extraction helpers --------------------------------------

// Extract the content of a <script id="id" ...>...</script> tag.
std::string extract_script_by_id(const std::string& html, const std::string& id) {
    std::string needle = "id=\"" + id + "\"";
    size_t pos = html.find(needle);
    if (pos == std::string::npos) return {};
    pos = html.find('>', pos);
    if (pos == std::string::npos) return {};
    size_t start = pos + 1;
    size_t end = html.find("</script>", start);
    if (end == std::string::npos) return {};
    return html.substr(start, end - start);
}

// Extract the first <script type="application/ld+json">...</script> block.
std::string extract_json_ld(const std::string& html) {
    const char* marker = "application/ld+json\">";
    size_t pos = html.find(marker);
    if (pos == std::string::npos) return {};
    size_t start = pos + std::strlen(marker);
    size_t end = html.find("</script>", start);
    if (end == std::string::npos) return {};
    return html.substr(start, end - start);
}

// ---------- Date helpers -------------------------------------------------

std::string today_iso() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);
    char buf[11];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
    return buf;
}

// Build "YYYY-MM-DD" from IMDB releaseDate object {"day":N,"month":N,"year":N}.
// Returns "" if any field is missing/zero.
std::string make_iso_date(const json& rd) {
    try {
        int y = rd.at("year").get<int>();
        int m = rd.at("month").get<int>();
        int d = rd.at("day").get<int>();
        if (y <= 0 || m <= 0 || d <= 0) return {};
        char buf[11];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
        return buf;
    } catch (...) { return {}; }
}

// ---------- Parse basic show info from JSON-LD ---------------------------

struct BasicInfo {
    std::string type;   // "TVSeries", "Movie", etc.
    std::string title;
    int         total_seasons{0};
    int         total_episodes{0};
};

std::optional<BasicInfo> parse_json_ld(const std::string& html) {
    std::string raw = extract_json_ld(html);
    if (raw.empty()) return std::nullopt;
    try {
        auto j = json::parse(raw);
        BasicInfo info;
        info.type  = j.value("@type", "");
        info.title = j.value("name", "");
        // numberOfSeasons / numberOfEpisodes may be int or string in the wild
        if (j.contains("numberOfSeasons")) {
            auto& v = j["numberOfSeasons"];
            info.total_seasons = v.is_number() ? v.get<int>() : std::stoi(v.get<std::string>());
        }
        if (j.contains("numberOfEpisodes")) {
            auto& v = j["numberOfEpisodes"];
            info.total_episodes = v.is_number() ? v.get<int>() : std::stoi(v.get<std::string>());
        }
        return info;
    } catch (...) { return std::nullopt; }
}

// ---------- Parse latest aired episode from __NEXT_DATA__ ----------------

// IMDB embeds episode data in __NEXT_DATA__ under several possible paths.
// We try each known path and return the first that yields episode items.
static const json* try_path(const json& root, std::initializer_list<const char*> keys) {
    const json* cur = &root;
    for (const char* k : keys) {
        if (!cur->is_object() || !cur->contains(k)) return nullptr;
        cur = &(*cur)[k];
    }
    return cur;
}

EpisodeInfo parse_latest_aired(const std::string& html) {
    std::string raw = extract_script_by_id(html, "__NEXT_DATA__");
    if (raw.empty()) return {};

    json root;
    try { root = json::parse(raw); } catch (...) { return {}; }

    // Try the known paths to the episode items array
    const json* items = nullptr;
    const json* pp = try_path(root, {"props", "pageProps"});
    if (pp) {
        // Path 1 (current IMDB as of 2024-25):
        //   pageProps.contentData.section.episodes.items
        if (auto* p = try_path(*pp, {"contentData","section","episodes","items"}))
            if (p->is_array()) items = p;

        // Path 2: pageProps.episodes.items
        if (!items)
            if (auto* p = try_path(*pp, {"episodes","items"}))
                if (p->is_array()) items = p;
    }

    if (!items || items->empty()) return {};

    std::string today = today_iso();
    EpisodeInfo best;

    for (const auto& ep : *items) {
        try {
            int s = 0, e = 0;
            // season/episode may be int or string
            if (ep.contains("season")) {
                auto& sv = ep["season"];
                s = sv.is_number() ? sv.get<int>() : std::stoi(sv.get<std::string>());
            }
            if (ep.contains("episode")) {
                auto& ev = ep["episode"];
                e = ev.is_number() ? ev.get<int>() : std::stoi(ev.get<std::string>());
            }
            if (s <= 0 || e <= 0) continue;

            // Air date
            std::string air_date;
            if (ep.contains("releaseDate") && ep["releaseDate"].is_object())
                air_date = make_iso_date(ep["releaseDate"]);

            // Only count episodes that have actually aired
            if (air_date.empty() || air_date > today) continue;

            // Title
            std::string title;
            if (auto* t = try_path(ep, {"titleText","text"}))
                if (t->is_string()) title = t->get<std::string>();

            // Keep the latest by (season, episode)
            if (s > best.season || (s == best.season && e > best.episode)) {
                best.season   = s;
                best.episode  = e;
                best.title    = title;
                best.air_date = air_date;
            }
        } catch (...) { continue; }
    }

    return best;
}

} // namespace

// ---------- Public API ---------------------------------------------------

std::optional<ShowInfo> fetch_show_info(const std::string& imdb_id) {
    if (imdb_id.empty()) return std::nullopt;

    // 1. Fetch main title page for basic info
    std::string title_url = "https://www.imdb.com/title/" + imdb_id + "/";
    std::string title_html = fetch_url(title_url);
    if (title_html.empty()) {
        std::cerr << "    [scraper] Failed to fetch " << title_url << "\n";
        return std::nullopt;
    }

    auto basic = parse_json_ld(title_html);
    if (!basic) {
        std::cerr << "    [scraper] Could not parse JSON-LD from " << title_url << "\n";
        return std::nullopt;
    }

    // Only handle TV series
    if (basic->type != "TVSeries") {
        std::cerr << "    [scraper] " << imdb_id << " is " << basic->type
                  << ", not a TVSeries\n";
        return std::nullopt;
    }

    ShowInfo info;
    info.title          = basic->title;
    info.total_seasons  = basic->total_seasons;
    info.total_episodes = basic->total_episodes;

    // 2. Fetch latest season's episode list for aired-episode detection.
    //    IMDB defaults to the current season when no season= param is given.
    std::string ep_url = "https://www.imdb.com/title/" + imdb_id + "/episodes/";
    std::string ep_html = fetch_url(ep_url);
    if (!ep_html.empty()) {
        info.latest_aired = parse_latest_aired(ep_html);

        // If the latest season returned no aired episodes yet (e.g. announced
        // but not started), try the previous season.
        if (info.latest_aired.season == 0 && info.total_seasons > 1) {
            int prev = info.total_seasons - 1;
            ep_url  = "https://www.imdb.com/title/" + imdb_id
                    + "/episodes/?season=" + std::to_string(prev);
            ep_html = fetch_url(ep_url);
            if (!ep_html.empty())
                info.latest_aired = parse_latest_aired(ep_html);
        }
    } else {
        std::cerr << "    [scraper] Failed to fetch episode list for " << imdb_id << "\n";
    }

    return info;
}

} // namespace FlickImp::Scraper

// SN: 00001
