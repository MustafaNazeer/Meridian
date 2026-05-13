#pragma once

#include "meridian/book.hpp"
#include "meridian/types.hpp"

#include <cstddef>
#include <initializer_list>
#include <unordered_map>

namespace meridian {

class BookRegistry {
public:
    BookRegistry();
    explicit BookRegistry(std::initializer_list<Symbol> symbols,
                          bool observability = true);

    BookRegistry(const BookRegistry&) = delete;
    BookRegistry& operator=(const BookRegistry&) = delete;

    [[nodiscard]] Book* book(Symbol s) noexcept;
    [[nodiscard]] const Book* book(Symbol s) const noexcept;
    [[nodiscard]] bool contains(Symbol s) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return books_.size(); }

private:
    std::unordered_map<Symbol, Book> books_;
};

}  // namespace meridian
