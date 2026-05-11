// Unit coverage for the WsServer Origin allowlist hardening.
//
// Drives an in-process WsServer on a loopback port through one full
// upgrade attempt per case: with an empty allowlist (the dev default;
// any Origin accepted, including a missing one) and with a non-empty
// allowlist (only the listed Origin accepted; missing or mismatched
// rejected with HTTP 403 and a counter bump).
//
// The test owns the server lifecycle: WsServer is constructed in the
// test process, serve_forever() runs on a helper thread, and
// request_stop() unblocks it at teardown.

#include "meridian/ws_server.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

constexpr const char* kAllowedOrigin = "https://meridian-orderbook.pages.dev";
constexpr const char* kBlockedOrigin = "https://attacker.example.com";

int connect_loopback(int port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int yes = 1;
    (void)::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd); return -1;
    }
    return fd;
}

bool send_all(int fd, const std::string& s) {
    std::size_t off = 0;
    while (off < s.size()) {
        const ssize_t n = ::send(fd, s.data() + off, s.size() - off, 0);
        if (n <= 0) return false;
        off += static_cast<std::size_t>(n);
    }
    return true;
}

// Read until either CRLFCRLF is observed or the deadline elapses; the
// resulting prefix is enough to decode the HTTP status line on a small
// response (101 Switching Protocols, 400 Bad Request, 403 Forbidden).
std::string read_response(int fd, int timeout_ms) {
    std::string out;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd p{fd, POLLIN, 0};
        const int rc = ::poll(&p, 1, 10);
        if (rc <= 0) continue;
        char buf[1024];
        const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        out.append(buf, static_cast<std::size_t>(n));
        if (out.find("\r\n\r\n") != std::string::npos) break;
    }
    return out;
}

std::string upgrade_request(int port, const char* origin_header) {
    std::string req =
        "GET /ws HTTP/1.1\r\n"
        "Host: 127.0.0.1:" + std::to_string(port) + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n";
    if (origin_header != nullptr) {
        req += "Origin: ";
        req += origin_header;
        req += "\r\n";
    }
    req += "\r\n";
    return req;
}

class ServerHarness {
public:
    explicit ServerHarness(std::vector<std::string> allowed)
        : server_(0) {
        server_.set_allowed_origins(std::move(allowed));
        thread_ = std::thread([this] { server_.serve_forever(); });
        // The poll loop sets up the listen fd in the constructor; the
        // serve_forever thread can start immediately. A short yield
        // gives the OS a chance to schedule it before the first connect.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ~ServerHarness() {
        server_.request_stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] int port() const noexcept { return server_.assigned_port(); }
    [[nodiscard]] const meridian::ws::WsServer& server() const noexcept { return server_; }

private:
    meridian::ws::WsServer server_;
    std::thread thread_;
};

// Wait briefly for the server's metrics to reflect an upgrade attempt
// the test has just driven. The server processes the client request on
// its serve_forever() thread on the next poll wakeup (33 ms timeout),
// so the metric bump lags the test by up to one poll tick.
bool wait_for_handshake_failures_at_least(
    const meridian::ws::WsServer& s, std::uint64_t want, int timeout_ms) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (s.metrics().handshake_failures >= want) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return s.metrics().handshake_failures >= want;
}

}  // namespace

TEST(WsServerOrigin, EmptyAllowlistAcceptsAnyOrigin) {
    ServerHarness h({});
    ASSERT_GT(h.port(), 0);

    const int fd = connect_loopback(h.port());
    ASSERT_GE(fd, 0);
    ASSERT_TRUE(send_all(fd, upgrade_request(h.port(), kBlockedOrigin)));
    const std::string resp = read_response(fd, 1000);
    EXPECT_NE(resp.find("HTTP/1.1 101"), std::string::npos)
        << "expected 101 Switching Protocols, got: " << resp;
    ::close(fd);
}

TEST(WsServerOrigin, EmptyAllowlistAcceptsMissingOrigin) {
    ServerHarness h({});
    ASSERT_GT(h.port(), 0);

    const int fd = connect_loopback(h.port());
    ASSERT_GE(fd, 0);
    ASSERT_TRUE(send_all(fd, upgrade_request(h.port(), nullptr)));
    const std::string resp = read_response(fd, 1000);
    EXPECT_NE(resp.find("HTTP/1.1 101"), std::string::npos)
        << "expected 101 Switching Protocols, got: " << resp;
    ::close(fd);
}

TEST(WsServerOrigin, NonEmptyAllowlistAcceptsListedOrigin) {
    ServerHarness h({kAllowedOrigin});
    ASSERT_GT(h.port(), 0);

    const int fd = connect_loopback(h.port());
    ASSERT_GE(fd, 0);
    ASSERT_TRUE(send_all(fd, upgrade_request(h.port(), kAllowedOrigin)));
    const std::string resp = read_response(fd, 1000);
    EXPECT_NE(resp.find("HTTP/1.1 101"), std::string::npos)
        << "expected 101 Switching Protocols, got: " << resp;
    ::close(fd);
}

TEST(WsServerOrigin, NonEmptyAllowlistRejectsUnlistedOrigin) {
    ServerHarness h({kAllowedOrigin});
    ASSERT_GT(h.port(), 0);

    const int fd = connect_loopback(h.port());
    ASSERT_GE(fd, 0);
    ASSERT_TRUE(send_all(fd, upgrade_request(h.port(), kBlockedOrigin)));
    const std::string resp = read_response(fd, 1000);
    EXPECT_NE(resp.find("HTTP/1.1 403"), std::string::npos)
        << "expected 403 Forbidden, got: " << resp;
    ::close(fd);

    EXPECT_TRUE(wait_for_handshake_failures_at_least(h.server(), 1, 500));
    EXPECT_GE(h.server().metrics().origin_rejects, 1u);
}

TEST(WsServerOrigin, NonEmptyAllowlistRejectsMissingOrigin) {
    ServerHarness h({kAllowedOrigin});
    ASSERT_GT(h.port(), 0);

    const int fd = connect_loopback(h.port());
    ASSERT_GE(fd, 0);
    ASSERT_TRUE(send_all(fd, upgrade_request(h.port(), nullptr)));
    const std::string resp = read_response(fd, 1000);
    EXPECT_NE(resp.find("HTTP/1.1 403"), std::string::npos)
        << "expected 403 Forbidden, got: " << resp;
    ::close(fd);

    EXPECT_TRUE(wait_for_handshake_failures_at_least(h.server(), 1, 500));
    EXPECT_GE(h.server().metrics().origin_rejects, 1u);
}

TEST(WsServerOrigin, MultipleAllowedOriginsAreEachAccepted) {
    ServerHarness h({kAllowedOrigin, "http://localhost:5173"});
    ASSERT_GT(h.port(), 0);

    for (const char* origin : {kAllowedOrigin, "http://localhost:5173"}) {
        const int fd = connect_loopback(h.port());
        ASSERT_GE(fd, 0) << "failed to connect for origin: " << origin;
        ASSERT_TRUE(send_all(fd, upgrade_request(h.port(), origin)));
        const std::string resp = read_response(fd, 1000);
        EXPECT_NE(resp.find("HTTP/1.1 101"), std::string::npos)
            << "expected 101 for origin " << origin << ", got: " << resp;
        ::close(fd);
    }
}
