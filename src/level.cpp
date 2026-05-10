#include "meridian/level.hpp"

namespace meridian {

Level::Level(Price price) noexcept
    : price_(price),
      total_qty_(0),
      order_count_(0),
      head_(nullptr),
      tail_(nullptr) {}

void Level::append(Order* order) noexcept {
    order->prev = tail_;
    order->next = nullptr;
    if (tail_ != nullptr) {
        tail_->next = order;
    } else {
        head_ = order;
    }
    tail_ = order;
    total_qty_ += order->qty_remaining;
    ++order_count_;
}

void Level::remove(Order* order) noexcept {
    if (order->prev != nullptr) {
        order->prev->next = order->next;
    } else {
        head_ = order->next;
    }
    if (order->next != nullptr) {
        order->next->prev = order->prev;
    } else {
        tail_ = order->prev;
    }
    total_qty_ -= order->qty_remaining;
    --order_count_;
    order->prev = nullptr;
    order->next = nullptr;
}

void Level::adjust_qty(Order* order, Quantity new_remaining) noexcept {
    total_qty_ += (new_remaining - order->qty_remaining);
    order->qty_remaining = new_remaining;
}

}  // namespace meridian
