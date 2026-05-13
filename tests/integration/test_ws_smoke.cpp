// Smoke test for meridian-server: spawn the binary on port 0, parse
// the announced port, open a TCP connection, perform the RFC 6455
// upgrade handshake, decode the first text frame (must be a snapshot),
// then wait for at least one delta frame within a short timeout.
//
// Pure C++; no Python websockets dep, no curl shell-out. The test
// owns the server lifecycle: fork + exec at SetUp, SIGTERM + waitpid
// at TearDown. Server stdout is piped back through a pipe so the test
// can read the "port N\n" announcement.

#include "meridian/ws_handshake.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

class ServerProc {
public:
    bool start(const char* binary, const char* rate) {
        int pipefd[2];
        if (::pipe(pipefd) < 0) return false;
        pid_ = ::fork();
        if (pid_ < 0) {
            ::close(pipefd[0]); ::close(pipefd[1]);
            return false;
        }
        if (pid_ == 0) {
            // Child.
            ::close(pipefd[0]);
            ::dup2(pipefd[1], STDOUT_FILENO);
            ::close(pipefd[1]);
            ::execl(binary, binary, "--port", "0", "--rate", rate, nullptr);
            std::_Exit(127);
        }
        // Parent.
        ::close(pipefd[1]);
        stdout_fd_ = pipefd[0];
        // Read the first line from the child's stdout: "port N\n".
        std::string line;
        char ch = 0;
        for (int i = 0; i < 256; ++i) {
            const ssize_t n = ::read(stdout_fd_, &ch, 1);
            if (n != 1) return false;
            if (ch == '\n') break;
            line.push_back(ch);
        }
        if (line.rfind("port ", 0) != 0) return false;
        port_ = std::atoi(line.c_str() + 5);
        return port_ > 0;
    }

    void stop() {
        if (pid_ > 0) {
            ::kill(pid_, SIGTERM);
            int status = 0;
            ::waitpid(pid_, &status, 0);
            pid_ = -1;
        }
        if (stdout_fd_ >= 0) { ::close(stdout_fd_); stdout_fd_ = -1; }
    }

    [[nodiscard]] int port() const noexcept { return port_; }

private:
    pid_t pid_ = -1;
    int stdout_fd_ = -1;
    int port_ = 0;
};

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

bool send_all(int fd, const std::string& data) {
    std::size_t off = 0;
    while (off < data.size()) {
        const ssize_t n = ::send(fd, data.data() + off, data.size() - off, 0);
        if (n <= 0) return false;
        off += static_cast<std::size_t>(n);
    }
    return true;
}

// Read until the buffer contains "\r\n\r\n", or timeout.
bool read_http_response(int fd, std::string& out, int timeout_ms) {
    out.clear();
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return false;
        const int remaining = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        pollfd p{fd, POLLIN, 0};
        const int rc = ::poll(&p, 1, remaining);
        if (rc <= 0) return false;
        char buf[1024];
        const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        out.append(buf, static_cast<std::size_t>(n));
        if (out.find("\r\n\r\n") != std::string::npos) return true;
    }
}

// Read exactly `want` bytes, accumulating in `buf`. Honors timeout.
bool read_n(int fd, std::string& buf, std::size_t want, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (buf.size() < want) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return false;
        const int remaining = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        pollfd p{fd, POLLIN, 0};
        const int rc = ::poll(&p, 1, remaining);
        if (rc <= 0) return false;
        char chunk[1024];
        const ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) return false;
        buf.append(chunk, static_cast<std::size_t>(n));
    }
    return true;
}

// Decode a single server-to-client text frame. Returns the payload on
// success (no mask bit set per RFC 6455 server-to-client direction);
// returns an empty optional-equivalent (out_payload empty + return
// false) on any decode failure.
bool read_text_frame(int fd, std::string& carry, std::string& out_payload,
                     int timeout_ms) {
    out_payload.clear();
    // Need at least 2 bytes for the frame header.
    if (carry.size() < 2 && !read_n(fd, carry, 2, timeout_ms)) return false;
    const auto b0 = static_cast<std::uint8_t>(carry[0]);
    const auto b1 = static_cast<std::uint8_t>(carry[1]);
    const bool fin = (b0 & 0x80u) != 0;
    const std::uint8_t opcode = b0 & 0x0Fu;
    const bool mask = (b1 & 0x80u) != 0;
    const std::uint8_t len7 = b1 & 0x7Fu;
    if (!fin || opcode != 0x1 || mask) return false;  // we expect FIN text, no mask
    std::size_t header_len = 2;
    std::uint64_t payload_len = len7;
    if (len7 == 126) {
        if (carry.size() < 4 && !read_n(fd, carry, 4, timeout_ms)) return false;
        payload_len = (static_cast<std::uint64_t>(static_cast<std::uint8_t>(carry[2])) << 8)
                    |  static_cast<std::uint64_t>(static_cast<std::uint8_t>(carry[3]));
        header_len = 4;
    } else if (len7 == 127) {
        if (carry.size() < 10 && !read_n(fd, carry, 10, timeout_ms)) return false;
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) |
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(carry[2 + i]));
        }
        header_len = 10;
    }
    if (payload_len > (1u << 20)) return false;  // sanity ceiling
    const std::size_t total = header_len + static_cast<std::size_t>(payload_len);
    if (carry.size() < total && !read_n(fd, carry, total, timeout_ms)) return false;
    out_payload.assign(carry, header_len, static_cast<std::size_t>(payload_len));
    carry.erase(0, total);
    return true;
}

class WsSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Slow rate: the engine thread should not steal CPU from the
        // main test thread on a shared CI runner.
        ASSERT_TRUE(server_.start(MERIDIAN_SERVER_PATH, "1000"))
            << "failed to start meridian-server (or parse port)";
    }
    void TearDown() override { server_.stop(); }

    ServerProc server_;
};

TEST_F(WsSmokeTest, UpgradeAndReceiveSnapshotThenDelta) {
    const int fd = connect_loopback(server_.port());
    ASSERT_GE(fd, 0) << "connect failed";

    // Send a minimal valid WS upgrade request.
    const std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";  // RFC 6455 example
    const std::string req =
        "GET /ws HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + client_key + "\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    ASSERT_TRUE(send_all(fd, req));

    std::string resp;
    ASSERT_TRUE(read_http_response(fd, resp, 1000)) << "no HTTP response";
    EXPECT_NE(resp.find("HTTP/1.1 101"), std::string::npos)
        << "expected 101 Switching Protocols, got:\n" << resp;
    const std::string expected_accept = meridian::ws::compute_accept(client_key);
    EXPECT_NE(resp.find("Sec-WebSocket-Accept: " + expected_accept), std::string::npos)
        << "Sec-WebSocket-Accept missing or wrong:\n" << resp;

    // Strip the headers and any bytes that came after \r\n\r\n; those
    // belong to the first WS frame.
    const auto header_end = resp.find("\r\n\r\n");
    std::string carry = (header_end == std::string::npos)
        ? std::string{}
        : resp.substr(header_end + 4);

    std::string snap_payload;
    ASSERT_TRUE(read_text_frame(fd, carry, snap_payload, 1000))
        << "did not receive a valid first frame";

    // Verify snapshot frame has all four nested sections.
    ASSERT_NE(snap_payload.find("\"kind\":\"snapshot\""), std::string::npos)
        << "expected snapshot frame, got: " << snap_payload;
    ASSERT_NE(snap_payload.find("\"tob\":{"),      std::string::npos);
    ASSERT_NE(snap_payload.find("\"depth\":{"),    std::string::npos);
    ASSERT_NE(snap_payload.find("\"trades\":["),   std::string::npos);
    ASSERT_NE(snap_payload.find("\"latency\":{"),  std::string::npos);
    ASSERT_NE(snap_payload.find("\"hist\":["),     std::string::npos);

    // 33 buckets in latency.hist: 32 commas between 33 numbers.
    {
        const auto hist_pos = snap_payload.find("\"hist\":[");
        ASSERT_NE(hist_pos, std::string::npos);
        const auto hist_end = snap_payload.find(']', hist_pos);
        ASSERT_NE(hist_end, std::string::npos);
        const auto hist_slice = snap_payload.substr(hist_pos, hist_end - hist_pos);
        EXPECT_EQ(std::count(hist_slice.begin(), hist_slice.end(), ','), 32);
    }

    // Wait up to 500 ms for at least one delta frame.
    bool got_delta = false;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(500);
    while (!got_delta && std::chrono::steady_clock::now() < deadline) {
        std::string payload;
        if (!read_text_frame(fd, carry, payload, 200)) break;
        if (payload.find("\"kind\":\"delta\"") != std::string::npos) {
            got_delta = true;
            // Verify delta frame has the same four nested sections.
            EXPECT_NE(payload.find("\"tob\":{"),     std::string::npos);
            EXPECT_NE(payload.find("\"depth\":{"),   std::string::npos);
            EXPECT_NE(payload.find("\"trades\":["),  std::string::npos);
            EXPECT_NE(payload.find("\"latency\":{"), std::string::npos);
            EXPECT_NE(payload.find("\"hist\":["),    std::string::npos);

            // 33 buckets in latency.hist: 32 commas between 33 numbers.
            const auto hist_pos = payload.find("\"hist\":[");
            ASSERT_NE(hist_pos, std::string::npos);
            const auto hist_end = payload.find(']', hist_pos);
            ASSERT_NE(hist_end, std::string::npos);
            const auto hist_slice = payload.substr(hist_pos, hist_end - hist_pos);
            EXPECT_EQ(std::count(hist_slice.begin(), hist_slice.end(), ','), 32);
        }
    }
    EXPECT_TRUE(got_delta) << "no delta frame within 500 ms";

    ::close(fd);
}

TEST_F(WsSmokeTest, HealthzReturnsOk) {
    const int fd = connect_loopback(server_.port());
    ASSERT_GE(fd, 0);
    const std::string req =
        "GET /healthz HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Connection: close\r\n\r\n";
    ASSERT_TRUE(send_all(fd, req));
    std::string resp;
    ASSERT_TRUE(read_http_response(fd, resp, 500));
    EXPECT_NE(resp.find("HTTP/1.1 200"), std::string::npos);
    EXPECT_NE(resp.find("\"status\":\"ok\""), std::string::npos);
    ::close(fd);
}

TEST_F(WsSmokeTest, MetricsReturnsCounters) {
    const int fd = connect_loopback(server_.port());
    ASSERT_GE(fd, 0);
    const std::string req =
        "GET /metrics HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Connection: close\r\n\r\n";
    ASSERT_TRUE(send_all(fd, req));
    std::string resp;
    ASSERT_TRUE(read_http_response(fd, resp, 500));
    EXPECT_NE(resp.find("HTTP/1.1 200"), std::string::npos);
    EXPECT_NE(resp.find("\"connections_total\""), std::string::npos);
    EXPECT_NE(resp.find("\"broadcasts_total\""), std::string::npos);
    ::close(fd);
}

TEST_F(WsSmokeTest, UnknownPathReturns404) {
    const int fd = connect_loopback(server_.port());
    ASSERT_GE(fd, 0);
    const std::string req =
        "GET /does-not-exist HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Connection: close\r\n\r\n";
    ASSERT_TRUE(send_all(fd, req));
    std::string resp;
    ASSERT_TRUE(read_http_response(fd, resp, 500));
    EXPECT_NE(resp.find("HTTP/1.1 404"), std::string::npos);
    ::close(fd);
}

}  // namespace
