#include <concepts>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>

#include "traits.hpp"

namespace calls {

trait(Fixed,
    method(const int&, read, const noexcept),
    method(int&, write, noexcept),
    method(int, select, (param(int, value)), const noexcept),
    method(int, select, (param(double, value)), const),
    method(void, consume, (param(std::unique_ptr<int>, token)), ),
    method(void, take, (param(std::unique_ptr<int>&&, token)), noexcept),
    default_method(int, doubled, (), const noexcept, { return read() * 2; })
);

// T must remain a user binding, not collide with generated receiver templates.
trait(Associated,
    associated_type(T),
    method(const T&, read, const noexcept),
    method(T&, write, noexcept),
    method(void, consume, (param(std::unique_ptr<T>, token)), ),
    method(void, take, (param(std::unique_ptr<T>&&, token)), noexcept)
);

trait(ReadOnly,
    associated_type(T),
    method(const T&, read, const noexcept)
);

trait(Concrete,
    method(int, read, const),
    concrete_method(clone, clone),
    concrete_method(merge, merge)
);

} // namespace calls

struct Native : implements(calls::Fixed) {
    int value = 7;
    Native() = default;
    Native(const Native&) = delete;
    Native(Native&&) = delete;
    const int& read() const noexcept override { return value; }
    // These tempting overloads must not replace the declared const overloads.
    int read() noexcept { return -100; }
    int& write() noexcept override { return value; }
    int select(int n) const noexcept override { return value + n; }
    int select(int) noexcept { return -101; }
    int select(double n) const noexcept override { return value + static_cast<int>(n) + 10; }
    void consume(std::unique_ptr<int> token) override { value = *token; }
    void take(std::unique_ptr<int>&& token) noexcept override {
        auto owned = std::move(token);
        value = *owned;
    }
    int extra() const noexcept { return 99; }
};

struct Derived : Native {
    const int& read() const noexcept override { return value; }
    int select(int n) const noexcept override { return value + n + 100; }
};

struct Mapped { int value = 8; };
impl(calls::Fixed, Mapped) {
    static const int& read(const Mapped& object) noexcept { return object.value; }
    static int read(Mapped&) noexcept { return -100; }
    static int& write(Mapped& object) noexcept { return object.value; }
    static int select(const Mapped& object, int n) noexcept { return object.value + n; }
    static int select(Mapped&, int) noexcept { return -101; }
    static int select(const Mapped& object, double n) noexcept {
        return object.value + static_cast<int>(n) + 10;
    }
    static void consume(Mapped& object, std::unique_ptr<int> token) { object.value = *token; }
    static void take(Mapped& object, std::unique_ptr<int>&& token) noexcept {
        auto owned = std::move(token);
        object.value = *owned;
    }
    static int extra(const Mapped&) { return 99; }
    static int doubled(const Mapped&) noexcept { return -1; }
};

struct Dual : Native { int mappedValue = 900; };
impl(calls::Fixed, Dual) {
    static const int& read(const Dual& object) noexcept { return object.mappedValue; }
    static int& write(Dual& object) noexcept { return object.mappedValue; }
    static int select(const Dual&, int) noexcept { return -200; }
    static int select(const Dual&, double) { return -201; }
    static void consume(Dual& object, std::unique_ptr<int> token) { object.mappedValue = *token; }
    static void take(Dual& object, std::unique_ptr<int>&& token) noexcept {
        object.mappedValue = *token;
        token.reset();
    }
};

struct AssociatedNative : implements(calls::Associated, int) {
    int value = 12;
    const int& read() const noexcept override { return value; }
    int read() noexcept { return -100; }
    int& write() noexcept override { return value; }
    void consume(std::unique_ptr<int> token) override { value = *token; }
    void take(std::unique_ptr<int>&& token) noexcept override {
        auto owned = std::move(token);
        value = *owned;
    }
};

struct NativeInvalidMapping : Native {};
impl(calls::Fixed, NativeInvalidMapping) {
    static void read() = delete;
};

struct AssociatedNativeInvalidMapping : AssociatedNative {};
impl(calls::Associated, AssociatedNativeInvalidMapping) {
    // Instantiating the associated mapping contract would form references to void.
    using T = void;
};

struct ReadOnlyMapped { int value = 73; };
impl(calls::ReadOnly, ReadOnlyMapped) {
    using T = int;
    static const T& read(const ReadOnlyMapped& object) noexcept { return object.value; }
};

struct AssociatedMapped { int value = 13; };
impl(calls::Associated, AssociatedMapped) {
    using T = int;
    static const T& read(const AssociatedMapped& object) noexcept { return object.value; }
    static int read(AssociatedMapped&) noexcept { return -100; }
    static T& write(AssociatedMapped& object) noexcept { return object.value; }
    static void consume(AssociatedMapped& object, std::unique_ptr<T> token) { object.value = *token; }
    static void take(AssociatedMapped& object, std::unique_ptr<T>&& token) noexcept {
        auto owned = std::move(token);
        object.value = *owned;
    }
};

struct AssociatedDual : AssociatedNative { long mappedValue = 901; };
impl(calls::Associated, AssociatedDual) {
    using T = long;
    static const T& read(const AssociatedDual& object) noexcept { return object.mappedValue; }
    static T& write(AssociatedDual& object) noexcept { return object.mappedValue; }
    static void consume(AssociatedDual& object, std::unique_ptr<T> token) { object.mappedValue = *token; }
    static void take(AssociatedDual& object, std::unique_ptr<T>&& token) noexcept {
        object.mappedValue = *token;
        token.reset();
    }
};

struct ConcreteMapped { int value = 1; };
impl(calls::Concrete, ConcreteMapped) {
    static int read(const ConcreteMapped& object) { return object.value; }
    static ConcreteMapped clone(const ConcreteMapped& object) { return object; }
    static void merge(ConcreteMapped& object, const ConcreteMapped& other) { object.value += other.value; }
};

struct Structural {
    const int& read() const noexcept;
    int& write() noexcept;
    int select(int) const noexcept;
    int select(double) const;
    void consume(std::unique_ptr<int>);
    void take(std::unique_ptr<int>&&) noexcept;
};
struct PrivateNative : private Native {};
struct Left : Native {};
struct Right : Native {};
struct Ambiguous : Left, Right {};
struct MissingMapping {};
struct InvalidMapping {};
impl(calls::Fixed, InvalidMapping) {
    // A method existing in a mapping is insufficient: the full contract is invalid.
    static int read(const InvalidMapping&) noexcept { return 0; }
};
struct AssociatedMissingAlias {};
impl(calls::Associated, AssociatedMissingAlias) {
    static const int& read(const AssociatedMissingAlias&) noexcept;
};
struct AssociatedInvalid {};
impl(calls::Associated, AssociatedInvalid) {
    using T = int;
    static const int& read(const AssociatedInvalid&); // Missing noexcept and other methods.
};
struct AssociatedStructural { using T = int; const int& read() const noexcept; };
struct AssociatedPrivate : private AssociatedNative {};
struct AssociatedLeft : AssociatedNative {};
struct AssociatedRight : AssociatedNative {};
struct AssociatedAmbiguous : AssociatedLeft, AssociatedRight {};
struct AssociatedShadowed : AssociatedNative { using T = long; };

template<class Object>
concept CanRead = requires(Object& object) { trait_call(calls::Fixed, read, object); };
template<class Object>
concept CanWrite = requires(Object& object) { trait_call(calls::Fixed, write, object); };
template<class Object>
concept CanExtra = requires(Object& object) { trait_call(calls::Fixed, extra, object); };
template<class Object>
concept CanDefault = requires(Object& object) { trait_call(calls::Fixed, doubled, object); };

template<class Object>
concept HasAssociatedMapping = requires { typename trait_mapping_t(calls::Associated, Object); };
template<class Object>
concept CanAssociatedRead = requires(Object& object) { trait_call(calls::Associated, read, object); };
template<class Object>
concept CanAssociatedWrite = requires(Object& object) { trait_call(calls::Associated, write, object); };
template<class Object>
concept CanClone = requires(Object& object) { trait_call(calls::Concrete, clone, object); };
template<class Object>
concept CanMerge = requires(Object& object) { trait_call(calls::Concrete, merge, object, object); };
template<class Object>
concept CanPassLvalueToken = requires(Object& object, std::unique_ptr<int>& token) {
    trait_call(calls::Fixed, take, object, token);
};
template<class Object>
concept CanPassMappedAssociatedToken = requires(Object& object, std::unique_ptr<long> token) {
    trait_call(calls::Associated, consume, object, std::move(token));
};

template<class Object>
constexpr bool fixedContract =
    CanRead<Object> && CanRead<const Object> && CanWrite<Object> && !CanWrite<const Object> &&
    std::same_as<decltype(trait_call(calls::Fixed, read, std::declval<Object&>())), const int&> &&
    std::same_as<decltype(trait_call(calls::Fixed, write, std::declval<Object&>())), int&> &&
    noexcept(trait_call(calls::Fixed, read, std::declval<const Object&>())) &&
    noexcept(trait_call(calls::Fixed, write, std::declval<Object&>())) &&
    noexcept(trait_call(calls::Fixed, select, std::declval<Object&>(), 1)) &&
    !noexcept(trait_call(calls::Fixed, select, std::declval<Object&>(), 1.0)) &&
    !noexcept(trait_call(calls::Fixed, consume, std::declval<Object&>(),
                        std::declval<std::unique_ptr<int>>())) &&
    noexcept(trait_call(calls::Fixed, take, std::declval<Object&>(),
                       std::declval<std::unique_ptr<int>&&>())) &&
    !CanPassLvalueToken<Object> && !CanExtra<Object> && !CanDefault<Object>;

using FixedBorrow = decltype(borrow(calls::Fixed, std::declval<Mapped&>()));
using FixedAdapt = decltype(adapt(calls::Fixed, std::declval<Mapped>()));
static_assert(fixedContract<Native> && fixedContract<Mapped> && fixedContract<Dual>);
static_assert(fixedContract<NativeInvalidMapping>);
static_assert(fixedContract<FixedBorrow> && fixedContract<FixedAdapt> && fixedContract<dyn(calls::Fixed)>);

static_assert(!CanRead<Structural> && !CanRead<PrivateNative> && !CanRead<Ambiguous>);
static_assert(!CanRead<MissingMapping> && !CanRead<InvalidMapping> && !CanRead<int>);
static_assert(!CanRead<volatile Native> && !CanRead<volatile Mapped>);
static_assert(calls::Concrete<ConcreteMapped>);
static_assert(!CanClone<ConcreteMapped> && !CanMerge<ConcreteMapped>);
using ConcreteBorrow = decltype(borrow(calls::Concrete, std::declval<ConcreteMapped&>()));
static_assert(!CanClone<ConcreteBorrow> && !CanMerge<ConcreteBorrow>);
static_assert(!CanClone<dyn(calls::Concrete)> && !CanMerge<dyn(calls::Concrete)>);

template<class Object>
constexpr bool associatedContract =
    CanAssociatedRead<Object> && CanAssociatedRead<const Object> &&
    CanAssociatedWrite<Object> && !CanAssociatedWrite<const Object> &&
    std::same_as<decltype(trait_call(calls::Associated, read, std::declval<Object&>())), const int&> &&
    std::same_as<decltype(trait_call(calls::Associated, write, std::declval<Object&>())), int&> &&
    noexcept(trait_call(calls::Associated, read, std::declval<const Object&>())) &&
    noexcept(trait_call(calls::Associated, write, std::declval<Object&>())) &&
    !noexcept(trait_call(calls::Associated, consume, std::declval<Object&>(),
                        std::declval<std::unique_ptr<int>>())) &&
    noexcept(trait_call(calls::Associated, take, std::declval<Object&>(),
                       std::declval<std::unique_ptr<int>&&>()));

using AssociatedBorrow = decltype(borrow(calls::Associated, std::declval<AssociatedMapped&>()));
using AssociatedAdapt = decltype(adapt(calls::Associated, std::declval<AssociatedMapped>()));
static_assert(associatedContract<AssociatedNative> && associatedContract<AssociatedMapped>);
static_assert(associatedContract<AssociatedNativeInvalidMapping>);
using ConstAssociatedBorrow = decltype(borrow(calls::ReadOnly, std::declval<const ReadOnlyMapped&>()));
static_assert(std::same_as<decltype(trait_call(calls::ReadOnly, read,
                                            std::declval<const ConstAssociatedBorrow&>())), const int&>);
static_assert(noexcept(trait_call(calls::ReadOnly, read, std::declval<const ConstAssociatedBorrow&>())));
static_assert(associatedContract<AssociatedDual> && associatedContract<dyn(calls::Associated, int)>);
static_assert(associatedContract<AssociatedBorrow> && associatedContract<AssociatedAdapt>);
static_assert(!HasAssociatedMapping<AssociatedNative> && HasAssociatedMapping<AssociatedMapped>);
static_assert(!HasAssociatedMapping<AssociatedBorrow> && !HasAssociatedMapping<AssociatedAdapt>);
static_assert(std::same_as<trait_mapping_t(calls::Associated, AssociatedDual)::T, long>);
static_assert(!CanPassMappedAssociatedToken<AssociatedDual>);
static_assert(!CanAssociatedRead<AssociatedMissingAlias> && !CanAssociatedRead<AssociatedInvalid>);
static_assert(!CanAssociatedRead<AssociatedStructural> && !CanAssociatedRead<AssociatedPrivate>);
static_assert(!CanAssociatedRead<AssociatedAmbiguous> && !CanAssociatedRead<AssociatedShadowed>);
static_assert(!CanAssociatedRead<int> && !CanAssociatedRead<volatile AssociatedNative>);

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

template<class Object>
void exerciseFixed(Object& object) {
    int evaluations = 0;
    int& reference = trait_call(calls::Fixed, write, (++evaluations, object));
    reference = 20;
    const auto& constant = object;
    check(evaluations == 1, "fixed receiver must be evaluated once");
    check(&trait_call(calls::Fixed, read, object) == &reference, "fixed read must preserve reference identity");
    check(trait_call(calls::Fixed, read, constant) == 20, "fixed const receiver must be readable");
    check(trait_call(calls::Fixed, select, object, 2) == 22, "fixed call must select declared const int overload");
    check(trait_call(calls::Fixed, select, object, 2.0) == 32, "fixed call must select declared double overload");
    auto byValue = std::make_unique<int>(31);
    trait_call(calls::Fixed, consume, object, std::move(byValue));
    check(!byValue && reference == 31, "fixed by-value move-only argument must transfer ownership");
    auto byReference = std::make_unique<int>(41);
    trait_call(calls::Fixed, take, object, std::move(byReference));
    check(!byReference && reference == 41, "fixed rvalue reference must reach the original token");
}

template<class Object>
void exerciseAssociated(Object& object) {
    int evaluations = 0;
    int& reference = trait_call(calls::Associated, write, (++evaluations, object));
    reference = 50;
    const auto& constant = object;
    check(evaluations == 1, "associated receiver must be evaluated once");
    check(&trait_call(calls::Associated, read, object) == &reference, "associated read must preserve reference identity");
    check(trait_call(calls::Associated, read, constant) == 50, "associated const receiver must be readable");
    auto byValue = std::make_unique<int>(51);
    trait_call(calls::Associated, consume, object, std::move(byValue));
    check(!byValue && reference == 51, "associated by-value move-only argument must transfer ownership");
    auto byReference = std::make_unique<int>(61);
    trait_call(calls::Associated, take, object, std::move(byReference));
    check(!byReference && reference == 61, "associated rvalue reference must reach the original token");
}
} // namespace

int main() {
    Native native; // Noncopyable and nonmovable: direct calls need no adapter.
    Mapped mapped;
    Dual dual;
    exerciseFixed(native);
    exerciseFixed(mapped);
    exerciseFixed(dual);
    NativeInvalidMapping nativeInvalidMapping;
    exerciseFixed(nativeInvalidMapping);
    check(dual.mappedValue == 900, "native fixed implementation must take precedence over mapping");
    auto borrowed = borrow(calls::Fixed, mapped);
    auto owned = adapt(calls::Fixed, Mapped{});
    exerciseFixed(borrowed);
    exerciseFixed(owned);
    dyn(calls::Fixed)& erased = borrowed;
    exerciseFixed(erased);
    check(mapped.value == 41, "borrowed and erased calls must update original mapped object");

    Derived derived;
    Native& base = derived;
    const dyn(calls::Fixed)& interface = derived;
    check(trait_call(calls::Fixed, select, base, 3) == 110, "native base call must retain virtual dispatch");
    check(trait_call(calls::Fixed, select, interface, 3) == 110, "dyn call must retain virtual dispatch");

    AssociatedNative associatedNative;
    AssociatedMapped associatedMapped;
    AssociatedDual associatedDual;
    exerciseAssociated(associatedNative);
    exerciseAssociated(associatedMapped);
    exerciseAssociated(associatedDual);
    AssociatedNativeInvalidMapping associatedNativeInvalidMapping;
    exerciseAssociated(associatedNativeInvalidMapping);
    check(associatedDual.mappedValue == 901, "native associated bindings must take precedence over mapping bindings");
    auto associatedBorrow = borrow(calls::Associated, associatedMapped);
    auto associatedOwn = adapt(calls::Associated, AssociatedMapped{});
    exerciseAssociated(associatedBorrow);
    exerciseAssociated(associatedOwn);
    dyn(calls::Associated, int)& associatedErased = associatedBorrow;
    exerciseAssociated(associatedErased);
    check(associatedMapped.value == 61, "associated borrowed and erased calls must update source");
    const ReadOnlyMapped readOnlyMapped;
    const auto constBorrow = borrow(calls::ReadOnly, readOnlyMapped);
    check(&trait_call(calls::ReadOnly, read, constBorrow) == &readOnlyMapped.value,
          "const associated borrow must use native adapter path and preserve reference identity");
    const dyn(calls::ReadOnly, int)& readOnlyErased = constBorrow;
    check(trait_call(calls::ReadOnly, read, readOnlyErased) == 73,
          "readonly associated dyn receiver must dispatch to const borrowed source");
    return failures == 0 ? 0 : 1;
}
