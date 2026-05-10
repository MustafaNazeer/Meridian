#include "meridian/order_pool.hpp"

namespace meridian {

OrderPool::OrderPool(std::size_t capacity)
    : capacity_(capacity),
      in_use_(0),
      storage_(std::make_unique<Order[]>(capacity)),
      free_head_(nullptr) {
    for (std::size_t i = 0; i < capacity_; ++i) {
        storage_[i].next = free_head_;
        free_head_ = &storage_[i];
    }
}

OrderPool::~OrderPool() = default;

Order* OrderPool::acquire() noexcept {
    if (free_head_ == nullptr) {
        return nullptr;
    }
    Order* slot = free_head_;
    free_head_ = slot->next;
    slot->prev = nullptr;
    slot->next = nullptr;
    ++in_use_;
    return slot;
}

void OrderPool::release(Order* order) noexcept {
    order->prev = nullptr;
    order->next = free_head_;
    free_head_ = order;
    --in_use_;
}

}  // namespace meridian

#ifndef NDEBUG
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace meridian::debug_alloc {

// When the matching sweep is active, set this flag to abort on any heap
// allocation. The HotPathGuard RAII helper in matching.cpp flips the flag
// on entry and off on exit.
std::atomic<bool> hot_path_active{false};

}  // namespace meridian::debug_alloc

void* operator new(std::size_t size) {
    if (meridian::debug_alloc::hot_path_active.load(std::memory_order_relaxed)) {
        std::fprintf(stderr,
                     "FATAL: heap allocation of %zu bytes during matching hot path\n",
                     size);
        std::abort();
    }
    void* p = std::malloc(size);
    if (p == nullptr) {
        throw std::bad_alloc{};
    }
    return p;
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
#endif
