#include <concepts>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "traits.hpp"

trait(Iterable,
    associated_type(Item),
    associated_type(Iterator),
    method(Iterator, begin, const noexcept),
    method(Iterator, end, const noexcept)
);

// Extend every std::vector specialization externally, including custom allocators.
// Const iterators make this a read-only iterable, even for a mutable vector.
impl(Iterable,
     (template<class T, class Allocator>),
     (std::vector<T, Allocator>)) {
    using Item = T;
    using Iterator = typename std::vector<T, Allocator>::const_iterator;

    static Iterator begin(const std::vector<T, Allocator>& values) noexcept {
        return values.cbegin();
    }

    static Iterator end(const std::vector<T, Allocator>& values) noexcept {
        return values.cend();
    }
};

using IntVector = std::vector<int>;
using IntIterable = dyn(Iterable, int, IntVector::const_iterator);

static_assert(Iterable<IntVector> && Iterable<const IntVector>);
static_assert(Iterable<std::vector<std::string>>);
static_assert(std::same_as<trait_mapping_t(Iterable, IntVector)::Item, int>);
static_assert(!std::derived_from<IntVector, IntIterable>);

int sum(const IntIterable& values)
{
    int total = 0;
    // Range-for calls the generated interface's virtual begin() and end().
    for (int value : values) {
        total += value;
    }
    return total;
}

int main()
{
    IntVector numbers{1, 2, 3};
    auto borrowed = borrow(Iterable, numbers);
    auto owned = adapt(Iterable, numbers);
    std::unique_ptr<IntIterable> boxed = box(Iterable, numbers);

    static_assert(Iterable<decltype(borrowed)>);
    static_assert(std::same_as<decltype(borrowed)::Item, int>);

    numbers.push_back(4);
    // Borrowing observes the original vector; adapt/box copied its elements.
    // As with vector itself, mutation can invalidate previously obtained iterators.
    if (sum(borrowed) != 10 || sum(owned) != 6 || sum(*boxed) != 6) {
        return 1;
    }

    const IntVector constant{5, 6};
    auto constBorrowed = borrow(Iterable, constant);
    if (sum(constBorrowed) != 11) {
        return 2;
    }

    // An rvalue is stored by value, so this adapter owns the vector and its data.
    auto moved = adapt(Iterable, IntVector{7, 8});
    if (sum(moved) != 15) {
        return 3;
    }

    std::vector<std::string> words{"external", "mapping"};
    auto text = borrow(Iterable, words);
    std::string joined;
    for (const auto& word : text) {
        joined += word;
    }
    return joined == "externalmapping" ? 0 : 4;
}
