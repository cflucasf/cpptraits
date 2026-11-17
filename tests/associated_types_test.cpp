#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "traits.hpp"

namespace associated {
trait(ReadOnly,
    associated_type(Item),
    associated_type(Key),
    method(const Item&, read, (param(const Key&, key)), const noexcept)
);

template<class T, class K>
struct Record {
    T value;
    K key;
};
}

// The foreign objects deliberately have neither aliases nor trait methods.
impl(associated::ReadOnly, (template<class T, class K>), (associated::Record<T, K>)) {
    using Item = T;
    using Key = K;
    static const Item& read(const associated::Record<T, K>& object, const Key&) noexcept {
        return object.value;
    }
};

using IntRecord = associated::Record<int, unsigned>;
using LongRecord = associated::Record<long, short>;
using IntReader = dyn(associated::ReadOnly, int, unsigned);
using LongReader = dyn(associated::ReadOnly, long, short);

template<class T>
concept CanBorrowRead = requires(T&& object) {
    borrow(associated::ReadOnly, std::forward<T>(object));
};
template<class T>
concept CanAdaptRead = requires(T&& object) {
    adapt(associated::ReadOnly, std::forward<T>(object));
};
template<class T>
concept CanBoxRead = requires(T&& object) {
    box(associated::ReadOnly, std::forward<T>(object));
};
template<class T>
constexpr bool rejectedRead = !associated::ReadOnly<T> &&
    !CanBorrowRead<T&> && !CanAdaptRead<T&> && !CanBoxRead<T&> &&
    !CanBorrowRead<T> && !CanAdaptRead<T> && !CanBoxRead<T>;

struct Unmapped {};
struct StructuralOnly {
    using Item = int;
    using Key = unsigned;
    const int& read(const unsigned&) const noexcept { return value; }
    int value = 0;
};
struct MissingBindings {};
impl(associated::ReadOnly, MissingBindings) {};
struct MissingBinding {};
impl(associated::ReadOnly, MissingBinding) {
    using Item = int;
    static const int& read(const MissingBinding&, const unsigned&) noexcept;
};
struct MissingMethod {};
impl(associated::ReadOnly, MissingMethod) {
    using Item = int;
    using Key = unsigned;
};
struct WrongReturn {};
impl(associated::ReadOnly, WrongReturn) {
    using Item = int;
    using Key = unsigned;
    static int read(const WrongReturn&, const Key&) noexcept;
};
struct WrongParameter {};
impl(associated::ReadOnly, WrongParameter) {
    using Item = int;
    using Key = unsigned;
    static const Item& read(const WrongParameter&, Key) noexcept;
};
struct WrongReceiver {};
impl(associated::ReadOnly, WrongReceiver) {
    using Item = int;
    using Key = unsigned;
    static const Item& read(WrongReceiver&, const Key&) noexcept;
};
struct ByValueReceiver {};
impl(associated::ReadOnly, ByValueReceiver) {
    using Item = int;
    using Key = unsigned;
    static const Item& read(ByValueReceiver, const Key&) noexcept;
};
struct ThrowingMapping {};
impl(associated::ReadOnly, ThrowingMapping) {
    using Item = int;
    using Key = unsigned;
    static const Item& read(const ThrowingMapping&, const Key&);
};
struct NonStaticMapping {};
impl(associated::ReadOnly, NonStaticMapping) {
    using Item = int;
    using Key = unsigned;
    const Item& read(const NonStaticMapping&, const Key&) noexcept;
};

static_assert(rejectedRead<Unmapped>);
static_assert(rejectedRead<StructuralOnly>);
static_assert(rejectedRead<MissingBindings>);
static_assert(rejectedRead<MissingBinding>);
static_assert(rejectedRead<MissingMethod>);
static_assert(rejectedRead<WrongReturn>);
static_assert(rejectedRead<WrongParameter>);
static_assert(rejectedRead<WrongReceiver>);
static_assert(rejectedRead<ByValueReceiver>);
static_assert(rejectedRead<ThrowingMapping>);
static_assert(rejectedRead<NonStaticMapping>);
static_assert(rejectedRead<volatile IntRecord>);

using OwnedRead = decltype(adapt(associated::ReadOnly, std::declval<IntRecord>()));
using BorrowedRead = decltype(borrow(associated::ReadOnly, std::declval<IntRecord&>()));
using ConstBorrowedRead = decltype(borrow(associated::ReadOnly,
                                         std::declval<const IntRecord&>()));
static_assert(associated::ReadOnly<IntRecord> && associated::ReadOnly<const IntRecord>);
static_assert(associated::ReadOnly<OwnedRead> && associated::ReadOnly<BorrowedRead> &&
              associated::ReadOnly<ConstBorrowedRead>);
static_assert(associated::ReadOnly<IntReader> && associated::ReadOnly<LongReader>);
static_assert(std::same_as<OwnedRead::Item, int> && std::same_as<OwnedRead::Key, unsigned>);
static_assert(std::same_as<ConstBorrowedRead::Item, int> &&
              std::same_as<ConstBorrowedRead::Key, unsigned>);
static_assert(std::same_as<IntReader::Item, int> && std::same_as<IntReader::Key, unsigned>);
static_assert(std::same_as<LongReader::Item, long> && std::same_as<LongReader::Key, short>);
static_assert(!std::same_as<IntReader, LongReader>);
static_assert(std::convertible_to<OwnedRead&, IntReader&>);
static_assert(std::convertible_to<BorrowedRead&, IntReader&>);
static_assert(std::convertible_to<const ConstBorrowedRead&, const IntReader&>);
static_assert(!std::convertible_to<OwnedRead&, LongReader&>);
static_assert(!std::convertible_to<IntRecord&, IntReader&>);
static_assert(std::same_as<decltype(box(associated::ReadOnly, std::declval<IntRecord>())),
                           std::unique_ptr<IntReader>>);
static_assert(std::same_as<decltype(box(associated::ReadOnly, std::declval<LongRecord>())),
                           std::unique_ptr<LongReader>>);
static_assert(std::has_virtual_destructor_v<IntReader>);
static_assert(CanBorrowRead<IntRecord&> && CanBorrowRead<const IntRecord&>);
static_assert(!CanBorrowRead<IntRecord> && !CanBorrowRead<const IntRecord>);
static_assert(!std::is_constructible_v<BorrowedRead, IntRecord&&>);
static_assert(!std::is_constructible_v<ConstBorrowedRead, IntRecord&&>);
static_assert(!std::is_constructible_v<ConstBorrowedRead, const IntRecord&&>);
static_assert(CanAdaptRead<IntRecord&> && CanAdaptRead<const IntRecord&>);
static_assert(CanBoxRead<IntRecord&> && CanBoxRead<const IntRecord&>);
static_assert(noexcept(borrow(associated::ReadOnly, std::declval<const IntRecord&>())));

template<class T>
constexpr bool readContract =
    std::same_as<decltype(std::declval<const T&>().read(0u)), const int&> &&
    noexcept(std::declval<const T&>().read(0u));
static_assert(readContract<IntReader> && readContract<OwnedRead> &&
              readContract<BorrowedRead> && readContract<ConstBorrowedRead>);

trait(Operations,
    associated_type(Item),
    associated_type(Token),
    method(Item&, value, noexcept),
    method(const Item&, inspect, const noexcept),
    method(void, combine,
           (param(const Item&, source), param(Item&, output), param(Item&&, temporary)), ),
    method(void, consume, (param(Token, token)), ),
    method(void, take, (param(Token&&, token)), noexcept)
);

struct State {
    int stored = 9;
};
impl(Operations, State) {
    using Item = int;
    using Token = std::unique_ptr<int>;
    static Item& value(State& object) noexcept { return object.stored; }
    static const Item& inspect(const State& object) noexcept { return object.stored; }
    static void combine(State& object, const Item& source, Item& output, Item&& temporary) {
        output += source;
        object.stored = output + temporary;
    }
    static void consume(State& object, Token token) { object.stored = *token; }
    static void take(State& object, Token&& token) noexcept {
        auto owned = std::move(token);
        object.stored = *owned;
    }
};

using OperationInterface = dyn(Operations, int, std::unique_ptr<int>);
using OwnedOperations = decltype(adapt(Operations, std::declval<State>()));
using BorrowedOperations = decltype(borrow(Operations, std::declval<State&>()));
template<class T>
concept CanBorrowOperations = requires(T&& object) {
    borrow(Operations, std::forward<T>(object));
};
static_assert(Operations<State> && !Operations<const State> && !Operations<volatile State>);
static_assert(Operations<OwnedOperations> && Operations<BorrowedOperations>);
static_assert(CanBorrowOperations<State&> && !CanBorrowOperations<const State&>);
static_assert(!CanBorrowOperations<State> && !CanBorrowOperations<const State>);
static_assert(!std::is_constructible_v<BorrowedOperations, State&&>);
static_assert(std::convertible_to<OwnedOperations&, OperationInterface&>);
static_assert(std::convertible_to<BorrowedOperations&, OperationInterface&>);
static_assert(std::same_as<decltype(box(Operations, std::declval<State>())),
                           std::unique_ptr<OperationInterface>>);

template<class T>
constexpr bool operationContract =
    std::same_as<decltype(std::declval<T&>().value()), int&> &&
    std::same_as<decltype(std::declval<const T&>().inspect()), const int&> &&
    noexcept(std::declval<T&>().value()) &&
    noexcept(std::declval<const T&>().inspect()) &&
    !noexcept(std::declval<T&>().combine(std::declval<const int&>(),
                                        std::declval<int&>(), 1)) &&
    !noexcept(std::declval<T&>().consume(std::declval<std::unique_ptr<int>>())) &&
    noexcept(std::declval<T&>().take(std::declval<std::unique_ptr<int>&&>()));
static_assert(operationContract<OperationInterface> && operationContract<OwnedOperations> &&
              operationContract<BorrowedOperations>);

struct Sentinel {
    int& destructions;
    explicit Sentinel(int& destructions) : destructions(destructions) {}
    ~Sentinel() { ++destructions; }
};
struct MoveOnly {
    std::unique_ptr<int> value;
    std::unique_ptr<Sentinel> sentinel;
};
impl(associated::ReadOnly, MoveOnly) {
    using Item = int;
    using Key = unsigned;
    static const Item& read(const MoveOnly& object, const Key&) noexcept {
        return *object.value;
    }
};
static_assert(!std::is_copy_constructible_v<MoveOnly>);
static_assert(CanAdaptRead<MoveOnly> && CanBoxRead<MoveOnly>);
static_assert(std::same_as<decltype(box(associated::ReadOnly, std::declval<MoveOnly>())),
                           std::unique_ptr<IntReader>>);
static_assert(!CanAdaptRead<MoveOnly&> && !CanBoxRead<MoveOnly&>);
static_assert(!CanAdaptRead<const MoveOnly> && !CanBoxRead<const MoveOnly>);
static_assert(CanBorrowRead<MoveOnly&> && CanBorrowRead<const MoveOnly&>);

// Native interface satisfaction alone must not enable mapping-based factories.
struct NativeRead : implements(associated::ReadOnly, int, unsigned) {
    int stored = 5;
    const int& read(const unsigned&) const noexcept override { return stored; }
};
static_assert(associated::ReadOnly<NativeRead>);
static_assert(!CanBorrowRead<NativeRead&> && !CanAdaptRead<NativeRead> && !CanBoxRead<NativeRead>);

bool exerciseOperations(OperationInterface& object)
{
    const auto& constant = object;
    int& reference = object.value();
    reference = 12;
    if (&reference != &constant.inspect() || constant.inspect() != 12) {
        return false;
    }
    const int source = 2;
    int output = 3;
    object.combine(source, output, 11);
    if (source != 2 || output != 5 || constant.inspect() != 16) {
        return false;
    }
    auto byValue = std::make_unique<int>(41);
    object.consume(std::move(byValue));
    if (byValue || reference != 41) {
        return false;
    }
    auto byReference = std::make_unique<int>(43);
    object.take(std::move(byReference));
    return !byReference && reference == 43 && constant.inspect() == 43;
}

// A common associated-type name must not be shadowed by adapter parameters.
trait(NextValue,
    associated_type(T),
    method(std::optional<std::remove_cv_t<T>>, next, )
);

impl(NextValue,
     (template<class Element, class Allocator>),
     (std::vector<Element, Allocator>)) {
    using T = Element;

    static std::optional<std::remove_cv_t<T>>
    next(std::vector<Element, Allocator>& values) {
        if (values.empty()) {
            return std::nullopt;
        }
        std::optional<std::remove_cv_t<T>> result{
            std::in_place, std::move(values.front())
        };
        values.erase(values.begin());
        return result;
    }
};

using IntNext = dyn(NextValue, int);
using OwnedNext = decltype(adapt(NextValue, std::declval<std::vector<int>>()));
using BorrowedNext = decltype(borrow(NextValue, std::declval<std::vector<int>&>()));
static_assert(NextValue<OwnedNext> && NextValue<BorrowedNext>);
static_assert(std::same_as<OwnedNext::T, int> && std::same_as<BorrowedNext::T, int>);
static_assert(std::same_as<decltype(std::declval<OwnedNext&>().next()), std::optional<int>>);
static_assert(std::same_as<decltype(std::declval<BorrowedNext&>().next()), std::optional<int>>);
static_assert(std::same_as<decltype(box(NextValue, std::declval<std::vector<int>>())),
                           std::unique_ptr<IntNext>>);

bool exerciseAssociatedT()
{
    std::vector<int> values{1, 2};
    auto borrowed = borrow(NextValue, values);
    auto owned = adapt(NextValue, values);
    std::unique_ptr<IntNext> boxed = box(NextValue, values);
    IntNext& interface = borrowed;
    if (interface.next() != 1 || values != std::vector<int>{2} ||
        owned.next() != 1 || boxed->next() != 1 ||
        interface.next() != 2 || owned.next() != 2 || boxed->next() != 2 ||
        interface.next() || owned.next() || boxed->next() || !values.empty()) {
        return false;
    }

    std::vector<std::unique_ptr<int>> pointers;
    pointers.push_back(std::make_unique<int>(42));
    auto moveOwned = adapt(NextValue, std::move(pointers));
    static_assert(std::same_as<decltype(moveOwned)::T, std::unique_ptr<int>>);
    auto result = moveOwned.next();
    return result && *result && **result == 42 && !moveOwned.next();
}

int main()
{
    if (!exerciseAssociatedT()) {
        return 15;
    }
    IntRecord source{10, 1};
    auto borrowed = borrow(associated::ReadOnly, source);
    auto owned = adapt(associated::ReadOnly, source);
    auto boxed = box(associated::ReadOnly, source);
    const IntReader& borrowedInterface = borrowed;
    const IntReader& ownedInterface = owned;
    source.value = 20;
    if (&borrowedInterface.read(source.key) != &source.value ||
        borrowedInterface.read(source.key) != 20 || ownedInterface.read(source.key) != 10 ||
        boxed->read(source.key) != 10) {
        return 1;
    }

    const IntRecord constant{30, 2};
    auto constBorrow = borrow(associated::ReadOnly, constant);
    auto constCopy = adapt(associated::ReadOnly, constant);
    auto constBox = box(associated::ReadOnly, constant);
    const IntReader& constInterface = constBorrow;
    if (&constInterface.read(constant.key) != &constant.value ||
        constCopy.read(constant.key) != 30 || constBox->read(constant.key) != 30 ||
        &constCopy.read(constant.key) == &constant.value ||
        &constBox->read(constant.key) == &constant.value) {
        return 2;
    }

    LongRecord other{99L, 3};
    auto otherBorrow = borrow(associated::ReadOnly, other);
    auto otherOwned = adapt(associated::ReadOnly, other);
    auto otherBox = box(associated::ReadOnly, other);
    const LongReader& otherInterface = otherBorrow;
    other.value = 100L;
    if (&otherInterface.read(other.key) != &other.value ||
        otherOwned.read(other.key) != 99L || otherBox->read(other.key) != 99L) {
        return 3;
    }

    State state;
    auto ownedOperations = adapt(Operations, state);
    auto borrowedOperations = borrow(Operations, state);
    auto boxedOperations = box(Operations, std::as_const(state));
    if (!exerciseOperations(ownedOperations) || state.stored != 9) {
        return 4;
    }
    if (!exerciseOperations(*boxedOperations) || state.stored != 9) {
        return 5;
    }
    if (!exerciseOperations(borrowedOperations) || state.stored != 43 ||
        &borrowedOperations.value() != &state.stored) {
        return 6;
    }

    int destructions = 0;
    {
        MoveOnly moving{std::make_unique<int>(70), std::make_unique<Sentinel>(destructions)};
        auto moveOwned = adapt(associated::ReadOnly, std::move(moving));
        const IntReader& interface = moveOwned;
        if (moving.value || moving.sentinel || interface.read(0u) != 70 || destructions != 0) {
            return 7;
        }
    }
    if (destructions != 1) {
        return 8;
    }
    {
        MoveOnly moving{std::make_unique<int>(80), std::make_unique<Sentinel>(destructions)};
        std::unique_ptr<IntReader> moveBox = box(associated::ReadOnly, std::move(moving));
        if (moving.value || moving.sentinel || moveBox->read(0u) != 80 || destructions != 1) {
            return 9;
        }
        moveBox.reset();
        if (destructions != 2) {
            return 10;
        }
    }
    if (destructions != 2) {
        return 11;
    }
    {
        MoveOnly lifetime{std::make_unique<int>(90), std::make_unique<Sentinel>(destructions)};
        {
            auto lifetimeBorrow = borrow(associated::ReadOnly, std::as_const(lifetime));
            if (&lifetimeBorrow.read(0u) != lifetime.value.get()) {
                return 12;
            }
        }
        if (!lifetime.sentinel || destructions != 2) {
            return 13;
        }
    }
    return destructions == 3 ? 0 : 14;
}
