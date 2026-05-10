// Unit tests for the ITCH 5.0 parser.
//
// End-to-end behaviour (parser plus engine plus diff against the
// Python reference) is covered by tests/integration/test_itch_replay.cpp.
// These tests focus on the parser's contract in isolation: field
// decoding, length-prefix-based skipping, truncated-frame handling.

#include "meridian/itch.hpp"
#include "meridian/types.hpp"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace meridian::itch {
namespace {

// Hand-rolled big-endian writers, kept inline so the tests do not
// depend on the tape-generator binary. Mirrors the encoder in
// apps/replay/tape_gen.cpp.

void put_be16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v >> 8));
    b.push_back(static_cast<std::uint8_t>(v & 0xFFu));
}
void put_be32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v >> 24));
    b.push_back(static_cast<std::uint8_t>(v >> 16));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
    b.push_back(static_cast<std::uint8_t>(v & 0xFFu));
}
void put_be48(std::vector<std::uint8_t>& b, std::uint64_t v) {
    for (int i = 5; i >= 0; --i) {
        b.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFu));
    }
}
void put_be64(std::vector<std::uint8_t>& b, std::uint64_t v) {
    for (int i = 7; i >= 0; --i) {
        b.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFu));
    }
}

std::vector<std::uint8_t> add_frame(std::uint64_t ts, std::uint64_t ref,
                                    char side, std::uint32_t shares,
                                    const char (&stock)[8],
                                    std::uint32_t price) {
    std::vector<std::uint8_t> b;
    put_be16(b, kAddOrderSize);
    b.push_back('A');
    put_be16(b, 0);
    put_be16(b, 0);
    put_be48(b, ts);
    put_be64(b, ref);
    b.push_back(static_cast<std::uint8_t>(side));
    put_be32(b, shares);
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<std::uint8_t>(stock[i]));
    put_be32(b, price);
    return b;
}

std::vector<std::uint8_t> delete_frame(std::uint64_t ts, std::uint64_t ref) {
    std::vector<std::uint8_t> b;
    put_be16(b, kOrderDeleteSize);
    b.push_back('D');
    put_be16(b, 0);
    put_be16(b, 0);
    put_be48(b, ts);
    put_be64(b, ref);
    return b;
}

std::vector<std::uint8_t> system_frame(std::uint64_t ts, char code) {
    std::vector<std::uint8_t> b;
    put_be16(b, kSystemEventSize);
    b.push_back('S');
    put_be16(b, 0);
    put_be16(b, 0);
    put_be48(b, ts);
    b.push_back(static_cast<std::uint8_t>(code));
    return b;
}

TEST(ItchParser, EmptyBufferIsEndOfStream) {
    Parser p(nullptr, 0);
    ParsedMessage msg{};
    EXPECT_EQ(p.next(msg), ParseStatus::EndOfStream);
}

TEST(ItchParser, AddOrderRoundTripBuyAndSell) {
    constexpr char stock[8] = {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '};
    auto buf = add_frame(123'456'789ULL, 42, 'B', 100, stock, 1'234'567);
    auto sell = add_frame(987'654'321ULL, 43, 'S', 50, stock, 7'654'321);
    buf.insert(buf.end(), sell.begin(), sell.end());

    Parser p(buf.data(), buf.size());
    ParsedMessage msg{};

    ASSERT_EQ(p.next(msg), ParseStatus::Ok);
    EXPECT_EQ(msg.kind, MessageKind::AddOrder);
    EXPECT_EQ(msg.add.ts_ns, 123'456'789);
    EXPECT_EQ(msg.add.order_ref, 42u);
    EXPECT_EQ(msg.add.side, Side::Buy);
    EXPECT_EQ(msg.add.shares, 100);
    EXPECT_EQ(msg.add.price, 1'234'567);
    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(msg.add.stock.data[i], stock[i]);
    }

    ASSERT_EQ(p.next(msg), ParseStatus::Ok);
    EXPECT_EQ(msg.kind, MessageKind::AddOrder);
    EXPECT_EQ(msg.add.side, Side::Sell);
    EXPECT_EQ(msg.add.order_ref, 43u);
    EXPECT_EQ(msg.add.shares, 50);
    EXPECT_EQ(msg.add.price, 7'654'321);

    EXPECT_EQ(p.next(msg), ParseStatus::EndOfStream);
}

TEST(ItchParser, OrderDeleteRoundTrip) {
    auto buf = delete_frame(555ULL, 7);
    Parser p(buf.data(), buf.size());
    ParsedMessage msg{};
    ASSERT_EQ(p.next(msg), ParseStatus::Ok);
    EXPECT_EQ(msg.kind, MessageKind::OrderDelete);
    EXPECT_EQ(msg.del.ts_ns, 555);
    EXPECT_EQ(msg.del.order_ref, 7u);
    EXPECT_EQ(p.next(msg), ParseStatus::EndOfStream);
}

TEST(ItchParser, SystemEventIsRecognizedAndSkipped) {
    auto buf = system_frame(1ULL, 'O');
    Parser p(buf.data(), buf.size());
    ParsedMessage msg{};
    ASSERT_EQ(p.next(msg), ParseStatus::Ok);
    EXPECT_EQ(msg.kind, MessageKind::OtherSkipped);
    EXPECT_EQ(p.next(msg), ParseStatus::EndOfStream);
}

TEST(ItchParser, UnknownTypeIsSkippedByLengthPrefix) {
    // Fabricate a 10-byte body of an unknown type 'Z', wedged between
    // an Add and a Delete. The parser must skip 'Z' cleanly and still
    // decode the surrounding Add and Delete.
    std::vector<std::uint8_t> buf;
    constexpr char stock[8] = {'M', 'S', 'F', 'T', ' ', ' ', ' ', ' '};
    auto a = add_frame(1, 1, 'B', 5, stock, 100);
    buf.insert(buf.end(), a.begin(), a.end());
    put_be16(buf, 10);
    buf.push_back('Z');
    for (int i = 0; i < 9; ++i) buf.push_back(0xCDu);
    auto d = delete_frame(2, 1);
    buf.insert(buf.end(), d.begin(), d.end());

    Parser p(buf.data(), buf.size());
    ParsedMessage msg{};
    ASSERT_EQ(p.next(msg), ParseStatus::Ok);
    EXPECT_EQ(msg.kind, MessageKind::AddOrder);
    EXPECT_EQ(msg.add.order_ref, 1u);

    ASSERT_EQ(p.next(msg), ParseStatus::Ok);
    EXPECT_EQ(msg.kind, MessageKind::OtherSkipped);

    ASSERT_EQ(p.next(msg), ParseStatus::Ok);
    EXPECT_EQ(msg.kind, MessageKind::OrderDelete);
    EXPECT_EQ(msg.del.order_ref, 1u);
    EXPECT_EQ(p.next(msg), ParseStatus::EndOfStream);
}

TEST(ItchParser, TruncatedLengthPrefixReportsTruncated) {
    std::vector<std::uint8_t> buf;
    buf.push_back(0x00);  // single byte of a 2-byte prefix
    Parser p(buf.data(), buf.size());
    ParsedMessage msg{};
    EXPECT_EQ(p.next(msg), ParseStatus::TruncatedFrame);
}

TEST(ItchParser, TruncatedBodyReportsTruncated) {
    std::vector<std::uint8_t> buf;
    put_be16(buf, kAddOrderSize);  // claim a 36-byte body
    buf.push_back('A');             // but provide only the type byte
    Parser p(buf.data(), buf.size());
    ParsedMessage msg{};
    EXPECT_EQ(p.next(msg), ParseStatus::TruncatedFrame);
}

TEST(ItchParser, LengthMismatchOnAddIsRejected) {
    std::vector<std::uint8_t> buf;
    put_be16(buf, 10);  // claim length 10 (wrong for 'A')
    buf.push_back('A');
    for (int i = 0; i < 9; ++i) buf.push_back(0);
    Parser p(buf.data(), buf.size());
    ParsedMessage msg{};
    EXPECT_EQ(p.next(msg), ParseStatus::LengthMismatch);
}

TEST(ItchParser, InvalidSideOnAddIsRejected) {
    constexpr char stock[8] = {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '};
    auto buf = add_frame(1, 1, 'X', 1, stock, 100);  // 'X' is neither B nor S
    Parser p(buf.data(), buf.size());
    ParsedMessage msg{};
    EXPECT_EQ(p.next(msg), ParseStatus::LengthMismatch);
}

TEST(ItchParser, BytesConsumedTracksProgress) {
    auto a = add_frame(1, 1, 'B', 1, {'A','A','P','L',' ',' ',' ',' '}, 100);
    auto d = delete_frame(2, 1);
    std::vector<std::uint8_t> buf;
    buf.insert(buf.end(), a.begin(), a.end());
    buf.insert(buf.end(), d.begin(), d.end());

    Parser p(buf.data(), buf.size());
    EXPECT_EQ(p.bytes_consumed(), 0u);
    ParsedMessage msg{};
    ASSERT_EQ(p.next(msg), ParseStatus::Ok);
    EXPECT_EQ(p.bytes_consumed(), a.size());
    ASSERT_EQ(p.next(msg), ParseStatus::Ok);
    EXPECT_EQ(p.bytes_consumed(), a.size() + d.size());
    EXPECT_EQ(p.bytes_remaining(), 0u);
}

}  // namespace
}  // namespace meridian::itch
