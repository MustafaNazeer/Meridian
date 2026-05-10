#pragma once

// Hand rolled WebSocket server, RFC 6455.
//
// Single-threaded, blocking, poll()-based event loop. Speaks the
// minimum of WSS that the live demo needs:
//
//   * HTTP/1.1 handshake on /ws (Upgrade: websocket).
//   * GET /healthz returns 200 OK with a one-line JSON body.
//   * GET /metrics returns 200 OK with a JSON body of counters.
//   * Server-to-client text frames only (opcode 0x1, FIN set, no
//     mask, length encoded per RFC 6455 section 5.2).
//   * No fragmentation, no permessage-deflate, no client-to-server
//     payload (the demo is a one-way data feed).
//
// Threading model: serve_forever() runs on the main thread.
// broadcast() may be called from any other thread (the sampler) and
// queues the payload through a mutex-protected deque; the main
// thread drains the queue on every poll wakeup. The poll timeout is
// short enough (33 ms) that broadcast latency stays inside one 30 Hz
// sampler tick.

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace meridian::ws {

class WsServer {
public:
    // Bind to the given port. Pass 0 to let the OS assign a port; the
    // assigned port is then readable via assigned_port() after bind.
    explicit WsServer(int port);
    ~WsServer();

    WsServer(const WsServer&) = delete;
    WsServer& operator=(const WsServer&) = delete;

    [[nodiscard]] int assigned_port() const noexcept { return assigned_port_; }

    // Run the event loop until stop() is called from another thread or
    // the binary is signalled. Blocks the caller.
    void serve_forever();

    // Ask the loop to exit. Safe to call from any thread or signal
    // handler (sets a relaxed atomic flag the main thread polls).
    void request_stop() noexcept { stopping_.store(true, std::memory_order_relaxed); }

    // Enqueue a text frame to be broadcast to every connected /ws
    // client on the next poll wakeup. Safe to call from any thread.
    // The payload is copied; the caller may free its buffer on
    // return.
    void broadcast(std::string_view payload);

    // The most recent snapshot payload. New /ws connections receive
    // this immediately after the handshake completes, so a client
    // sees the current top of book without waiting for the next
    // sampler tick. Replaced by every broadcast(); access is
    // synchronised by the same mutex used for the broadcast queue.
    void set_snapshot(std::string_view payload);

    // Diagnostic counters; readable from any thread.
    struct Metrics {
        std::uint64_t connections_total    = 0;
        std::uint64_t connections_active   = 0;
        std::uint64_t broadcasts_total     = 0;
        std::uint64_t bytes_sent_total     = 0;
        std::uint64_t handshake_failures   = 0;
    };
    [[nodiscard]] Metrics metrics() const noexcept;

private:
    struct Client {
        int fd = -1;
        std::string in_buf;       // partial HTTP request being read
        bool upgraded = false;    // true once the WS handshake completes
        std::string out_buf;      // pending bytes the kernel has not accepted yet
    };

    void on_listener_ready();
    void on_client_readable(std::size_t i);
    void on_client_writable(std::size_t i);
    bool handle_http_request(Client& c);
    void close_client(std::size_t i);
    void drain_broadcast_queue();
    void flush_to_client(Client& c);

    int listen_fd_ = -1;
    int assigned_port_ = 0;
    std::vector<Client> clients_;

    std::atomic<bool> stopping_{false};

    mutable std::mutex broadcast_mu_;
    std::deque<std::string> broadcast_queue_;
    std::string current_snapshot_;

    // Counters; updated only on the main thread, read with relaxed
    // atomics from any thread.
    std::atomic<std::uint64_t> connections_total_{0};
    std::atomic<std::uint64_t> broadcasts_total_{0};
    std::atomic<std::uint64_t> bytes_sent_total_{0};
    std::atomic<std::uint64_t> handshake_failures_{0};
};

}  // namespace meridian::ws
