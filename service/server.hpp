// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include "../lib/config.hpp"
#include <string>

namespace FlickImp {

struct ServerOptions {
    std::string db_path;
    std::string web_root;
    int         port{8647};       // port to listen on (already resolved)
    int         cli_port{0};      // > 0 if --port was given; locks the Settings port field
    Config      file_cfg;         // fi_config.json as loaded, for settings resolution
};

class Server {
public:
    explicit Server(const ServerOptions& opts);
    ~Server();

    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    void run();   // blocks until SIGINT/SIGTERM

private:
    struct Impl;
    Impl* impl_;
};

} // namespace FlickImp

// SN: 00006
