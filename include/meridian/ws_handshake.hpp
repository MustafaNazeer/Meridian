#pragma once

// SHA-1 (RFC 3174) and base64 (RFC 4648) helpers, hand rolled,
// header only. Used by the WebSocket upgrade handshake to compute
// Sec-WebSocket-Accept = base64(sha1(client_key + GUID)) per RFC 6455.
//
// Both implementations are documented to handle the small inputs used
// by the WS handshake (a 24-character base64 client key plus a 36-byte
// GUID, 60 bytes total). They are not optimized for high throughput
// and are not used on any latency-sensitive path.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace meridian::ws {

// Magic GUID per RFC 6455 section 1.3. Concatenated to the client's
// Sec-WebSocket-Key, the SHA-1 hash of the result is the value the
// server returns in Sec-WebSocket-Accept.
inline constexpr std::string_view kWebSocketGuid =
    "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

namespace detail {

constexpr std::uint32_t rotl32(std::uint32_t v, int n) noexcept {
    return (v << n) | (v >> (32 - n));
}

inline std::array<std::uint8_t, 20> sha1(const std::uint8_t* data,
                                         std::size_t len) noexcept {
    // Initial hash values per RFC 3174 section 6.1.
    std::uint32_t h0 = 0x67452301u;
    std::uint32_t h1 = 0xEFCDAB89u;
    std::uint32_t h2 = 0x98BADCFEu;
    std::uint32_t h3 = 0x10325476u;
    std::uint32_t h4 = 0xC3D2E1F0u;

    // Build padded message: original bytes + 0x80 + zero pad to
    // (len mod 64) == 56, then 8-byte big-endian bit length.
    const std::uint64_t bit_len = static_cast<std::uint64_t>(len) * 8u;
    const std::size_t padded_len = ((len + 9 + 63) / 64) * 64;
    std::string padded;
    padded.resize(padded_len, '\0');
    for (std::size_t i = 0; i < len; ++i) {
        padded[i] = static_cast<char>(data[i]);
    }
    padded[len] = static_cast<char>(0x80u);
    for (int i = 7; i >= 0; --i) {
        padded[padded_len - 1 - static_cast<std::size_t>(i)] =
            static_cast<char>((bit_len >> (i * 8)) & 0xFFu);
    }

    for (std::size_t chunk = 0; chunk < padded_len; chunk += 64) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            const std::size_t base = chunk + static_cast<std::size_t>(i) * 4;
            w[i] = (static_cast<std::uint32_t>(static_cast<std::uint8_t>(padded[base])) << 24)
                 | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(padded[base + 1])) << 16)
                 | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(padded[base + 2])) << 8)
                 |  static_cast<std::uint32_t>(static_cast<std::uint8_t>(padded[base + 3]));
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        std::uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            const std::uint32_t temp = rotl32(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotl32(b, 30);
            b = a;
            a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<std::uint8_t, 20> out{};
    auto pack = [&](std::uint32_t v, std::size_t off) {
        out[off + 0] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
        out[off + 1] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
        out[off + 2] = static_cast<std::uint8_t>((v >> 8)  & 0xFFu);
        out[off + 3] = static_cast<std::uint8_t>(v & 0xFFu);
    };
    pack(h0, 0); pack(h1, 4); pack(h2, 8); pack(h3, 12); pack(h4, 16);
    return out;
}

inline std::string base64_encode(const std::uint8_t* data,
                                 std::size_t len) noexcept {
    static constexpr char kAlpha[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    for (; i + 3 <= len; i += 3) {
        const std::uint32_t v = (std::uint32_t{data[i]}     << 16)
                              | (std::uint32_t{data[i + 1]} <<  8)
                              |  std::uint32_t{data[i + 2]};
        out.push_back(kAlpha[(v >> 18) & 0x3Fu]);
        out.push_back(kAlpha[(v >> 12) & 0x3Fu]);
        out.push_back(kAlpha[(v >>  6) & 0x3Fu]);
        out.push_back(kAlpha[ v        & 0x3Fu]);
    }
    if (i < len) {
        const std::size_t rem = len - i;
        std::uint32_t v = std::uint32_t{data[i]} << 16;
        if (rem == 2) v |= std::uint32_t{data[i + 1]} << 8;
        out.push_back(kAlpha[(v >> 18) & 0x3Fu]);
        out.push_back(kAlpha[(v >> 12) & 0x3Fu]);
        if (rem == 2) {
            out.push_back(kAlpha[(v >> 6) & 0x3Fu]);
            out.push_back('=');
        } else {
            out.push_back('=');
            out.push_back('=');
        }
    }
    return out;
}

}  // namespace detail

// Compute Sec-WebSocket-Accept for the given Sec-WebSocket-Key.
inline std::string compute_accept(std::string_view client_key) {
    std::string concat;
    concat.reserve(client_key.size() + kWebSocketGuid.size());
    concat.append(client_key);
    concat.append(kWebSocketGuid);
    const auto hash = detail::sha1(
        reinterpret_cast<const std::uint8_t*>(concat.data()), concat.size());
    return detail::base64_encode(hash.data(), hash.size());
}

}  // namespace meridian::ws
