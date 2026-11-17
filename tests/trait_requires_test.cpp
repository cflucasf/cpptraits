#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

#include "traits.hpp"

#ifdef Self
#error Self must remain an ordinary user-selected parameter identifier
#endif

static_trait(Enabled, Object, requires {
    typename Object::X;
    requires Object::enabled;
});

// Eight descriptors, with requirements interleaved among ordinary descriptors.
trait(Checked,
    trait_requires(Self, Enabled<Self>),
    method(int, read, const noexcept),
    trait_requires(Candidate, std::same_as<typename Candidate::X, int>),
    default_method(int, doubled, (), const, { return read() * 2; }),
    trait_requires(Object, std::copy_constructible<Object>),
    concrete_method(duplicate, clone),
    trait_requires(Value, requires(const Value& value) {
        { value.token() } -> std::same_as<int>;
    }),
    concrete_method(combine, merge)
);

// Associated binding T must not collide with generated requirement parameters.
trait(Associated,
    trait_requires(Subject, Enabled<Subject>),
    associated_type(T),
    trait_requires(Self, std::same_as<typename Self::X, int>),
    method(T, read, const noexcept),
    trait_requires(T, std::copy_constructible<T> &&
                      !std::is_const_v<T> && !std::is_reference_v<T>)
);

trait(NativeRead,
    method(int, read, const noexcept),
    trait_requires(Concrete, Enabled<Concrete>),
    trait_requires(Self, std::same_as<typename Self::X, int>),
    trait_requires(Object, std::copy_constructible<Object>)
);
trait(DirectAlias,
    trait_requires(Self, std::same_as<typename Self::X, int>),
    method(int, read, const noexcept)
);
trait(Other, method(int, other, const noexcept));
composed_trait(Bundle, Object, NativeRead, Other);

// Structural predicates deliberately mention properties absent from adapters.
template<class Alias, bool IsEnabled>
struct Payload {
    using X = Alias;
    static constexpr bool enabled = IsEnabled;
    int value = 7;
    int token() const { return 1; }
};
using Good = Payload<int, true>;
using Disabled = Payload<int, false>;
using WrongAlias = Payload<long, true>;
struct Missing { int value = 7; int token() const { return 1; } };
struct MissingToken {
    using X = int;
    static constexpr bool enabled = true;
    int value = 7;
};
struct MoveOnly : Good {
    MoveOnly() = default;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly(const MoveOnly&) = delete;
};

template<class Object>
struct Mapping {
    using T = int;
    static int read(const Object& object) noexcept { return object.value; }
    static Object duplicate(const Object& object) { return object; }
    static void combine(Object& left, const Object& right) { left.value += right.value; }
};
impl(Checked, (template<class Alias, bool Flag>), (Payload<Alias, Flag>))
    : Mapping<Payload<Alias, Flag>> {};
impl(Associated, (template<class Alias, bool Flag>), (Payload<Alias, Flag>))
    : Mapping<Payload<Alias, Flag>> {};
impl(Checked, Missing) : Mapping<Missing> {};
impl(Checked, MissingToken) : Mapping<MissingToken> {};
impl(Associated, Missing) : Mapping<Missing> {};
impl(DirectAlias, Missing) : Mapping<Missing> {};
impl(DirectAlias, Good) : Mapping<Good> {};
impl(Checked, MoveOnly) : Mapping<MoveOnly> {};
impl(Associated, MoveOnly) : Mapping<MoveOnly> {};

// The base declaration must not evaluate requirements on this incomplete type.
struct Native : implements(NativeRead), implements(Other) {
    using X = int;
    static constexpr bool enabled = true;
    int read() const noexcept override { return 11; }
    int other() const noexcept override { return 13; }
};
struct DerivedDisabled : Native { static constexpr bool enabled = false; };
struct DerivedWrongAlias : Native { using X = double; };
struct DerivedGood : Native {};
struct NativeMoveOnly : Native {
    NativeMoveOnly() = default;
    NativeMoveOnly(NativeMoveOnly&&) = default;
    NativeMoveOnly(const NativeMoveOnly&) = delete;
};
struct AssociatedNative : implements(Associated, int) {
    using X = int;
    static constexpr bool enabled = true;
    int read() const noexcept override { return 17; }
};
struct AssociatedDisabled : AssociatedNative { static constexpr bool enabled = false; };
struct AssociatedWrongAlias : AssociatedNative { using X = float; };

// Identical probes ensure rejection occurs at every public entry point.
#define REQUIREMENT_PROBES(Name) \
    template<class Object> concept Borrow##Name = requires(Object& object) { borrow(Name, object); }; \
    template<class Object> concept Adapt##Name = requires(Object&& object) { adapt(Name, std::forward<Object>(object)); }; \
    template<class Object> concept Box##Name = requires(Object&& object) { box(Name, std::forward<Object>(object)); }; \
    template<class Object> concept Call##Name = requires(const Object& object) { trait_call(Name, read, object); }; \
    template<class Object> constexpr bool Rejected##Name = !Name<Object> && \
        !Borrow##Name<Object> && !Adapt##Name<Object> && !Box##Name<Object> && !Call##Name<Object>
REQUIREMENT_PROBES(Checked);
REQUIREMENT_PROBES(Associated);
REQUIREMENT_PROBES(NativeRead);
REQUIREMENT_PROBES(DirectAlias);
#undef REQUIREMENT_PROBES

template<class Object>
concept HasAssociatedMapping = requires { typename trait_mapping_t(Associated, Object); };
static_assert(Checked<Good> && Associated<Good>);
static_assert(DirectAlias<Good> && RejectedDirectAlias<Missing>);
static_assert(!Checked<const Good> && Associated<const Good>);
static_assert(!BorrowChecked<const Good> && BorrowAssociated<const Good>);
static_assert(RejectedChecked<Disabled> && RejectedAssociated<Disabled>);
static_assert(RejectedChecked<WrongAlias> && RejectedAssociated<WrongAlias>);
static_assert(RejectedChecked<Missing> && RejectedAssociated<Missing>);
static_assert(RejectedChecked<MissingToken>);
static_assert(RejectedChecked<MoveOnly> && RejectedAssociated<MoveOnly>);
static_assert(RejectedChecked<int> && RejectedAssociated<int>);
static_assert(HasAssociatedMapping<Good> && HasAssociatedMapping<const Good>);
static_assert(!HasAssociatedMapping<Disabled> && !HasAssociatedMapping<WrongAlias>);
static_assert(!HasAssociatedMapping<Missing> && !HasAssociatedMapping<MoveOnly>);
static_assert(NativeRead<Native> && NativeRead<DerivedGood>);
static_assert(RejectedNativeRead<DerivedDisabled> && RejectedNativeRead<DerivedWrongAlias>);
static_assert(RejectedNativeRead<NativeMoveOnly>);
static_assert(Associated<AssociatedNative>);
static_assert(RejectedAssociated<AssociatedDisabled> && RejectedAssociated<AssociatedWrongAlias>);
static_assert(!HasAssociatedMapping<AssociatedDisabled>);

using Owned = decltype(adapt(Checked, std::declval<Good>()));
using Borrowed = decltype(borrow(Checked, std::declval<Good&>()));
using AssociatedOwned = decltype(adapt(Associated, std::declval<Good>()));
using AssociatedBorrowed = decltype(borrow(Associated, std::declval<const Good&>()));
static_assert(!Enabled<Owned> && !Enabled<Borrowed>);
static_assert(Checked<Owned> && Checked<Borrowed>);
static_assert(CallChecked<Owned> && CallChecked<Borrowed>);
// Only exact generated wrappers inherit the erased contract exemption.
struct DerivedOwned : Owned { using Owned::Owned; };
struct DerivedAssociatedOwned : AssociatedOwned { using AssociatedOwned::AssociatedOwned; };
static_assert(!Enabled<DerivedOwned> && RejectedChecked<DerivedOwned>);
static_assert(!Enabled<DerivedAssociatedOwned> && RejectedAssociated<DerivedAssociatedOwned>);
static_assert(!HasAssociatedMapping<DerivedAssociatedOwned>);
static_assert(Associated<AssociatedOwned> && Associated<AssociatedBorrowed>);
static_assert(CallAssociated<AssociatedOwned> && CallAssociated<AssociatedBorrowed>);
static_assert(!Enabled<dyn(Checked)> && Checked<dyn(Checked)> && CallChecked<dyn(Checked)>);
static_assert(!Enabled<dyn(Associated, int)> && Associated<dyn(Associated, int)>);
static_assert(CallAssociated<dyn(Associated, int)>);
static_assert(!Enabled<dyn(NativeRead)> && NativeRead<dyn(NativeRead)>);
static_assert(CallNativeRead<dyn(NativeRead)>);

template<class Object>
concept BorrowBundle = requires(Object& object) { borrow(Bundle, object); };
template<class Object>
concept AdaptBundle = requires(Object&& object) { adapt(Bundle, std::forward<Object>(object)); };
template<class Object>
concept BoxBundle = requires(Object&& object) { box(Bundle, std::forward<Object>(object)); };
static_assert(Bundle<Native> && Bundle<DerivedGood>);
static_assert(!Bundle<DerivedDisabled> && !Bundle<DerivedWrongAlias>);
static_assert(!BorrowBundle<DerivedDisabled> && !AdaptBundle<DerivedDisabled> && !BoxBundle<DerivedDisabled>);
static_assert(!BorrowBundle<DerivedWrongAlias> && !AdaptBundle<DerivedWrongAlias> && !BoxBundle<DerivedWrongAlias>);
static_assert(!BorrowBundle<NativeMoveOnly> && !AdaptBundle<NativeMoveOnly> && !BoxBundle<NativeMoveOnly>);

// Raw mappings still do not opt into runtime composition.
impl(NativeRead, Good) : Mapping<Good> {};
impl(Other, Good) {
    static int other(const Good&) noexcept { return 19; }
};
static_assert(NativeRead<Good> && Other<Good> && Bundle<Good>);
static_assert(!BorrowBundle<Good> && !AdaptBundle<Good> && !BoxBundle<Good>);

int main() {
    Good value;
    const Good constant{9};
    auto borrowed = borrow(Checked, value);
    auto owned = adapt(Checked, Good{5});
    auto boxed = box(Checked, Good{6});
    dyn(Checked)& erased = borrowed;
    if (trait_call(Checked, read, value) != 7 ||
        trait_call(Checked, read, borrowed) != 7 ||
        trait_call(Checked, read, owned) != 5 ||
        trait_call(Checked, read, *boxed) != 6 ||
        trait_call(Checked, read, erased) != 7 || borrowed.doubled() != 14) return 1;
    auto clone = borrowed.duplicate();
    borrowed.combine(*clone);
    if (value.value != 14 || clone->read() != 7 || boxed->doubled() != 12) return 2;

    auto associatedOwned = adapt(Associated, Good{21});
    auto associatedBorrowed = borrow(Associated, constant);
    auto associatedBoxed = box(Associated, Good{23});
    const dyn(Associated, int)& associatedErased = associatedBorrowed;
    AssociatedNative associatedNative;
    if (trait_call(Associated, read, constant) != 9 ||
        trait_call(Associated, read, associatedOwned) != 21 ||
        trait_call(Associated, read, associatedBorrowed) != 9 ||
        trait_call(Associated, read, *associatedBoxed) != 23 ||
        trait_call(Associated, read, associatedErased) != 9 ||
        trait_call(Associated, read, associatedNative) != 17) return 3;

    Native native;
    const Native constNative;
    auto view = borrow(Bundle, native);
    auto constView = borrow(Bundle, constNative);
    auto bundleOwned = adapt(Bundle, DerivedGood{});
    auto bundleBoxed = box(Bundle, Native{});
    const dyn(NativeRead)& nativeErased = native;
    if (trait_call(NativeRead, read, nativeErased) != 11 ||
        view.as<dyn(NativeRead)>().read() != 11 ||
        constView.as<dyn(Other)>().other() != 13 ||
        bundleOwned.as<dyn(NativeRead)>().read() != 11 ||
        bundleBoxed->as<dyn(Other)>().other() != 13) return 4;
    return 0;
}
