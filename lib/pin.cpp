// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "pin.hpp"
#include <array>
#include <cstdint>
#include <random>

namespace FlickImp::Pin {

namespace {

const char* PREFIX = "sha256$";

// ---------- SHA-256 (FIPS 180-4) ------------------------------------------

constexpr std::array<uint32_t, 64> K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void compress(std::array<uint32_t, 8>& h, const uint8_t* block) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) |
               (uint32_t(block[i * 4 + 2]) << 8) | uint32_t(block[i * 4 + 3]);
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

std::string to_hex(const uint8_t* p, std::size_t n) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        out += digits[p[i] >> 4];
        out += digits[p[i] & 0x0f];
    }
    return out;
}

// Compare without an early exit, so timing doesn't reveal how much matched
bool equal_constant_time(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

} // namespace

std::string sha256_hex(const std::string& data) {
    std::array<uint32_t, 8> h = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
    const std::size_t len = data.size();

    std::size_t i = 0;
    for (; i + 64 <= len; i += 64) compress(h, bytes + i);

    // Final block(s): remaining bytes, 0x80, zero pad, 64-bit big-endian bit length
    uint8_t tail[128] = {};
    std::size_t rem = len - i;
    for (std::size_t k = 0; k < rem; ++k) tail[k] = bytes[i + k];
    tail[rem] = 0x80;
    std::size_t tail_len = (rem < 56) ? 64 : 128;
    uint64_t bits = static_cast<uint64_t>(len) * 8;
    for (int k = 0; k < 8; ++k)
        tail[tail_len - 1 - k] = static_cast<uint8_t>(bits >> (8 * k));
    compress(h, tail);
    if (tail_len == 128) compress(h, tail + 64);

    uint8_t digest[32];
    for (int k = 0; k < 8; ++k) {
        digest[k * 4]     = static_cast<uint8_t>(h[k] >> 24);
        digest[k * 4 + 1] = static_cast<uint8_t>(h[k] >> 16);
        digest[k * 4 + 2] = static_cast<uint8_t>(h[k] >> 8);
        digest[k * 4 + 3] = static_cast<uint8_t>(h[k]);
    }
    return to_hex(digest, 32);
}

std::string random_hex(std::size_t bytes) {
    std::random_device rd;
    std::string raw(bytes, '\0');
    for (auto& c : raw) c = static_cast<char>(rd() & 0xff);
    return to_hex(reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
}

std::string hash(const std::string& pin) {
    std::string salt = random_hex(16);
    return PREFIX + salt + "$" + sha256_hex(salt + pin);
}

bool is_hashed(const std::string& stored) {
    return stored.rfind(PREFIX, 0) == 0;
}

bool verify(const std::string& pin, const std::string& stored) {
    if (stored.empty()) return true;                       // no PIN set
    if (!is_hashed(stored)) return equal_constant_time(pin, stored);  // legacy plain text
    std::string rest = stored.substr(std::string(PREFIX).size());
    auto dollar = rest.find('$');
    if (dollar == std::string::npos) return false;
    std::string salt = rest.substr(0, dollar);
    return equal_constant_time(sha256_hex(salt + pin), rest.substr(dollar + 1));
}

} // namespace FlickImp::Pin

// SN: 00006
