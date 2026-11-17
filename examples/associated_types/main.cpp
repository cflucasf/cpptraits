#include <array>
#include <concepts>
#include <memory>
#include <optional>
#include <span>

#include "traits.hpp"

trait(Iterator,
    associated_type(Item),
    method(const Item*, peek, const noexcept),
    method(std::optional<Item>, next, )
);

template<class T>
struct ArrayIter : implements(Iterator, T) {
    explicit ArrayIter(std::span<const T> values) : remaining(values) {}

    std::span<const T> remaining;

    const T* peek() const noexcept override {
        return remaining.empty() ? nullptr : remaining.data();
    }

    std::optional<T> next() override {
        if (const T* item = peek()) {
            T value = *item;
            remaining = remaining.subspan(1);
            return value;
        }
        return std::nullopt;
    }
};

// Map a foreign cursor without adding inheritance or member operations.
impl(Iterator, (template<class T>), (std::span<const T>)) {
    using Item = T;

    static const Item* peek(const std::span<const T>& cursor) noexcept {
        return cursor.empty() ? nullptr : cursor.data();
    }

    static std::optional<Item> next(std::span<const T>& cursor) {
        if (cursor.empty()) {
            return std::nullopt;
        }
        Item value = cursor.front();
        cursor = cursor.subspan(1);
        return value;
    }
};

static_assert(Iterator<ArrayIter<int>> && Iterator<ArrayIter<char>>);
static_assert(std::same_as<ArrayIter<int>::Item, int>);

int main()
{
    const std::array numbers{10, 20};
    ArrayIter<int> iter{numbers};
    if (iter.peek() != numbers.data() || iter.next() != 10 ||
        iter.next() != 20 || iter.peek() != nullptr || iter.next()) {
        return 1;
    }

    std::span<const int> cursor{numbers};
    auto borrowed = borrow(Iterator, cursor);
    auto owned = adapt(Iterator, cursor);
    std::unique_ptr<dyn(Iterator, int)> boxed = box(Iterator, cursor);
    dyn(Iterator, int)& interface = borrowed;
    if (interface.next() != 10 || cursor.size() != 1 ||
        owned.next() != 10 || boxed->next() != 10) {
        return 2;
    }
    return interface.next() == 20 && owned.next() == 20 &&
           boxed->next() == 20 && !boxed->next() ? 0 : 3;
}
