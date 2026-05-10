#include "meridian/order_index.hpp"

namespace meridian {

OrderIndex::OrderIndex() = default;

void OrderIndex::insert(OrderId id, Order* order) {
    map_[id] = order;
}

Order* OrderIndex::find(OrderId id) const noexcept {
    auto it = map_.find(id);
    return it == map_.end() ? nullptr : it->second;
}

void OrderIndex::erase(OrderId id) {
    map_.erase(id);
}

bool OrderIndex::contains(OrderId id) const noexcept {
    return map_.find(id) != map_.end();
}

}  // namespace meridian
