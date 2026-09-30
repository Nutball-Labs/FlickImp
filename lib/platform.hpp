// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include <string>

namespace FlickImp::Platform {

std::string config_dir();  // ~/.config/flickimp/
std::string data_dir();    // ~/.local/share/flickimp/
std::string db_path();     // data_dir() + "flickimp.db"
std::string exe_dir();     // directory containing the running executable

} // namespace FlickImp::Platform

// SN: 00005
