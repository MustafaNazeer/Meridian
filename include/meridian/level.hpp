#pragma once

#include "meridian/types.hpp"

#include <cstddef>

namespace meridian {

class Level {
public:
    explicit Level(Price price) noexcept;

    [[nodiscard]] Price price() const noexcept { return price_; }
    [[nodiscard]] Quantity total_qty() const noexcept { return total_qty_; }
    [[nodiscard]] std::size_t order_count() const noexcept { return order_count_; }
    [[nodiscard]] Order* head() const noexcept { return head_; }
    [[nodiscard]] Order* tail() const noexcept { return tail_; }
    [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

    void append(Order* order) noexcept;
    void remove(Order* order) noexcept;
    void adjust_qty(Order* order, Quantity new_remaining) noexcept;

private:
    Price price_;
    Quantity total_qty_;
    std::size_t order_count_;
    Order* head_;
    Order* tail_;
};

}  // namespace meridian
