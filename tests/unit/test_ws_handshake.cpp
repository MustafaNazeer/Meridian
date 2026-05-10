// Unit tests for the WS handshake helpers.
//
// SHA-1 vectors come from RFC 3174 appendix A. base64 vectors come
// from RFC 4648 section 10. The compute_accept end-to-end check uses
// the RFC 6455 section 1.3 worked example: the client key
// "dGhlIHNhbXBsZSBub25jZQ==" must produce the accept value
// "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=".

#include "meridian/ws_handshake.hpp"

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <string>

namespace meridian::ws {
namespace {

std::string hex(const std::array<std::uint8_t, 20>& h) {
    static constexpr char nib[] = "0123456789abcdef";
    std::string s(40, '0');
    for (std::size_t i = 0; i < 20; ++i) {
        s[i * 2 + 0] = nib[(h[i] >> 4) & 0xFu];
        s[i * 2 + 1] = nib[h[i] & 0xFu];
    }
    return s;
}

TEST(Sha1, EmptyString) {
    auto h = detail::sha1(nullptr, 0);
    EXPECT_EQ(hex(h), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST(Sha1, AbcVectorRfc3174) {
    const char* msg = "abc";
    auto h = detail::sha1(reinterpret_cast<const std::uint8_t*>(msg), 3);
    EXPECT_EQ(hex(h), "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST(Sha1, MultiBlockVectorRfc3174) {
    const char* msg =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    auto h = detail::sha1(reinterpret_cast<const std::uint8_t*>(msg), 56);
    EXPECT_EQ(hex(h), "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

TEST(Base64, EmptyString) {
    EXPECT_EQ(detail::base64_encode(nullptr, 0), "");
}

TEST(Base64, RfcVectors) {
    auto enc = [](const char* s) {
        return detail::base64_encode(
            reinterpret_cast<const std::uint8_t*>(s), std::strlen(s));
    };
    EXPECT_EQ(enc("f"),       "Zg==");
    EXPECT_EQ(enc("fo"),      "Zm8=");
    EXPECT_EQ(enc("foo"),     "Zm9v");
    EXPECT_EQ(enc("foob"),    "Zm9vYg==");
    EXPECT_EQ(enc("fooba"),   "Zm9vYmE=");
    EXPECT_EQ(enc("foobar"),  "Zm9vYmFy");
}

TEST(ComputeAccept, Rfc6455Example) {
    // RFC 6455 section 1.3 worked example.
    EXPECT_EQ(compute_accept("dGhlIHNhbXBsZSBub25jZQ=="),
              "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

}  // namespace
}  // namespace meridian::ws
