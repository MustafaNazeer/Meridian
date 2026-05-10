#include "meridian/ws_server.hpp"

#include "meridian/ws_handshake.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace meridian::ws {

namespace {

constexpr int kPollTimeoutMs = 33;             // one sampler tick at 30 Hz
constexpr std::size_t kMaxRequestBytes = 8192; // upper bound on a sane HTTP request
constexpr std::size_t kMaxFramePayload = 1u << 20;  // 1 MiB ceiling on a single text frame

void set_nonblocking(int fd) noexcept {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Encode a single text frame (FIN=1, opcode=0x1, MASK=0). The server
// never masks; only clients are required to mask per RFC 6455.
std::string encode_text_frame(std::string_view payload) {
    std::string out;
    out.reserve(payload.size() + 10);
    out.push_back(static_cast<char>(0x81));  // FIN | opcode TEXT
    const std::size_t n = payload.size();
    if (n < 126) {
        out.push_back(static_cast<char>(n));
    } else if (n < 65536) {
        out.push_back(static_cast<char>(126));
        out.push_back(static_cast<char>((n >> 8) & 0xFFu));
        out.push_back(static_cast<char>(n & 0xFFu));
    } else {
        out.push_back(static_cast<char>(127));
        for (int i = 7; i >= 0; --i) {
            out.push_back(static_cast<char>((n >> (i * 8)) & 0xFFu));
        }
    }
    out.append(payload);
    return out;
}

// Case-insensitive header lookup. Returns trimmed value or empty if
// not present. Headers come in CRLF-separated form per HTTP/1.1.
std::string header_value(std::string_view req, std::string_view name) {
    const std::string lower_req = [&]() {
        std::string s(req);
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }();
    std::string lower_name(name);
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // Match either at the start (first header) or after a CRLF.
    // We do not look at the request line itself, which always has the
    // method on the left side of the first space, never a header name.
    const std::string needle_after_crlf = "\r\n" + lower_name + ":";
    auto pos = lower_req.find(needle_after_crlf);
    if (pos != std::string::npos) {
        pos += needle_after_crlf.size();
    } else {
        // Try matching at byte 0 (no preceding CRLF) for the unusual
        // case where the caller hands in just the headers without a
        // request line. The current call site always has a request
        // line, so this is defensive.
        const std::string needle_at_start = lower_name + ":";
        if (lower_req.compare(0, needle_at_start.size(), needle_at_start) == 0) {
            pos = needle_at_start.size();
        } else {
            return "";
        }
    }
    // Bound the value by either the next CRLF or end-of-string. The
    // last header has no trailing CRLF inside the request slice we
    // were handed (we trimmed at \r\n\r\n in the caller).
    auto end = lower_req.find("\r\n", pos);
    if (end == std::string::npos) end = lower_req.size();
    // Use the original-case region for the value so the returned key
    // is byte-identical to what the client sent.
    std::string v(req.substr(pos, end - pos));
    // Trim leading whitespace.
    std::size_t i = 0;
    while (i < v.size() && (v[i] == ' ' || v[i] == '\t')) ++i;
    return v.substr(i);
}

}  // namespace

WsServer::WsServer(int port) {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::fprintf(stderr, "WsServer: socket(): %s\n", std::strerror(errno));
        return;
    }
    int yes = 1;
    (void)::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::fprintf(stderr, "WsServer: bind(%d): %s\n", port, std::strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }
    socklen_t len = sizeof(addr);
    if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        assigned_port_ = ntohs(addr.sin_port);
    }
    if (::listen(listen_fd_, 64) < 0) {
        std::fprintf(stderr, "WsServer: listen(): %s\n", std::strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }
    set_nonblocking(listen_fd_);
}

WsServer::~WsServer() {
    if (listen_fd_ >= 0) ::close(listen_fd_);
    for (auto& c : clients_) {
        if (c.fd >= 0) ::close(c.fd);
    }
}

void WsServer::broadcast(std::string_view payload) {
    std::lock_guard<std::mutex> g(broadcast_mu_);
    broadcast_queue_.emplace_back(payload);
}

void WsServer::set_snapshot(std::string_view payload) {
    std::lock_guard<std::mutex> g(broadcast_mu_);
    current_snapshot_.assign(payload);
}

WsServer::Metrics WsServer::metrics() const noexcept {
    Metrics m;
    m.connections_total  = connections_total_.load(std::memory_order_relaxed);
    m.connections_active = static_cast<std::uint64_t>(clients_.size());
    m.broadcasts_total   = broadcasts_total_.load(std::memory_order_relaxed);
    m.bytes_sent_total   = bytes_sent_total_.load(std::memory_order_relaxed);
    m.handshake_failures = handshake_failures_.load(std::memory_order_relaxed);
    return m;
}

void WsServer::serve_forever() {
    if (listen_fd_ < 0) return;

    std::vector<pollfd> pfds;
    pfds.reserve(64);

    while (!stopping_.load(std::memory_order_relaxed)) {
        pfds.clear();
        pfds.push_back({listen_fd_, POLLIN, 0});
        for (const auto& c : clients_) {
            short events = POLLIN;
            if (!c.out_buf.empty()) events |= POLLOUT;
            pfds.push_back({c.fd, events, 0});
        }

        const int rc = ::poll(pfds.data(), pfds.size(), kPollTimeoutMs);
        if (rc < 0) {
            if (errno == EINTR) continue;
            std::fprintf(stderr, "WsServer: poll(): %s\n", std::strerror(errno));
            break;
        }

        // Drain the broadcast queue every loop turn so the broadcast
        // hop from sampler thread to /ws clients is bounded by the
        // poll timeout (33 ms in the steady state).
        drain_broadcast_queue();

        if (pfds[0].revents & POLLIN) on_listener_ready();

        // Walk the client pollfds in reverse so close_client()'s
        // swap-and-pop does not invalidate indices we have not visited.
        for (std::size_t i = pfds.size(); i-- > 1;) {
            const std::size_t client_idx = i - 1;
            if (client_idx >= clients_.size()) continue;  // closed under us
            const auto re = pfds[i].revents;
            if (re & (POLLERR | POLLHUP | POLLNVAL)) {
                close_client(client_idx);
                continue;
            }
            if (re & POLLIN)  on_client_readable(client_idx);
            if (client_idx >= clients_.size()) continue;
            if (re & POLLOUT) on_client_writable(client_idx);
        }
    }
}

void WsServer::on_listener_ready() {
    while (true) {
        sockaddr_in cli{};
        socklen_t len = sizeof(cli);
        const int fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&cli), &len);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            if (errno == EINTR) continue;
            std::fprintf(stderr, "WsServer: accept(): %s\n", std::strerror(errno));
            return;
        }
        set_nonblocking(fd);
        // Disable Nagle: small frames at 30 Hz benefit from no
        // coalescing delay. Failure is non-fatal.
        int yes = 1;
        (void)::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        clients_.push_back(Client{});
        clients_.back().fd = fd;
        connections_total_.fetch_add(1, std::memory_order_relaxed);
    }
}

void WsServer::on_client_readable(std::size_t i) {
    Client& c = clients_[i];
    char buf[4096];
    while (true) {
        const ssize_t n = ::recv(c.fd, buf, sizeof(buf), 0);
        if (n > 0) {
            if (c.in_buf.size() + static_cast<std::size_t>(n) > kMaxRequestBytes) {
                handshake_failures_.fetch_add(1, std::memory_order_relaxed);
                close_client(i);
                return;
            }
            c.in_buf.append(buf, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) { close_client(i); return; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        if (errno == EINTR) continue;
        close_client(i);
        return;
    }
    if (!c.upgraded) {
        if (c.in_buf.find("\r\n\r\n") == std::string::npos) return;  // wait for full headers
        if (!handle_http_request(c)) {
            close_client(i);
            return;
        }
    } else {
        // Discard any client-to-server payload; the demo is one way.
        // We do not process control frames either; a well-behaved
        // client that closes will appear via POLLHUP, which closes
        // the connection cleanly above. Drop everything in in_buf.
        c.in_buf.clear();
    }
}

bool WsServer::handle_http_request(Client& c) {
    const auto header_end = c.in_buf.find("\r\n\r\n");
    if (header_end == std::string::npos) return false;
    const std::string req = c.in_buf.substr(0, header_end);
    c.in_buf.erase(0, header_end + 4);

    // Parse request line.
    const auto first_space = req.find(' ');
    const auto second_space = req.find(' ', first_space + 1);
    if (first_space == std::string::npos || second_space == std::string::npos) {
        handshake_failures_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    const std::string method = req.substr(0, first_space);
    const std::string path = req.substr(first_space + 1, second_space - first_space - 1);

    if (method != "GET") {
        const std::string resp =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        c.out_buf.append(resp);
        flush_to_client(c);
        return false;
    }

    if (path == "/healthz") {
        const std::string body = "{\"status\":\"ok\"}\n";
        std::string resp = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(body.size()) + "\r\n"
                           "Connection: close\r\n\r\n" + body;
        c.out_buf.append(resp);
        flush_to_client(c);
        return false;  // closes after flush
    }

    if (path == "/metrics") {
        const auto m = metrics();
        std::string body = "{";
        body += "\"connections_total\":"   + std::to_string(m.connections_total)   + ",";
        body += "\"connections_active\":"  + std::to_string(m.connections_active)  + ",";
        body += "\"broadcasts_total\":"    + std::to_string(m.broadcasts_total)    + ",";
        body += "\"bytes_sent_total\":"    + std::to_string(m.bytes_sent_total)    + ",";
        body += "\"handshake_failures\":"  + std::to_string(m.handshake_failures);
        body += "}\n";
        std::string resp = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(body.size()) + "\r\n"
                           "Connection: close\r\n\r\n" + body;
        c.out_buf.append(resp);
        flush_to_client(c);
        return false;
    }

    if (path != "/ws") {
        const std::string resp =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        c.out_buf.append(resp);
        flush_to_client(c);
        return false;
    }

    // /ws upgrade.
    const std::string upgrade   = header_value(req, "Upgrade");
    const std::string conn_hdr  = header_value(req, "Connection");
    const std::string ws_key    = header_value(req, "Sec-WebSocket-Key");
    const std::string ws_ver    = header_value(req, "Sec-WebSocket-Version");
    auto contains_ci = [](std::string_view hay, std::string_view needle) {
        std::string h(hay), n(needle);
        std::transform(h.begin(), h.end(), h.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::transform(n.begin(), n.end(), n.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return h.find(n) != std::string::npos;
    };
    if (!contains_ci(upgrade, "websocket") || !contains_ci(conn_hdr, "upgrade")
        || ws_key.empty() || ws_ver != "13") {
        handshake_failures_.fetch_add(1, std::memory_order_relaxed);
        const std::string resp =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        c.out_buf.append(resp);
        flush_to_client(c);
        return false;
    }

    const std::string accept = compute_accept(ws_key);
    const std::string resp =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    c.out_buf.append(resp);
    c.upgraded = true;
    flush_to_client(c);

    // Send the current snapshot, if any, immediately on connect so a
    // brand new client does not have to wait for the next sampler tick.
    std::string snap_copy;
    {
        std::lock_guard<std::mutex> g(broadcast_mu_);
        snap_copy = current_snapshot_;
    }
    if (!snap_copy.empty()) {
        c.out_buf.append(encode_text_frame(snap_copy));
        flush_to_client(c);
    }
    return true;
}

void WsServer::on_client_writable(std::size_t i) {
    flush_to_client(clients_[i]);
}

void WsServer::flush_to_client(Client& c) {
    while (!c.out_buf.empty()) {
        const ssize_t n = ::send(c.fd, c.out_buf.data(), c.out_buf.size(),
                                 MSG_NOSIGNAL);
        if (n > 0) {
            c.out_buf.erase(0, static_cast<std::size_t>(n));
            bytes_sent_total_.fetch_add(static_cast<std::uint64_t>(n),
                                        std::memory_order_relaxed);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
        if (n < 0 && errno == EINTR) continue;
        // Any other error is a hard close; the next poll iteration's
        // POLLERR/POLLHUP will pick it up via close_client. We simply
        // drop the pending bytes here.
        c.out_buf.clear();
        return;
    }
}

void WsServer::close_client(std::size_t i) {
    if (clients_[i].fd >= 0) ::close(clients_[i].fd);
    if (i + 1 != clients_.size()) {
        clients_[i] = std::move(clients_.back());
    }
    clients_.pop_back();
}

void WsServer::drain_broadcast_queue() {
    std::deque<std::string> local;
    {
        std::lock_guard<std::mutex> g(broadcast_mu_);
        local.swap(broadcast_queue_);
        if (!local.empty()) {
            // Track the latest broadcast as the snapshot so newly
            // connecting clients see the freshest picture.
            current_snapshot_ = local.back();
        }
    }
    if (local.empty()) return;
    broadcasts_total_.fetch_add(local.size(), std::memory_order_relaxed);
    for (const auto& payload : local) {
        const std::string frame = encode_text_frame(payload);
        if (frame.size() > kMaxFramePayload + 10) continue;  // sanity guard
        for (auto& c : clients_) {
            if (!c.upgraded) continue;
            c.out_buf.append(frame);
            flush_to_client(c);
        }
    }
}

}  // namespace meridian::ws
