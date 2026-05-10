#include "meridian/book_registry.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

namespace meridian {
namespace {

TEST(BookRegistryTest, EmptyRegistryHasZeroSize) {
    BookRegistry registry;
    EXPECT_EQ(registry.size(), 0u);
    EXPECT_FALSE(registry.contains(1));
    EXPECT_EQ(registry.book(1), nullptr);
}

TEST(BookRegistryTest, ConstructorWithSymbolsRegistersAllOfThem) {
    BookRegistry registry{1, 2, 3, 4, 5};

    EXPECT_EQ(registry.size(), 5u);
    for (Symbol s : {1, 2, 3, 4, 5}) {
        EXPECT_TRUE(registry.contains(static_cast<Symbol>(s)));
        Book* b = registry.book(static_cast<Symbol>(s));
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(b->symbol(), static_cast<Symbol>(s));
    }
}

TEST(BookRegistryTest, UnknownSymbolReturnsNullptr) {
    BookRegistry registry{1, 2};
    EXPECT_EQ(registry.book(99), nullptr);
    EXPECT_FALSE(registry.contains(99));
}

TEST(BookRegistryTest, BooksForDifferentSymbolsAreDistinctObjects) {
    BookRegistry registry{1, 2};
    Book* a = registry.book(1);
    Book* b = registry.book(2);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
    EXPECT_EQ(a->symbol(), 1);
    EXPECT_EQ(b->symbol(), 2);
}

TEST(BookRegistryTest, ConstFindReturnsConstPointer) {
    const BookRegistry registry{1};
    const Book* a = registry.book(1);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->symbol(), 1);
}

}  // namespace
}  // namespace meridian
