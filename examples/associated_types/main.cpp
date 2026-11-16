#include <array>
#include <concepts>
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

static_assert(Iterator<ArrayIter<int>> && Iterator<ArrayIter<char>>);
static_assert(std::same_as<ArrayIter<int>::Item, int>);

int main()
{
    const std::array numbers{10, 20};
    ArrayIter<int> iter{numbers};
    return iter.peek() == numbers.data() && iter.next() == 10 &&
           iter.next() == 20 && iter.peek() == nullptr && !iter.next() ? 0 : 1;
}
