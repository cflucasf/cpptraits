#include "traits.hpp"

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

trait(Ordinary,
    method(int, value, (), const),
    method(void, clear, )
);

struct OrdinaryValue : implements(Ordinary) {
    int stored = 17;
    int value() const override { return stored; }
    void clear() override { stored = 0; }
};

namespace declarations::nested {
// Implementation signatures may use aliases; declarations exercise wrapped types.
using Snapshot = std::pair<int, int>;

// Exactly eight entries exercise the declaration-count boundary. Both lookup
// signatures must survive interface generation, signature checks and forwarding.
trait(Operations,
    method(void, reset, ),
    method(int, read, const),
    method(void, tick, noexcept),
    method((std::pair<int, int>), snapshot, const noexcept),
    method(int, sum, (param(int, amount), param(int, multiplier)), const),
    method(void, consume, (param(std::unique_ptr<int>, next)), ),
    method(int, lookup, (param(int, offset)), const),
    method(double, lookup, (param(double, offset)), const noexcept)
);

struct Native : implements(Operations) {
    int stored = 9;
    void reset() override { stored = 0; }
    int read() const override { return stored; }
    void tick() noexcept override { ++stored; }
    Snapshot snapshot() const noexcept override { return {stored, stored + 1}; }
    int sum(int amount, int multiplier) const override {
        return stored + amount * multiplier;
    }
    void consume(std::unique_ptr<int> next) override { stored = *next; }
    int lookup(int offset) const override { return stored + offset; }
    double lookup(double offset) const noexcept override { return stored + offset; }
};

// No matching member functions or inheritance: every operation uses the mapping.
struct External {
    int stored = 9;
};
}

// Qualified customization from an enclosing namespace exercises public lookup.
impl(declarations::nested::Operations, declarations::nested::External) {
    using External = declarations::nested::External;
    using Snapshot = declarations::nested::Snapshot;

    static void reset(External& object) { object.stored = 0; }
    static int read(const External& object) { return object.stored; }
    static void tick(External& object) noexcept { ++object.stored; }
    static Snapshot snapshot(const External& object) noexcept {
        return {object.stored, object.stored + 1};
    }
    static int sum(const External& object, int amount, int multiplier) {
        return object.stored + amount * multiplier;
    }
    static void consume(External& object, std::unique_ptr<int> next) {
        object.stored = *next;
    }
    static int lookup(const External& object, int offset) {
        return object.stored + offset;
    }
    static double lookup(const External& object, double offset) noexcept {
        return object.stored + offset;
    }
};

using Owned = decltype(adapt(declarations::nested::Operations,
                             std::declval<declarations::nested::External>()));
using Borrowed = decltype(borrow(declarations::nested::Operations,
                                 std::declval<declarations::nested::External&>()));

static_assert(Ordinary<OrdinaryValue>);
static_assert(declarations::nested::Operations<declarations::nested::Native>);
static_assert(declarations::nested::Operations<declarations::nested::External>);
static_assert(!declarations::nested::Operations<const declarations::nested::External>);
static_assert(!std::convertible_to<declarations::nested::External&,
                                  dyn(declarations::nested::Operations)&>);
static_assert(declarations::nested::Operations<Owned>);
static_assert(declarations::nested::Operations<Borrowed>);
static_assert(std::has_virtual_destructor_v<dyn(declarations::nested::Operations)>);

template<typename T>
constexpr bool qualifierContract =
    !noexcept(std::declval<T&>().reset()) &&
    !noexcept(std::declval<const T&>().read()) &&
    noexcept(std::declval<T&>().tick()) &&
    noexcept(std::declval<const T&>().snapshot()) &&
    !noexcept(std::declval<const T&>().lookup(1)) &&
    noexcept(std::declval<const T&>().lookup(0.5)) &&
    std::same_as<decltype(std::declval<const T&>().snapshot()),
                 declarations::nested::Snapshot> &&
    std::same_as<decltype(std::declval<const T&>().lookup(1)), int> &&
    std::same_as<decltype(std::declval<const T&>().lookup(0.5)), double>;

static_assert(qualifierContract<dyn(declarations::nested::Operations)>);
static_assert(qualifierContract<Owned>);
static_assert(qualifierContract<Borrowed>);

// Opt-in compile-fail fixture: method(...) expects 3 or 4 arguments.
#ifdef CPPTRAITS_TEST_BAD_METHOD_ARITY
trait(BadMethodArity, method(void, broken, (), (), ));
#endif

bool exercise(dyn(declarations::nested::Operations)& object)
{
    const auto& constant = object;
    object.reset();
    if (constant.read() != 0 || constant.snapshot() != declarations::nested::Snapshot{0, 1}) {
        return false;
    }
    object.tick();
    if (constant.read() != 1 || constant.sum(2, 3) != 7) {
        return false;
    }

    auto next = std::make_unique<int>(42);
    object.consume(std::move(next));
    return !next && constant.read() == 42 &&
           constant.lookup(3) == 45 && constant.lookup(0.5) == 42.5 &&
           constant.snapshot() == declarations::nested::Snapshot{42, 43};
}

trait(Forwarding,
    method((std::pair<int, int>), snapshot, (), const),
    method(void, replace, (param((std::pair<int, int>), snapshot)), ),
    method(int, inspect, (param((const std::pair<int, int>&), snapshot)), const),
    method(void, mutate, (param(int&, value)), const),
    method(void, consume, (param(std::unique_ptr<int>, next)), ),
    method(void, take, (param(std::unique_ptr<int>&&, next)), ),
    method(int, categories,
           (param(const int&, constant), param(int&, mutableValue), param(int&&, temporary)),
           const),
    method(int, eight,
           (param(int, a), param(int, b), param(int, c), param(int, d),
            param(int, e), param(int, f), param(int, g), param(int, h)), const noexcept)
);

struct NativeForwarding : implements(Forwarding) {
    int stored = 9;
    std::pair<int, int> snapshot() const override { return {stored, stored + 1}; }
    void replace(std::pair<int, int> value) override { stored = value.first + value.second; }
    int inspect(const std::pair<int, int>& value) const override {
        return stored + value.first + value.second;
    }
    void mutate(int& value) const override { value += stored; }
    void consume(std::unique_ptr<int> next) override { stored = *next; }
    void take(std::unique_ptr<int>&& next) override {
        auto owned = std::move(next);
        stored = *owned;
    }
    int categories(const int& constant, int& mutableValue, int&& temporary) const override {
        mutableValue += constant;
        return stored + constant + mutableValue + temporary;
    }
    int eight(int a, int b, int c, int d, int e, int f, int g, int h) const noexcept override {
        return stored + a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h;
    }
};

struct ExternalForwarding {
    int stored = 9;
};

impl(Forwarding, ExternalForwarding) {
    static std::pair<int, int> snapshot(const ExternalForwarding& object) {
        return {object.stored, object.stored + 1};
    }
    static void replace(ExternalForwarding& object, std::pair<int, int> value) {
        object.stored = value.first + value.second;
    }
    static int inspect(const ExternalForwarding& object, const std::pair<int, int>& value) {
        return object.stored + value.first + value.second;
    }
    static void mutate(const ExternalForwarding& object, int& value) {
        value += object.stored;
    }
    static void consume(ExternalForwarding& object, std::unique_ptr<int> next) {
        object.stored = *next;
    }
    static void take(ExternalForwarding& object, std::unique_ptr<int>&& next) {
        auto owned = std::move(next);
        object.stored = *owned;
    }
    static int categories(const ExternalForwarding& object, const int& constant,
                          int& mutableValue, int&& temporary) {
        mutableValue += constant;
        return object.stored + constant + mutableValue + temporary;
    }
    static int eight(const ExternalForwarding& object,
                     int a, int b, int c, int d, int e, int f, int g, int h) noexcept {
        return object.stored + a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h;
    }
};

static_assert(Forwarding<NativeForwarding>);
static_assert(std::is_convertible_v<decltype(&dyn(Forwarding)::replace),
                           void (dyn(Forwarding)::*)(std::pair<int, int>)>);
static_assert(std::is_convertible_v<decltype(&dyn(Forwarding)::inspect),
                           int (dyn(Forwarding)::*)(const std::pair<int, int>&) const>);
static_assert(std::is_convertible_v<decltype(&dyn(Forwarding)::categories),
                           int (dyn(Forwarding)::*)(const int&, int&, int&&) const>);
static_assert(noexcept(std::declval<const dyn(Forwarding)&>().eight(1, 2, 3, 4, 5, 6, 7, 8)));

bool exerciseForwarding(dyn(Forwarding)& object)
{
    const auto& constant = object;
    if (constant.snapshot() != std::pair<int, int>{9, 10}) {
        return false;
    }
    std::pair<int, int> replacement{12, 13};
    object.replace(replacement);
    const std::pair<int, int> input{3, 4};
    int mutated = 5;
    constant.mutate(mutated);
    if (replacement != std::pair<int, int>{12, 13} ||
        constant.snapshot() != std::pair<int, int>{25, 26} ||
        constant.inspect(input) != 32 || mutated != 30) {
        return false;
    }

    auto byValue = std::make_unique<int>(41);
    object.consume(std::move(byValue));
    if (byValue || constant.snapshot().first != 41) {
        return false;
    }
    auto byRvalueReference = std::make_unique<int>(43);
    object.take(std::move(byRvalueReference));
    if (byRvalueReference || constant.snapshot().first != 43) {
        return false;
    }

    const int source = 2;
    int mutableValue = 3;
    return constant.categories(source, mutableValue, 4) == 54 && mutableValue == 5 &&
           source == 2 && constant.eight(1, 2, 3, 4, 5, 6, 7, 8) == 247;
}

int main()
{
    OrdinaryValue ordinary;
    dyn(Ordinary)& ordinaryInterface = ordinary;
    const dyn(Ordinary)& constantOrdinary = ordinary;
    if (constantOrdinary.value() != 17) {
        return 1;
    }
    ordinaryInterface.clear();
    if (constantOrdinary.value() != 0 || ordinary.stored != 0) {
        return 2;
    }

    declarations::nested::Native native;
    if (!exercise(native) || native.stored != 42) {
        return 3;
    }

    declarations::nested::External external;
    auto owned = adapt(declarations::nested::Operations, external);
    auto borrowed = borrow(declarations::nested::Operations, external);
    auto boxed = box(declarations::nested::Operations, external);
    external.stored = 5;
    if (owned.read() != 9 || boxed->read() != 9 || borrowed.read() != 5) {
        return 4;
    }
    if (!exercise(owned) || external.stored != 5) {
        return 5;
    }
    if (!exercise(*boxed) || external.stored != 5) {
        return 6;
    }
    if (!exercise(borrowed) || external.stored != 42) {
        return 7;
    }

    NativeForwarding nativeForwarding;
    if (!exerciseForwarding(nativeForwarding) || nativeForwarding.stored != 43) {
        return 8;
    }
    ExternalForwarding forwarding;
    auto ownedForwarding = adapt(Forwarding, forwarding);
    auto borrowedForwarding = borrow(Forwarding, forwarding);
    auto boxedForwarding = box(Forwarding, forwarding);
    if (!exerciseForwarding(ownedForwarding) || forwarding.stored != 9) {
        return 9;
    }
    if (!exerciseForwarding(*boxedForwarding) || forwarding.stored != 9) {
        return 10;
    }
    if (!exerciseForwarding(borrowedForwarding) || forwarding.stored != 43) {
        return 11;
    }
}
