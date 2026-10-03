// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include <cstddef>
#include <string>

// Queue PINs: a parental-control speed bump, not real security. PINs are
// stored as "sha256$<salt hex>$<hash hex>" (salted SHA-256), never in plain
// text. Self-contained so the project needs no crypto library.
namespace FlickImp::Pin {

std::string sha256_hex(const std::string& data);

// Random bytes from std::random_device, as lower-case hex
std::string random_hex(std::size_t bytes);

std::string hash(const std::string& pin);                         // new salted hash
bool        is_hashed(const std::string& stored);                 // "sha256$..." form?
bool        verify(const std::string& pin, const std::string& stored);

} // namespace FlickImp::Pin

// SN: 00006
