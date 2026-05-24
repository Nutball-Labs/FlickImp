// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once

#define APP_NAME "FlickImp"

#define VERSION_MAJOR  1
#define VERSION_MINOR  0
#define VERSION_PATCH  0
#define VERSION_BUILD  0
#define VERSION_SUFFIX ""

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#define APP_VERSION STRINGIFY(VERSION_MAJOR) "." \
                    STRINGIFY(VERSION_MINOR) "." \
                    STRINGIFY(VERSION_PATCH) VERSION_SUFFIX

#ifdef _MSC_VER
static const char PROJECT_LICENSE_NOTICE[] =
#else
static const char PROJECT_LICENSE_NOTICE[] __attribute__((used)) =
#endif
    "FlickImp " APP_VERSION "\n"
    "| Copyright (C) 2026 Nutball Labs / Stephen Berg\n"
    "| GNU General Public License v3 or later\n"
    "| https://github.com/Nutball-Labs/FlickImp\n"
    "| If you paid anyone other than Nutball Labs\n"
    "| for this software, you've been ripped off.\n"
    "|\n"
    "|      / \\__\n"
    "|     (    @\\___\n"
    "|     /         O\n"
    "|    /   (_____/\n"
    "|   /_____/   U\n"
    "|\n"
    "|   ... Kali does NOT approve.\n";

// SN: 00004
