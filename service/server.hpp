// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include <string>

namespace FlickImp {

class Server {
public:
    Server(const std::string& db_path,
           const std::string& web_root,
           int port = 8080);
    ~Server();

    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    void run();   // blocks until SIGINT/SIGTERM

private:
    struct Impl;
    Impl* impl_;
};

} // namespace FlickImp

// SN: 00001
