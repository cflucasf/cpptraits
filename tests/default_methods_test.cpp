#include "traits.hpp"

#include <concepts>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>

trait(DefaultCounter,
    method(int, get, const noexcept),
    method(void, set, (param(int, value)), noexcept),
    default_method(int, doubled, (), const noexcept, { return get() * 2; }),
    default_method(int, chained, (), const noexcept, { return doubled() + get(); }),
    default_method(void, increment, (param(int, amount)), noexcept, {
        set(get() + amount);
    }),
    default_method((std::pair<int, int>), snapshot, (), const noexcept, {
        const int first = get(), second = doubled();
        return {first, second};
    })
);

trait(DefaultOnly,
    default_method(int, answer, (), const noexcept, { return 42; }),
    default_method(int, nextAnswer, (), const noexcept, { return answer() + 1; })
);

composed_trait(DefaultBundle, T, DefaultCounter, DefaultOnly);
composed_trait(CounterBundle, T, DefaultCounter);
composed_trait(DefaultOnlyBundle, T, DefaultOnly);

struct NativeCounter : implements(DefaultCounter), implements(DefaultOnly) {
    int value = 5;
    int get() const noexcept override { return value; }
    void set(int next) noexcept override { value = next; }
};

struct OverridingCounter : NativeCounter {
    int doubled() const noexcept override { return get() * 10; }
    int answer() const noexcept override { return 70; }
};

// Having all the right members is not nominal trait participation.
struct StructuralCounter {
    int get() const noexcept { return 5; }
    void set(int) noexcept {}
    int doubled() const noexcept { return 10; }
    int chained() const noexcept { return 15; }
    void increment(int) noexcept {}
    std::pair<int, int> snapshot() const noexcept { return {5, 10}; }
};

struct ExternalCounter {
    int value;
};

// Defaults must not require corresponding static functions in the mapping.
impl(DefaultCounter, ExternalCounter) {
    static int get(const ExternalCounter& object) noexcept { return object.value; }
    static void set(ExternalCounter& object, int value) noexcept { object.value = value; }
};
impl(DefaultOnly, ExternalCounter) {};
impl(DefaultOnly, int) {};

struct StaticDefaultTrap {
    int value;
};
impl(DefaultCounter, StaticDefaultTrap) {
    static int get(const StaticDefaultTrap& object) noexcept { return object.value; }
    static void set(StaticDefaultTrap& object, int value) noexcept { object.value = value; }
    static int doubled(const StaticDefaultTrap&) noexcept { return -999; }
    static void increment(StaticDefaultTrap& object, int amount) noexcept {
        object.value += amount * 10;
    }
};

struct MissingRequired {};
impl(DefaultCounter, MissingRequired) {
    static int get(const MissingRequired&) noexcept { return 1; }
};
struct NoMapping {};
struct StructuralDefaultOnly {
    int answer() const noexcept { return 42; }
    int nextAnswer() const noexcept { return 43; }
};

template<typename T>
concept CanBorrowCounter = requires(T& object) { borrow(DefaultCounter, object); };
template<typename T>
concept CanBorrowDefaultOnly = requires(T& object) { borrow(DefaultOnly, object); };
template<typename T>
concept CanAdaptDefaultOnly = requires(T&& object) {
    adapt(DefaultOnly, std::forward<T>(object));
};
template<typename T>
concept CanBoxDefaultOnly = requires(T&& object) {
    box(DefaultOnly, std::forward<T>(object));
};

template<typename T>
concept CanBorrowDefaultBundle = requires(T& object) { borrow(DefaultBundle, object); };
template<typename T>
concept CanAdaptDefaultBundle = requires(T&& object) {
    adapt(DefaultBundle, std::forward<T>(object));
};
template<typename T>
concept CanBoxDefaultBundle = requires(T&& object) {
    box(DefaultBundle, std::forward<T>(object));
};

using OwnedCounter = decltype(adapt(DefaultCounter, std::declval<ExternalCounter>()));
using BorrowedCounter = decltype(borrow(DefaultCounter, std::declval<ExternalCounter&>()));

static_assert(DefaultCounter<NativeCounter>);
static_assert(DefaultCounter<OverridingCounter>);
static_assert(DefaultCounter<OwnedCounter> && DefaultCounter<BorrowedCounter>);
static_assert(!DefaultCounter<StructuralCounter>);
static_assert(DefaultCounter<ExternalCounter> && !DefaultCounter<const ExternalCounter>);
static_assert(DefaultOnly<ExternalCounter> && DefaultOnly<const ExternalCounter>);
static_assert(DefaultBundle<ExternalCounter> && !DefaultBundle<const ExternalCounter>);
static_assert(!std::convertible_to<ExternalCounter&, dyn(DefaultCounter)&>);
static_assert(!DefaultCounter<MissingRequired> && !DefaultCounter<NoMapping>);
static_assert(!CanBorrowDefaultBundle<ExternalCounter>);
static_assert(!CanAdaptDefaultBundle<ExternalCounter>);
static_assert(!CanBoxDefaultBundle<ExternalCounter>);
static_assert(CanBorrowDefaultBundle<NativeCounter>);
static_assert(CanAdaptDefaultBundle<NativeCounter>);
static_assert(CanBoxDefaultBundle<NativeCounter>);
static_assert(DefaultOnly<int> && DefaultOnly<const int>);
static_assert(!DefaultOnly<StructuralDefaultOnly> && !DefaultOnly<NoMapping>);
static_assert(!std::convertible_to<int&, dyn(DefaultOnly)&>);
static_assert(DefaultOnly<dyn(DefaultOnly)>);
static_assert(!std::is_abstract_v<dyn(DefaultOnly)>);
static_assert(std::is_abstract_v<dyn(DefaultCounter)>);
static_assert(CanBorrowCounter<ExternalCounter>);
static_assert(!CanBorrowCounter<MissingRequired>);
static_assert(!CanBorrowCounter<StructuralCounter>);
static_assert(CanBorrowDefaultOnly<int>);
static_assert(CanAdaptDefaultOnly<int> && CanBoxDefaultOnly<int>);
static_assert(!CanBorrowDefaultOnly<NoMapping>);
static_assert(!CanAdaptDefaultOnly<NoMapping> && !CanBoxDefaultOnly<NoMapping>);
static_assert(!CanBorrowDefaultOnly<StructuralDefaultOnly>);

static_assert(std::is_convertible_v<decltype(&dyn(DefaultCounter)::snapshot),
    std::pair<int, int> (dyn(DefaultCounter)::*)() const noexcept>);
static_assert(noexcept(std::declval<const OwnedCounter&>().doubled()));
static_assert(noexcept(std::declval<const BorrowedCounter&>().snapshot()));
static_assert(noexcept(std::declval<BorrowedCounter&>().increment(1)));

// These defaults exercise wrapped comma types, reference identity, move-only
// arguments, and a body whose commas are not protected by parentheses.
trait(DefaultArguments,
    default_method((std::pair<int, int>), combine,
        (param((const std::pair<int, int>&), input), param(int&, total)), const noexcept, {
            const int left = input.first, right = input.second;
            total += left + right;
            return {total, left - right};
        }),
    default_method(int&, identity, (param(int&, value)), const noexcept, { return value; }),
    default_method(std::unique_ptr<int>, consume,
        (param(std::unique_ptr<int>, value)), const, { return value; }),
    default_method(std::unique_ptr<int>, take,
        (param(std::unique_ptr<int>&&, value)), const, { return std::move(value); }),
    default_method((std::pair<int, int>), echo,
        (param((std::pair<int, int>), value)), const noexcept, { return value; }),
    default_method(const int&, constantIdentity, (param(const int&, value)), const noexcept, {
        return value;
    }),
    default_method(int&&, rvalueIdentity, (param(int&&, value)), const noexcept, {
        return std::move(value);
    })
);

struct NativeArguments : implements(DefaultArguments) {};
struct ExternalArguments {};
impl(DefaultArguments, ExternalArguments) {};

struct OverriddenArguments {
    int marker = 100;
};
impl(DefaultArguments, OverriddenArguments) {
    static std::pair<int, int> combine(const OverriddenArguments& object,
                                     const std::pair<int, int>& input, int& total) noexcept {
        total += object.marker + input.first + input.second;
        return {total, input.first - input.second};
    }
    static int& identity(const OverriddenArguments&, int& value) noexcept {
        value += 100;
        return value;
    }
    // A noexcept mapping is compatible with a potentially throwing contract.
    static std::unique_ptr<int> consume(const OverriddenArguments& object,
                                       std::unique_ptr<int> value) noexcept {
        *value += object.marker;
        return value;
    }
    static std::unique_ptr<int> take(const OverriddenArguments& object,
                                    std::unique_ptr<int>&& value) noexcept {
        *value += object.marker;
        return std::move(value);
    }
    static std::pair<int, int> echo(const OverriddenArguments& object,
                                  std::pair<int, int> value) noexcept {
        value.first += object.marker;
        return value;
    }
    static const int& constantIdentity(const OverriddenArguments& object,
                                       const int&) noexcept { return object.marker; }
    static int&& rvalueIdentity(const OverriddenArguments&, int&& value) noexcept {
        value += 100;
        return std::move(value);
    }
};
composed_trait(ArgumentsBundle, T, DefaultArguments);

using OwnedOverrideArguments = decltype(adapt(DefaultArguments,
                                             std::declval<OverriddenArguments>()));
static_assert(!noexcept(std::declval<const OwnedOverrideArguments&>().consume(nullptr)));
static_assert(!noexcept(std::declval<const OwnedOverrideArguments&>().take(
    std::declval<std::unique_ptr<int>&&>())));
static_assert(noexcept(std::declval<const OwnedOverrideArguments&>().identity(
    std::declval<int&>())));
static_assert(std::same_as<decltype(std::declval<const OwnedOverrideArguments&>().identity(
    std::declval<int&>())), int&>);
static_assert(std::same_as<decltype(std::declval<const OwnedOverrideArguments&>().constantIdentity(
    std::declval<const int&>())), const int&>);
static_assert(std::same_as<decltype(std::declval<const OwnedOverrideArguments&>().rvalueIdentity(
    std::declval<int&&>())), int&&>);

// Invalid optional names must fail factory constraints, not fail in adapter bodies.
trait(OptionalProbe,
    default_method(int, probe, (param(int, value)), const noexcept, { return value; })
);
struct WrongReturn {};
impl(OptionalProbe, WrongReturn) {
    static long probe(const WrongReturn&, int) noexcept { return 0; }
};
struct WrongArgument {};
impl(OptionalProbe, WrongArgument) {
    static int probe(const WrongArgument&, long) noexcept { return 0; }
};
struct WrongConst {};
impl(OptionalProbe, WrongConst) {
    static int probe(WrongConst&, int) noexcept { return 0; }
};
struct WrongNoexcept {};
impl(OptionalProbe, WrongNoexcept) {
    static int probe(const WrongNoexcept&, int) { return 0; }
};
struct NonstaticOptional {};
impl(OptionalProbe, NonstaticOptional) {
    int probe(const NonstaticOptional&, int) const noexcept { return 0; }
};
struct PrivateOptional {};
impl(OptionalProbe, PrivateOptional) {
private:
    static int probe(const PrivateOptional&, int) noexcept { return 0; }
};
struct DataOptional {};
impl(OptionalProbe, DataOptional) {
    static inline int probe = 0;
};
struct DeletedOptional {};
impl(OptionalProbe, DeletedOptional) {
    static int probe(const DeletedOptional&, int) noexcept = delete;
};
struct UnmatchedOverloads {};
impl(OptionalProbe, UnmatchedOverloads) {
    static int probe(const UnmatchedOverloads&, long) noexcept { return 0; }
    static int probe(const UnmatchedOverloads&, double) noexcept { return 0; }
};
struct MatchingOverloads {};
impl(OptionalProbe, MatchingOverloads) {
    static int probe(const MatchingOverloads&, int value) noexcept { return value + 100; }
    static int probe(const MatchingOverloads&, double) noexcept { return -1; }
};
struct FinalFull {};
impl(OptionalProbe, FinalFull) final {
    static int probe(const FinalFull&, int value) noexcept { return value + 200; }
};
struct FinalMissing {};
impl(OptionalProbe, FinalMissing) final {};
struct FinalInvalid {};
impl(OptionalProbe, FinalInvalid) final {
    static int probe(const FinalInvalid&, int value) { return value; }
};
struct MissingOptional {};
impl(OptionalProbe, MissingOptional) {};

template<typename T>
concept CanAdaptProbe = requires(T&& object) {
    adapt(OptionalProbe, std::forward<T>(object));
};
template<typename T>
concept CanBorrowProbe = requires(T& object) { borrow(OptionalProbe, object); };
template<typename T>
concept CanBoxProbe = requires(T&& object) { box(OptionalProbe, std::forward<T>(object)); };
template<typename T>
constexpr bool probeRejected =
    !OptionalProbe<T> && !CanAdaptProbe<T> && !CanBorrowProbe<T> && !CanBoxProbe<T>;
template<typename T>
constexpr bool probeAccepted =
    OptionalProbe<T> && CanAdaptProbe<T> && CanBorrowProbe<T> && CanBoxProbe<T>;

static_assert(probeRejected<WrongReturn>);
static_assert(probeRejected<WrongArgument>);
static_assert(probeRejected<WrongConst>);
static_assert(probeRejected<WrongNoexcept>);
static_assert(probeRejected<NonstaticOptional>);
static_assert(probeRejected<PrivateOptional>);
static_assert(probeRejected<DataOptional>);
static_assert(probeRejected<DeletedOptional>);
static_assert(probeRejected<UnmatchedOverloads>);
static_assert(probeRejected<FinalMissing>);
static_assert(probeRejected<FinalInvalid>);
static_assert(probeAccepted<MatchingOverloads>);
static_assert(probeAccepted<FinalFull>);
static_assert(probeAccepted<MissingOptional>);
static_assert(probeRejected<NoMapping>);

trait(OverloadedDefaults,
    default_method(int, lookup, (param(int, value)), const noexcept, { return value; }),
    default_method(double, lookup, (param(double, value)), const, { return value; })
);
struct FullOverloadFamily {};
impl(OverloadedDefaults, FullOverloadFamily) {
    static int lookup(const FullOverloadFamily&, int value) noexcept { return value + 10; }
    static double lookup(const FullOverloadFamily&, double value) noexcept { return value + 20; }
};
struct PartialOverloadFamily {};
impl(OverloadedDefaults, PartialOverloadFamily) {
    static int lookup(const PartialOverloadFamily&, int value) noexcept { return value; }
};
struct MissingOverloadFamily {};
impl(OverloadedDefaults, MissingOverloadFamily) {};

template<typename T>
concept CanAdaptOverloads = requires(T& object) { adapt(OverloadedDefaults, object); };
template<typename T>
concept CanBorrowOverloads = requires(T& object) { borrow(OverloadedDefaults, object); };
template<typename T>
concept CanBoxOverloads = requires(T& object) { box(OverloadedDefaults, object); };
template<typename T>
constexpr bool acceptsOverloadFamily =
    CanAdaptOverloads<T> && CanBorrowOverloads<T> && CanBoxOverloads<T>;
static_assert(OverloadedDefaults<FullOverloadFamily>);
static_assert(OverloadedDefaults<MissingOverloadFamily>);
static_assert(!OverloadedDefaults<PartialOverloadFamily>);
static_assert(acceptsOverloadFamily<FullOverloadFamily>);
static_assert(acceptsOverloadFamily<MissingOverloadFamily>);
static_assert(!CanAdaptOverloads<PartialOverloadFamily>);
static_assert(!CanBorrowOverloads<PartialOverloadFamily>);
static_assert(!CanBoxOverloads<PartialOverloadFamily>);

static_assert(std::is_convertible_v<decltype(&dyn(DefaultArguments)::combine),
    std::pair<int, int> (dyn(DefaultArguments)::*)(const std::pair<int, int>&, int&)
        const noexcept>);
static_assert(std::is_convertible_v<decltype(&dyn(DefaultArguments)::identity),
    int& (dyn(DefaultArguments)::*)(int&) const noexcept>);

namespace {
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

void nativeDefaults()
{
    NativeCounter native;
    dyn(DefaultCounter)& interface = native;
    check(interface.doubled() == 10 && interface.chained() == 15,
          "native inherited defaults call the required virtual getter");
    interface.increment(3);
    check(native.value == 8 && interface.snapshot() == std::pair<int, int>{8, 16},
          "mutable native default calls the required virtual setter");

    OverridingCounter overriding;
    dyn(DefaultCounter)& overridden = overriding;
    check(overridden.doubled() == 50 && overridden.chained() == 55 &&
              overridden.snapshot() == std::pair<int, int>{5, 50},
          "dyn dispatch and inherited default bodies honor a native default override");
    overridden.set(9);
    check(overridden.chained() == 99, "default chaining reads current native state");

    auto borrowed = borrow(DefaultBundle, overriding);
    auto owned = adapt(DefaultBundle, overriding);
    auto boxed = box(DefaultBundle, overriding);
    overriding.value = 11;
    check(borrowed.as<dyn(DefaultCounter)>().chained() == 121,
          "composed borrow preserves native default overrides and source state");
    check(owned.as<dyn(DefaultCounter)>().chained() == 99 &&
              boxed->as<dyn(DefaultCounter)>().chained() == 99,
          "composed adapt and box preserve native default overrides in their copies");
    check(borrowed.as<dyn(DefaultOnly)>().nextAnswer() == 71 &&
              owned.as<dyn(DefaultOnly)>().nextAnswer() == 71 &&
              boxed->as<dyn(DefaultOnly)>().nextAnswer() == 71,
          "concrete default-only component supports composition and virtual chaining");
}

void externalDefaults()
{
    ExternalCounter external{3};
    auto borrowed = borrow(DefaultCounter, external);
    auto owned = adapt(DefaultCounter, external);
    auto boxed = box(DefaultCounter, external);
    external.value = 7;
    check(borrowed.chained() == 21 && owned.chained() == 9 && boxed->chained() == 9,
          "required-only mappings inherit defaults with correct borrow/own semantics");
    borrowed.increment(2);
    owned.increment(10);
    boxed->increment(20);
    check(external.value == 9 && borrowed.snapshot() == std::pair<int, int>{9, 18} &&
              owned.snapshot() == std::pair<int, int>{13, 26} &&
              boxed->snapshot() == std::pair<int, int>{23, 46},
          "external mutable defaults dispatch to each adapter's required setter");

    // Composition selects nominal components from ordinary external adapters,
    // not directly from the mapped external object.
    auto composedBorrow = borrow(CounterBundle, borrowed);
    auto composedOwned = adapt(CounterBundle, owned);
    // The ordinary box erases to an abstract interface; owning composition
    // needs the concrete, copyable adapter rather than that erased base.
    auto composedBox = box(CounterBundle, dynamic_cast<OwnedCounter&>(*boxed));
    composedBorrow.as<dyn(DefaultCounter)>().increment(1);
    check(external.value == 10 && composedBorrow.as<dyn(DefaultCounter)>().chained() == 30 &&
              composedOwned.as<dyn(DefaultCounter)>().chained() == 39 &&
              composedBox->as<dyn(DefaultCounter)>().chained() == 69,
          "composed external adapters inherit required-backed defaults");
    auto defaultOnlyAdapter = adapt(DefaultOnly, external);
    auto defaultOnlyBorrow = borrow(DefaultOnlyBundle, defaultOnlyAdapter);
    auto defaultOnlyOwned = adapt(DefaultOnlyBundle, defaultOnlyAdapter);
    auto defaultOnlyBox = box(DefaultOnlyBundle, defaultOnlyAdapter);
    check(defaultOnlyBorrow.as<dyn(DefaultOnly)>().nextAnswer() == 43 &&
              defaultOnlyOwned.as<dyn(DefaultOnly)>().nextAnswer() == 43 &&
              defaultOnlyBox->as<dyn(DefaultOnly)>().nextAnswer() == 43,
          "composed external adapters accept a concrete default-only component");

    StaticDefaultTrap trap{4};
    auto trapBorrow = borrow(DefaultCounter, trap);
    auto trapOwned = adapt(DefaultCounter, trap);
    auto trapBox = box(DefaultCounter, trap);
    auto exerciseOverride = [](dyn(DefaultCounter)& object, int value) {
        check(object.doubled() == -999 && object.chained() == -999 + value &&
                  object.snapshot() == std::pair<int, int>{value, -999},
              "external overrides dispatch directly and from qualified fallback bodies");
        object.increment(2);
        check(object.get() == value + 20 && object.chained() == -999 + value + 20,
              "parameterized mutable optional overrides dispatch through dyn");
    };
    exerciseOverride(trapBorrow, 4);
    exerciseOverride(trapOwned, 4);
    exerciseOverride(*trapBox, 4);
    check(trap.value == 24, "mutable external override updates the borrowed source");

    auto overrideBorrow = borrow(CounterBundle, trapBorrow);
    auto overrideOwned = adapt(CounterBundle, trapOwned);
    auto overrideBox = box(CounterBundle, trapOwned);
    exerciseOverride(overrideBorrow.as<dyn(DefaultCounter)>(), 24);
    exerciseOverride(overrideOwned.as<dyn(DefaultCounter)>(), 24);
    exerciseOverride(overrideBox->as<dyn(DefaultCounter)>(), 24);
    check(trap.value == 44 && trapOwned.get() == 24,
          "composed external overrides preserve borrow and copy semantics");
}

void defaultOnly()
{
    dyn(DefaultOnly) concrete;
    check(concrete.answer() == 42 && concrete.nextAnswer() == 43,
          "an all-default ordinary interface is directly concrete");
    int source = 17;
    auto borrowed = borrow(DefaultOnly, source);
    auto owned = adapt(DefaultOnly, source);
    auto boxed = box(DefaultOnly, source);
    check(borrowed.nextAnswer() == 43 && owned.nextAnswer() == 43 &&
              boxed->nextAnswer() == 43 && source == 17,
          "an empty explicit mapping enables default-only borrow, adapt and box");
}

void exerciseArguments(const dyn(DefaultArguments)& object)
{
    const std::pair<int, int> input{7, 3};
    int total = 2;
    check(object.combine(input, total) == std::pair<int, int>{12, 4} && total == 12 &&
              input == std::pair<int, int>{7, 3},
          "parameterized defaults preserve wrapped const references and body commas");
    check(&object.identity(total) == &total, "default reference return preserves identity");
    object.identity(total) = 19;
    check(total == 19, "default reference return remains mutable");
    int&& reference = object.rvalueIdentity(std::move(total));
    check(&object.constantIdentity(total) == &total && &reference == &total,
          "default const-reference and rvalue-reference returns preserve identity");
    check(object.echo({8, 9}) == std::pair<int, int>{8, 9},
          "wrapped comma parameter types support value arguments");
    auto first = std::make_unique<int>(31);
    auto firstResult = object.consume(std::move(first));
    check(!first && firstResult && *firstResult == 31,
          "default by-value parameters and returns support move-only values");
    auto second = std::make_unique<int>(37);
    auto secondResult = object.take(std::move(second));
    check(!second && secondResult && *secondResult == 37,
          "default rvalue-reference parameters support move-only values");
}
void exerciseArgumentOverrides(const dyn(DefaultArguments)& object)
{
    const std::pair<int, int> input{7, 3};
    int total = 2;
    check(object.combine(input, total) == std::pair<int, int>{112, 4} && total == 112 &&
              input == std::pair<int, int>{7, 3},
          "optional overrides preserve wrapped const-reference and mutable-reference parameters");
    check(&object.identity(total) == &total && total == 212,
          "optional reference return preserves identity and dispatches override");
    object.identity(total) = 19;
    check(total == 19, "optional reference return remains mutable");
    const int& constant = object.constantIdentity(total);
    check(constant == 100 && &constant != &total &&
              &constant == &object.constantIdentity(total),
          "optional const-reference return refers to stable implementation state");
    int&& reference = object.rvalueIdentity(std::move(total));
    check(&reference == &total && total == 119,
          "optional rvalue-reference return preserves identity and dispatches override");
    check(object.echo({8, 9}) == std::pair<int, int>{108, 9},
          "optional wrapped by-value parameter dispatches override");
    auto first = std::make_unique<int>(31);
    auto firstResult = object.consume(std::move(first));
    check(!first && firstResult && *firstResult == 131,
          "noexcept optional override forwards move-only by-value arguments and returns");
    auto second = std::make_unique<int>(37);
    auto secondResult = object.take(std::move(second));
    check(!second && secondResult && *secondResult == 137,
          "noexcept optional override forwards move-only rvalue-reference arguments");
}

void argumentOverrides()
{
    OverriddenArguments source;
    auto borrowed = borrow(DefaultArguments, source);
    auto owned = adapt(DefaultArguments, source);
    auto boxed = box(DefaultArguments, source);
    exerciseArgumentOverrides(borrowed);
    exerciseArgumentOverrides(owned);
    exerciseArgumentOverrides(*boxed);
    auto composedBorrow = borrow(ArgumentsBundle, borrowed);
    auto composedOwned = adapt(ArgumentsBundle, owned);
    auto composedBox = box(ArgumentsBundle, owned);
    exerciseArgumentOverrides(composedBorrow.as<dyn(DefaultArguments)>());
    exerciseArgumentOverrides(composedOwned.as<dyn(DefaultArguments)>());
    exerciseArgumentOverrides(composedBox->as<dyn(DefaultArguments)>());
}

template<typename T>
void exerciseProbe(int expected)
{
    T source;
    auto borrowed = borrow(OptionalProbe, source);
    auto owned = adapt(OptionalProbe, source);
    auto boxed = box(OptionalProbe, source);
    const dyn(OptionalProbe)& interface = borrowed;
    check(interface.probe(3) == expected && owned.probe(3) == expected &&
              boxed->probe(3) == expected,
          "optional overload resolution, final full mapping and missing-name fallback dispatch");
}

void overloadedDefaults()
{
    FullOverloadFamily source;
    auto borrowed = borrow(OverloadedDefaults, source);
    auto owned = adapt(OverloadedDefaults, source);
    auto boxed = box(OverloadedDefaults, source);
    auto exercise = [](const dyn(OverloadedDefaults)& object) {
        check(object.lookup(2) == 12 && object.lookup(0.5) == 20.5,
              "each optional overload dispatches its exact compatible static function");
    };
    exercise(borrowed);
    exercise(owned);
    exercise(*boxed);
    MissingOverloadFamily missing;
    auto fallback = adapt(OverloadedDefaults, missing);
    check(fallback.lookup(2) == 2 && fallback.lookup(0.5) == 0.5,
          "an entirely absent overload-family name falls back without recursion");
}
} // namespace

int main()
{
    nativeDefaults();
    externalDefaults();
    defaultOnly();
    argumentOverrides();
    exerciseProbe<MatchingOverloads>(103);
    exerciseProbe<FinalFull>(203);
    exerciseProbe<MissingOptional>(3);
    overloadedDefaults();

    NativeArguments native;
    ExternalArguments external;
    auto borrowed = borrow(DefaultArguments, external);
    auto owned = adapt(DefaultArguments, external);
    auto boxed = box(DefaultArguments, external);
    exerciseArguments(native);
    exerciseArguments(borrowed);
    exerciseArguments(owned);
    exerciseArguments(*boxed);
    return failures == 0 ? 0 : 1;
}
