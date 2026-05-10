#include "meridian/book_registry.hpp"

#include <utility>

namespace meridian {

BookRegistry::BookRegistry() = default;

BookRegistry::BookRegistry(std::initializer_list<Symbol> symbols) {
    books_.reserve(symbols.size());
    for (Symbol s : symbols) {
        books_.emplace(std::piecewise_construct,
                       std::forward_as_tuple(s),
                       std::forward_as_tuple(s));
    }
}

Book* BookRegistry::book(Symbol s) noexcept {
    auto it = books_.find(s);
    return it == books_.end() ? nullptr : &it->second;
}

const Book* BookRegistry::book(Symbol s) const noexcept {
    auto it = books_.find(s);
    return it == books_.end() ? nullptr : &it->second;
}

bool BookRegistry::contains(Symbol s) const noexcept {
    return books_.find(s) != books_.end();
}

}  // namespace meridian
