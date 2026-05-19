// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include <optional>
#include <string>
#include <vector>

namespace FlickImp::Scraper {

// Call once at startup. Bearer token is preferred; API key is the v3 fallback.
void init(const std::string& api_key, const std::string& bearer_token = {});

struct EpisodeInfo {
    int         season{0};
    int         episode{0};
    std::string title;
    std::string air_date;  // "YYYY-MM-DD"
};

struct ShowInfo {
    int         tmdb_id{0};
    std::string title;
    std::string image_url;     // full https://image.tmdb.org/... URL
    int         total_seasons{0};
    int         total_episodes{0};
    EpisodeInfo latest_aired;  // season/episode == 0 if not available
};

struct EpisodeEntry {
    int         season{0};
    int         episode{0};
    std::string title;
    std::string episode_url;   // https://www.themoviedb.org/tv/.../episode/...
    std::string air_date;      // "YYYY-MM-DD"
};

struct SeasonSummary {
    int season_number{0};
    int episode_count{0};
};

struct MovieInfo {
    int         tmdb_id{0};
    std::string title;
    std::string release_date;
    std::string image_url;
};

struct SearchResult {
    int         tmdb_id{0};
    std::string title;
    std::string year;        // "2023" or ""
    std::string poster_url;
};

// Fetch show metadata from TMDB using an IMDB tt-number or full IMDB URL.
std::optional<ShowInfo> fetch_show_info(const std::string& imdb_id);

// Fetch the list of seasons with episode counts for a show by TMDB ID.
std::vector<SeasonSummary> fetch_show_seasons(int tmdb_show_id);

// Fetch all episodes for a season. Requires the cached TMDB show ID.
std::vector<EpisodeEntry> fetch_season_episodes(int tmdb_show_id, int season);

// Fetch movie metadata from TMDB using an IMDB tt-number or full IMDB URL.
std::optional<MovieInfo> fetch_movie_info(const std::string& imdb_id);

// Fetch just the poster image URL (one API call). Accepts IMDB ID or URL.
std::string fetch_poster_url(const std::string& imdb_id);

// Reverse-lookup the IMDB ID for a TV show given its TMDB ID.
std::string fetch_imdb_id(int tmdb_id);

// Reverse-lookup the IMDB ID for a movie given its TMDB ID.
std::string fetch_movie_imdb_id(int tmdb_id);

// Fetch movie metadata directly by TMDB movie ID (no IMDB lookup required).
std::optional<MovieInfo> fetch_movie_info_by_tmdb_id(int tmdb_id);

// Search TMDB by title. Returns up to 5 matches.
std::vector<SearchResult> search_shows(const std::string& query);
std::vector<SearchResult> search_movies(const std::string& query);

} // namespace FlickImp::Scraper

// SN: 00003
