#pragma once

#include "meridian/types.hpp"

#include <cstddef>
#include <unordered_map>

namespace meridian {

class OrderIndex {
public:
    OrderIndex();

    OrderIndex(const OrderIndex&) = delete;
    OrderIndex& operator=(const OrderIndex&) = delete;

    void insert(OrderId id, Order* order);
    [[nodiscard]] Order* find(OrderId id) const noexcept;
    void erase(OrderId id);
    [[nodiscard]] bool contains(OrderId id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return map_.size(); }

private:
    std::unordered_map<OrderId, Order*> map_;
};

}  // namespace meridian
