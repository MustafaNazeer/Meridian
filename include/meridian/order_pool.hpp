#pragma once

#include "meridian/types.hpp"

#include <cstddef>
#include <memory>

namespace meridian {

class OrderPool {
public:
    explicit OrderPool(std::size_t capacity);
    ~OrderPool();

    OrderPool(const OrderPool&) = delete;
    OrderPool& operator=(const OrderPool&) = delete;

    Order* acquire() noexcept;
    void release(Order* order) noexcept;

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t in_use() const noexcept { return in_use_; }

private:
    std::size_t capacity_;
    std::size_t in_use_;
    std::unique_ptr<Order[]> storage_;
    Order* free_head_;
};

}  // namespace meridian
