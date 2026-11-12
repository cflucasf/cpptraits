#include "traits.hpp"

#include <concepts>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// Fail the next box allocation without relying on memory exhaustion. The clone
// used by this fixture returns a trivial payload and performs no allocations.
namespace allocation_test {
inline bool failNext = false;
// Keep GCC from inlining free into replacement delete and diagnosing callers
// that correctly pair new with delete as mismatched allocations.
inline auto volatile freeMemory = &std::free;
}
void* operator new(std::size_t size) {
    if (std::exchange(allocation_test::failNext, false)) {
        throw std::bad_alloc{};
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { allocation_test::freeMemory(memory); }
void operator delete(void* memory, std::size_t) noexcept { allocation_test::freeMemory(memory); }

namespace clone_tests {
trait(Snapshot,
    method(int, read, const),
    concrete_method(duplicate, clone),
    default_method(int, doubled, (), const, { return read() * 2; })
);
trait(CloneOnly, concrete_method(reproduce, clone));
trait(MutableSnapshot,
    concrete_method(fork, clone),
    method(void, write, (param(int, value)), )
);

// Const borrows must also satisfy the operations of the mutable owning clone.
trait(MarkerClone,
    default_method(int, marker, (), , { return 1; }),
    concrete_method(copyMarker, clone)
);
trait(RequiredReadClone,
    concrete_method(copyReader, clone),
    method(int, read, )
);
struct ConstMarker {};
struct DeletedMutableRead {};

struct Number { int value; };
struct MappingError { int token; };
inline int cloneCalls = 0;

struct WrongReturn {};
struct RefReturn {};
struct ConstReturn {};
struct WrongReceiver {};
struct ValueReceiver {};
struct Missing {};
struct NoMapping {};
struct Private {};
struct Nonstatic {};
struct Immovable {
    Immovable() = default;
    Immovable(const Immovable&) = delete;
    Immovable(Immovable&&) = delete;
};

// Moving is required for boxing, but copying is not: the mapping deep-copies.
struct MoveOnly {
    std::unique_ptr<int> value;
    explicit MoveOnly(int initial) : value(std::make_unique<int>(initial)) {}
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
    MoveOnly(const MoveOnly&) = delete;
};
}

impl(clone_tests::MarkerClone, clone_tests::ConstMarker) {
    using T = clone_tests::ConstMarker;
    static int marker(const T&) { return 2; }
    static T copyMarker(const T& value) { return value; }
};
impl(clone_tests::RequiredReadClone, clone_tests::DeletedMutableRead) {
    using T = clone_tests::DeletedMutableRead;
    static int read(const T&) { return 3; }
    static int read(T&) = delete;
    static T copyReader(const T& value) { return value; }
};

impl(clone_tests::Snapshot, clone_tests::Number) {
    using T = clone_tests::Number;
    static int read(const T& value) { return value.value; }
    static T duplicate(const T& value) {
        ++clone_tests::cloneCalls;
        if (value.value < 0) {
            throw clone_tests::MappingError{value.value};
        }
        return value;
    }
    // Calling the name directly on a mutable object would pick the wrong overload.
    static T duplicate(T&) { throw std::logic_error("mutable clone overload"); }
    static T duplicate(int) { throw std::logic_error("integer clone overload"); }
};
impl(clone_tests::Snapshot, clone_tests::MoveOnly) {
    using T = clone_tests::MoveOnly;
    static int read(const T& value) { return *value.value; }
    static T duplicate(const T& value) { return T{*value.value}; }
};
impl(clone_tests::MutableSnapshot, clone_tests::Number) {
    using T = clone_tests::Number;
    static T fork(const T& value) { return value; }
    static void write(T& value, int next) { value.value = next; }
};
impl(clone_tests::CloneOnly, clone_tests::Number) {
    static clone_tests::Number reproduce(const clone_tests::Number& value) noexcept {
        return value;
    }
};
impl(clone_tests::CloneOnly, clone_tests::WrongReturn) {
    static int reproduce(const clone_tests::WrongReturn&) { return 0; }
};
impl(clone_tests::CloneOnly, clone_tests::RefReturn) {
    static const clone_tests::RefReturn& reproduce(const clone_tests::RefReturn& value) {
        return value;
    }
};
impl(clone_tests::CloneOnly, clone_tests::ConstReturn) {
    static const clone_tests::ConstReturn reproduce(const clone_tests::ConstReturn&) {
        return {};
    }
};
impl(clone_tests::CloneOnly, clone_tests::WrongReceiver) {
    static clone_tests::WrongReceiver reproduce(clone_tests::WrongReceiver&) { return {}; }
};
impl(clone_tests::CloneOnly, clone_tests::ValueReceiver) {
    static clone_tests::ValueReceiver reproduce(clone_tests::ValueReceiver) { return {}; }
};
impl(clone_tests::CloneOnly, clone_tests::Missing) {};
impl(clone_tests::CloneOnly, clone_tests::Private) {
private:
    static clone_tests::Private reproduce(const clone_tests::Private&) { return {}; }
};
impl(clone_tests::CloneOnly, clone_tests::Nonstatic) {
    clone_tests::Nonstatic reproduce(const clone_tests::Nonstatic&) { return {}; }
};
impl(clone_tests::CloneOnly, clone_tests::Immovable) {
    static clone_tests::Immovable reproduce(const clone_tests::Immovable&) { return {}; }
};

namespace clone_tests {
using MarkerMapping = MarkerClone_cpptraits_detail::cpptraits_implementation<ConstMarker>;
using ReaderMapping = RequiredReadClone_cpptraits_detail::cpptraits_implementation<DeletedMutableRead>;
static_assert(std::same_as<decltype(&MarkerMapping::copyMarker), ConstMarker (*)(const ConstMarker&)>);
static_assert(std::same_as<decltype(&ReaderMapping::copyReader),
                           DeletedMutableRead (*)(const DeletedMutableRead&)>);
static_assert(std::move_constructible<ConstMarker> && std::move_constructible<DeletedMutableRead>);

// The const-only override must not disappear into the default on an owning clone.
static_assert(!MarkerClone<ConstMarker> && !MarkerClone<const ConstMarker>);
template<typename T>
constexpr bool markerFactoriesRejected =
    !requires(T&& value) { borrow(clone_tests::MarkerClone, std::forward<T>(value)); } &&
    !requires(T&& value) { adapt(clone_tests::MarkerClone, std::forward<T>(value)); } &&
    !requires(T&& value) { box(clone_tests::MarkerClone, std::forward<T>(value)); };
static_assert(markerFactoriesRejected<ConstMarker&> && markerFactoriesRejected<const ConstMarker&>);
static_assert(markerFactoriesRejected<ConstMarker> && markerFactoriesRejected<const ConstMarker>);

// Reject the const borrow by constraints, not by instantiating an invalid clone
// adapter body that tries to use the deleted mutable required operation.
static_assert(!RequiredReadClone<DeletedMutableRead> && !RequiredReadClone<const DeletedMutableRead>);
template<typename T>
constexpr bool readerFactoriesRejected =
    !requires(T&& value) { borrow(clone_tests::RequiredReadClone, std::forward<T>(value)); } &&
    !requires(T&& value) { adapt(clone_tests::RequiredReadClone, std::forward<T>(value)); } &&
    !requires(T&& value) { box(clone_tests::RequiredReadClone, std::forward<T>(value)); };
static_assert(readerFactoriesRejected<DeletedMutableRead&> &&
              readerFactoriesRejected<const DeletedMutableRead&>);
static_assert(readerFactoriesRejected<DeletedMutableRead> &&
              readerFactoriesRejected<const DeletedMutableRead>);

template<typename T>
concept CanBorrowOnly = requires(T&& value) {
    borrow(clone_tests::CloneOnly, std::forward<T>(value));
};
template<typename T>
concept CanAdaptOnly = requires(T&& value) {
    adapt(clone_tests::CloneOnly, std::forward<T>(value));
};
template<typename T>
concept CanBoxOnly = requires(T&& value) {
    box(clone_tests::CloneOnly, std::forward<T>(value));
};
template<typename T>
constexpr bool rejected = !CloneOnly<T> && !CloneOnly<const T> &&
    !CanBorrowOnly<T&> && !CanBorrowOnly<const T&> &&
    !CanAdaptOnly<T> && !CanBoxOnly<T>;
static_assert(rejected<WrongReturn> && rejected<RefReturn> && rejected<ConstReturn>);
static_assert(rejected<WrongReceiver> && rejected<ValueReceiver> && rejected<Missing>);
static_assert(rejected<NoMapping> && rejected<Private> && rejected<Nonstatic>);
static_assert(rejected<Immovable>);
static_assert(CloneOnly<Number> && CloneOnly<const Number>);
static_assert(CanBorrowOnly<Number&> && CanBorrowOnly<const Number&>);
static_assert(!CanBorrowOnly<Number> && !CanBorrowOnly<const Number>);
static_assert(CanAdaptOnly<Number> && CanBoxOnly<Number>);
static_assert(!std::is_polymorphic_v<Number> && sizeof(Number) == sizeof(int));

using SnapshotInterface = dyn(clone_tests::Snapshot);
using SnapshotOwned = decltype(adapt(clone_tests::Snapshot, std::declval<Number>()));
using SnapshotBorrowed = decltype(borrow(clone_tests::Snapshot, std::declval<Number&>()));
using SnapshotConstBorrowed = decltype(borrow(clone_tests::Snapshot,
                                             std::declval<const Number&>()));
static_assert(Snapshot<Number> && Snapshot<const Number> && Snapshot<SnapshotInterface>);
static_assert(Snapshot<SnapshotOwned> && Snapshot<SnapshotBorrowed> &&
              Snapshot<SnapshotConstBorrowed>);
static_assert(std::same_as<decltype(&SnapshotInterface::duplicate),
    std::unique_ptr<SnapshotInterface> (SnapshotInterface::*)() const>);
static_assert(!noexcept(std::declval<const SnapshotInterface&>().duplicate()));
static_assert(std::same_as<decltype(std::declval<const SnapshotOwned&>().duplicate()),
                           std::unique_ptr<SnapshotInterface>>);
static_assert(!std::convertible_to<Number&, SnapshotInterface&>);
static_assert(Snapshot<MoveOnly> && Snapshot<const MoveOnly>);
static_assert(std::move_constructible<MoveOnly> && !std::copy_constructible<MoveOnly>);
static_assert(MutableSnapshot<Number> && !MutableSnapshot<const Number>);
template<typename T>
concept CanBorrowMutable = requires(T& value) { borrow(clone_tests::MutableSnapshot, value); };
static_assert(CanBorrowMutable<Number> && !CanBorrowMutable<const Number>);

int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

void cloneOnlyAdapters() {
    Number source{17};
    const Number constant{23};
    auto borrowed = borrow(clone_tests::Snapshot, source);
    auto constBorrowed = borrow(clone_tests::Snapshot, constant);
    auto owned = adapt(clone_tests::Snapshot, source);
    auto boxed = box(clone_tests::Snapshot, source);
    const SnapshotInterface* sources[] = {&borrowed, &constBorrowed, &owned, boxed.get()};
    const int expected[] = {17, 23, 17, 17};
    std::vector<std::unique_ptr<SnapshotInterface>> copies;
    for (std::size_t i = 0; i != 4; ++i) {
        const int calls = cloneCalls;
        copies.push_back(sources[i]->duplicate());
#ifndef CPPTRAITS_TEST_CLONE_NO_RTTI
        check(dynamic_cast<SnapshotOwned*>(copies.back().get()) != nullptr,
              "even const borrowed clones must own the same unqualified raw T");
#endif
        check(copies.back() && copies.back().get() != sources[i],
              "const erased clone must return a new owning interface");
        check(copies.back()->read() == expected[i] &&
              copies.back()->doubled() == expected[i] * 2 && cloneCalls == calls + 1,
              "clone-only traits must retain fixed/default operations and exact overload selection");
    }
    source.value = 99;
    check(borrowed.read() == 99 && copies[0]->read() == 17 && copies[1]->read() == 23,
          "clones of mutable and const borrows must not alias their source");

    auto only = borrow(clone_tests::CloneOnly, constant);
    auto onlyCopy = only.reproduce();
    check(onlyCopy && onlyCopy->reproduce(), "a descriptor-only trait must clone repeatedly");
    auto mutableSource = borrow(clone_tests::MutableSnapshot, source);
    auto mutableCopy = std::as_const(mutableSource).fork();
    mutableCopy->write(5);
    check(source.value == 99, "a const clone call must return a mutable owning result");
}

void moveOnlyAndLifetime() {
    std::unique_ptr<SnapshotInterface> survivor;
    {
        MoveOnly source{31};
        const auto borrowed = borrow(clone_tests::Snapshot, std::as_const(source));
        survivor = borrowed.duplicate();
        *source.value = 41;
        check(survivor->read() == 31 && borrowed.read() == 41,
              "move-only mapping must explicitly deep-copy unique payloads");
        auto owned = adapt(clone_tests::Snapshot, MoveOnly{51});
        auto boxed = box(clone_tests::Snapshot, MoveOnly{61});
        auto ownedCopy = owned.duplicate();
        auto boxedCopy = boxed->duplicate();
        check(ownedCopy->read() == 51 && boxedCopy->read() == 61,
              "move-only owning and boxed adapters must clone without copying T");
    }
    check(survivor->read() == 31 && survivor->duplicate()->read() == 31,
          "clone must own its deep payload after source and borrowing adapter die");
}

void exceptions() {
    Number source{-7};
    auto borrowed = borrow(clone_tests::Snapshot, source);
    auto owned = adapt(clone_tests::Snapshot, source);
    auto boxed = box(clone_tests::Snapshot, source);
    const SnapshotInterface* sources[] = {&borrowed, &owned, boxed.get()};
    for (const auto* value : sources) {
        bool propagated = false;
        const int calls = cloneCalls;
        try {
            (void)value->duplicate();
        } catch (const MappingError& error) {
            propagated = error.token == -7;
        } catch (...) {
            check(false, "clone must not translate mapping exceptions");
        }
        check(propagated && value->read() == -7 && cloneCalls == calls + 1,
              "user clone exception must propagate unchanged and preserve source");
    }
    source.value = 7;
    bool allocationPropagated = false;
    // Let the result escape through an opaque call so new/delete elision cannot
    // remove the allocation whose failure this test needs to observe.
    using Observer = int (*)(const SnapshotInterface*);
    Observer volatile observe = +[](const SnapshotInterface* value) { return value->read(); };
    allocation_test::failNext = true;
    try {
        auto copy = borrowed.duplicate();
        check(observe(copy.get()) == 7, "successful clone must retain its payload");
    } catch (const std::bad_alloc&) {
        allocationPropagated = true;
    } catch (...) {
        allocation_test::failNext = false;
        check(false, "box allocation failure must not be translated");
    }
    allocation_test::failNext = false;
    check(allocationPropagated && source.value == 7 && borrowed.duplicate()->read() == 7,
          "box allocation exception must propagate and permit subsequent clones");
}
} // namespace clone_tests

// Compile with -DCPPTRAITS_TEST_CLONE_NO_RTTI -fno-rtti to exercise only traits
// whose concrete operations need no runtime type comparison.
#ifndef CPPTRAITS_TEST_CLONE_NO_RTTI
namespace clone_tests {
trait(Archive,
    concrete_method(reconstitute, clone),
    method(int, read, const),
    concrete_method(absorb, merge),
    default_method(int, doubled, (), const, { return read() * 2; })
);
struct Counter { int value; };
struct DerivedNumber : Number {};
}
impl(clone_tests::Archive, clone_tests::Number) {
    using T = clone_tests::Number;
    static int read(const T& value) { return value.value; }
    static T reconstitute(const T& value) { return value; }
    static T reconstitute(T&) { throw std::logic_error("wrong clone overload"); }
    static void absorb(T& left, const T& right) { left.value += right.value; }
};
impl(clone_tests::Archive, clone_tests::Counter) {
    using T = clone_tests::Counter;
    static int read(const T& value) { return value.value; }
    static T reconstitute(const T& value) noexcept { return value; }
    static void absorb(T& left, const T& right) noexcept { left.value += right.value; }
};
impl(clone_tests::Archive, clone_tests::DerivedNumber) {
    using T = clone_tests::DerivedNumber;
    static int read(const T& value) { return value.value; }
    static T reconstitute(const T& value) { return value; }
    static void absorb(T& left, const T& right) { left.value += right.value; }
};
namespace clone_tests {
using ArchiveInterface = dyn(clone_tests::Archive);
using ArchiveOwned = decltype(adapt(clone_tests::Archive, std::declval<Number>()));
static_assert(Archive<Number> && Archive<Counter> && Archive<DerivedNumber>);
static_assert(!Archive<const Number>);
static_assert(Archive<ArchiveOwned> && Archive<ArchiveInterface>);
static_assert(std::same_as<decltype(&ArchiveInterface::reconstitute),
    std::unique_ptr<ArchiveInterface> (ArchiveInterface::*)() const>);
static_assert(std::same_as<decltype(&ArchiveInterface::absorb),
    void (ArchiveInterface::*)(const ArchiveInterface&)>);

void mismatch(ArchiveInterface& left, const ArchiveInterface& right) {
    const int before = left.read(), otherBefore = right.read();
    bool rejectedType = false;
    try {
        left.absorb(right);
    } catch (const std::invalid_argument&) {
        rejectedType = true;
    } catch (...) {
        check(false, "mixed-trait mismatch must throw std::invalid_argument");
    }
    check(rejectedType && left.read() == before && right.read() == otherBefore,
          "clone must preserve exact concrete identity and mismatch must preserve both operands");
}

void mixedAdapters() {
    Number raw{10};
    auto borrowed = borrow(clone_tests::Archive, raw);
    auto owned = adapt(clone_tests::Archive, raw);
    auto boxed = box(clone_tests::Archive, raw);
    ArchiveInterface* sources[] = {&borrowed, &owned, boxed.get()};
    for (auto* source : sources) {
        const ArchiveInterface& constant = *source;
        auto copy = constant.reconstitute();
        check(copy && copy->read() == 10 && copy->doubled() == 20,
              "mixed trait clone must retain data and shared defaults");
        check(dynamic_cast<ArchiveOwned*>(copy.get()) != nullptr,
              "all clone sources must produce the cpptraits owning adapter of raw T");
        copy->absorb(*source);
        check(copy->read() == 20 && source->read() == 10,
              "cloned owning receiver must merge the original exact raw type independently");
        source->absorb(*copy);
        check(source->read() == 30 && copy->read() == 20,
              "borrowed, owned and boxed sources must accept their clones as operands");
        auto second = copy->reconstitute();
        copy.reset();
        check(second->read() == 20, "clone of a clone must survive destruction of its source");
    }
    check(raw.value == 30, "only borrowed mixed receiver must update its raw source");
}

void heterogeneousDeepClone() {
    std::vector<std::unique_ptr<ArchiveInterface>> originals;
    originals.push_back(box(clone_tests::Archive, Number{3}));
    originals.push_back(box(clone_tests::Archive, Counter{5}));
    originals.push_back(box(clone_tests::Archive, DerivedNumber{{7}}));
    std::vector<std::unique_ptr<ArchiveInterface>> copies;
    for (const auto& original : originals) {
        copies.push_back(std::as_const(*original).reconstitute());
        check(copies.back()->read() == original->read() && copies.back().get() != original.get(),
              "deep-cloned heterogeneous interface vector must retain every value independently");
    }
    for (std::size_t i = 0; i != copies.size(); ++i) {
        for (std::size_t j = 0; j != copies.size(); ++j) {
            if (i != j) {
                mismatch(*copies[i], *originals[j]);
                mismatch(*originals[i], *copies[j]);
                mismatch(*copies[i], *copies[j]);
            }
        }
        copies[i]->absorb(*originals[i]);
        check(copies[i]->read() == originals[i]->read() * 2,
              "each clone must still accept exactly its own concrete mapping");
    }
    originals.clear();
    check(copies[0]->read() == 6 && copies[1]->read() == 10 && copies[2]->read() == 14,
          "heterogeneous clones must outlive the entire original vector");
}
} // namespace clone_tests
#endif

int main() {
    using namespace clone_tests;
    cloneOnlyAdapters();
    moveOnlyAndLifetime();
    exceptions();
#ifndef CPPTRAITS_TEST_CLONE_NO_RTTI
    mixedAdapters();
    heterogeneousDeepClone();
#endif
    return failures == 0 ? 0 : 1;
}
