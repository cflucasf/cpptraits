#include "traits.hpp"

#include <concepts>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// Compile this branch with -fno-rtti to check that ordinary traits remain usable.
#ifdef CPPTRAITS_TEST_ORDINARY_NO_RTTI
trait(OrdinaryWithoutRtti, method(int, read, const));
struct OrdinaryPayload { int value; };
impl(OrdinaryWithoutRtti, OrdinaryPayload) {
    static int read(const OrdinaryPayload& value) { return value.value; }
};
int main() {
    OrdinaryPayload value{17};
    auto borrowed = borrow(OrdinaryWithoutRtti, value);
    auto owned = adapt(OrdinaryWithoutRtti, value);
    auto boxed = box(OrdinaryWithoutRtti, value);
    return borrowed.read() == 17 && owned.read() == 17 && boxed->read() == 17 ? 0 : 1;
}
#else

namespace concrete_tests {
trait(Average,
    concrete_method(combine, merge),
    method(int, read, const),
    default_method(int, doubled, (), const, { return read() * 2; })
);

// Concrete descriptors must work in different positions, not just as the first entry.
trait(TwoOperations,
    default_method(int, doubled, (), const, { return read() * 2; }),
    concrete_method(combine, merge),
    method(int, read, const),
    concrete_method(subtract, merge),
    default_method(bool, isZero, (), const, { return read() == 0; }),
    default_method(void, combineTwice,
        (param(const dyn(concrete_tests::TwoOperations)&, other)), , {
            combine(other);
            combine(other);
        })
);
trait(ConcreteOnly, concrete_method(combine, merge));

struct Motor { int value; };
struct Counter { int value; };
struct DerivedMotor : Motor {};
struct MappingError { int token; };
inline int mappingCalls = 0;

struct WrongReturn {};
struct MutableOperand {};
struct ConstReceiver {};
struct ValueOperand {};
struct WrongOperand {};
struct Nonstatic {};
struct Missing {};
struct NoMapping {};
struct Private {};
struct IntOnly {};
}

// Qualified traits and payloads must resolve from their enclosing namespace.
impl(concrete_tests::Average, concrete_tests::Motor) {
    using T = concrete_tests::Motor;
    static int read(const T& value) { return value.value; }
    static void combine(T& left, const T& right) {
        ++concrete_tests::mappingCalls;
        if (right.value < 0) {
            throw concrete_tests::MappingError{right.value};
        }
        left.value = (left.value + right.value) / 2;
    }
    // An overload set requires selection by the exact static function pointer type.
    static void combine(T&, int) { throw std::logic_error("wrong int overload"); }
    static void combine(T&, T&) { throw std::logic_error("wrong mutable overload"); }
};
impl(concrete_tests::Average, concrete_tests::Counter) {
    using T = concrete_tests::Counter;
    static int read(const T& value) { return value.value; }
    static void combine(T& left, const T& right) noexcept {
        ++concrete_tests::mappingCalls;
        left.value += right.value;
    }
};
impl(concrete_tests::Average, concrete_tests::DerivedMotor) {
    using T = concrete_tests::DerivedMotor;
    static int read(const T& value) { return value.value; }
    static void combine(T& left, const T& right) {
        ++concrete_tests::mappingCalls;
        left.value = (left.value + right.value) / 2;
    }
};
impl(concrete_tests::TwoOperations, concrete_tests::Motor) {
    using T = concrete_tests::Motor;
    static int read(const T& value) { return value.value; }
    static void combine(T& left, const T& right) noexcept {
        left.value = (left.value + right.value) / 2;
    }
    static void subtract(T& left, const T& right) { left.value -= right.value; }
    static void subtract(T&, int) { throw std::logic_error("wrong subtract overload"); }
};

impl(concrete_tests::ConcreteOnly, concrete_tests::WrongReturn) {
    static int combine(concrete_tests::WrongReturn&, const concrete_tests::WrongReturn&) {
        return 0;
    }
};
impl(concrete_tests::ConcreteOnly, concrete_tests::MutableOperand) {
    static void combine(concrete_tests::MutableOperand&, concrete_tests::MutableOperand&) {}
};
impl(concrete_tests::ConcreteOnly, concrete_tests::ConstReceiver) {
    static void combine(const concrete_tests::ConstReceiver&,
                        const concrete_tests::ConstReceiver&) {}
};
impl(concrete_tests::ConcreteOnly, concrete_tests::ValueOperand) {
    static void combine(concrete_tests::ValueOperand&, concrete_tests::ValueOperand) {}
};
impl(concrete_tests::ConcreteOnly, concrete_tests::WrongOperand) {
    static void combine(concrete_tests::WrongOperand&, const concrete_tests::Motor&) {}
};
impl(concrete_tests::ConcreteOnly, concrete_tests::Nonstatic) {
    void combine(concrete_tests::Nonstatic&, const concrete_tests::Nonstatic&) {}
};
impl(concrete_tests::ConcreteOnly, concrete_tests::Missing) {};
impl(concrete_tests::ConcreteOnly, concrete_tests::Private) {
private:
    static void combine(concrete_tests::Private&, const concrete_tests::Private&) {}
};
impl(concrete_tests::ConcreteOnly, concrete_tests::IntOnly) {
    static void combine(concrete_tests::IntOnly&, int) {}
};

// Opt-in compile-fail fixture: concrete traits cannot be implemented natively.
#ifdef CPPTRAITS_TEST_NATIVE_CONCRETE
struct NativeConcrete : implements(concrete_tests::Average) {
    int read() const override { return 0; }
    void combine(const dyn(concrete_tests::Average)&) override {}
};
#endif

namespace concrete_tests {
template<typename T>
concept CanBorrow = requires(T&& value) {
    borrow(concrete_tests::Average, std::forward<T>(value));
};
template<typename T>
concept CanAdapt = requires(T&& value) {
    adapt(concrete_tests::Average, std::forward<T>(value));
};
template<typename T>
concept CanBox = requires(T&& value) {
    box(concrete_tests::Average, std::forward<T>(value));
};
template<typename T>
concept CanBorrowOnly = requires(T&& value) {
    borrow(concrete_tests::ConcreteOnly, std::forward<T>(value));
};
template<typename T>
concept CanBorrowTwo = requires(T&& value) {
    borrow(concrete_tests::TwoOperations, std::forward<T>(value));
};
template<typename T>
concept CanAdaptOnly = requires(T&& value) {
    adapt(concrete_tests::ConcreteOnly, std::forward<T>(value));
};
template<typename T>
concept CanBoxOnly = requires(T&& value) {
    box(concrete_tests::ConcreteOnly, std::forward<T>(value));
};
template<typename T>
constexpr bool rejected = !ConcreteOnly<T> && !ConcreteOnly<const T> &&
                          !CanBorrowOnly<T&> && !CanBorrowOnly<const T&> &&
                          !CanAdaptOnly<T> && !CanBoxOnly<T>;
static_assert(rejected<WrongReturn> && rejected<MutableOperand> && rejected<ConstReceiver>);
static_assert(rejected<ValueOperand> && rejected<WrongOperand> && rejected<Nonstatic>);
static_assert(rejected<Missing> && rejected<NoMapping> && rejected<Private> && rejected<IntOnly>);
static_assert(CanBorrow<Motor&> && CanAdapt<Motor> && CanBox<Motor>);
static_assert(CanBorrow<Counter&> && CanAdapt<Counter> && CanBox<Counter>);
static_assert(CanBorrow<DerivedMotor&> && CanAdapt<DerivedMotor> && CanBox<DerivedMotor>);
static_assert(!CanBorrow<const Motor&> && !CanBorrow<const Counter&>);
static_assert(!CanBorrow<Motor> && !CanBorrow<const Motor>);
static_assert(CanBorrowTwo<Motor&> && !CanBorrowTwo<const Motor&> && !CanBorrowTwo<Motor>);

using Interface = dyn(concrete_tests::Average);
using Owned = decltype(adapt(concrete_tests::Average, std::declval<Motor>()));
using Borrowed = decltype(borrow(concrete_tests::Average, std::declval<Motor&>()));
static_assert(std::same_as<decltype(&Interface::combine), void (Interface::*)(const Interface&)>);
static_assert(!noexcept(std::declval<Interface&>().combine(std::declval<const Interface&>())));
static_assert(std::same_as<decltype(box(concrete_tests::Average, Motor{})),
                           std::unique_ptr<Interface>>);
static_assert(Average<Owned> && Average<Borrowed> && Average<Interface>);
static_assert(Average<Motor> && Average<Counter> && Average<DerivedMotor>);
static_assert(TwoOperations<Motor>);
static_assert(!Average<const Motor> && !Average<const Counter>);
static_assert(!TwoOperations<const Motor>);
static_assert(!Average<NoMapping>);
static_assert(!std::convertible_to<Motor&, Interface&>);
static_assert(!std::convertible_to<Counter&, Interface&>);

template<typename T>
concept CanCombineErased = requires(T& left, const Interface& right) {
    left.combine(right);
};
static_assert(CanCombineErased<Interface> && CanCombineErased<Owned> && CanCombineErased<Borrowed>);
static_assert(!CanCombineErased<Motor> && !CanCombineErased<Counter>);
static_assert(std::is_copy_constructible_v<Owned> && std::is_move_constructible_v<Owned>);
static_assert(std::is_copy_constructible_v<Borrowed> && std::is_move_constructible_v<Borrowed>);

template<typename T>
concept ConstCanCombine = requires(const T& left, const Interface& right) {
    left.combine(right);
};
static_assert(!ConstCanCombine<Interface> && !ConstCanCombine<Owned> && !ConstCanCombine<Borrowed>);
static_assert(!std::is_polymorphic_v<Motor> && !std::is_polymorphic_v<Counter>);
static_assert(!std::is_polymorphic_v<DerivedMotor>);
static_assert(std::is_standard_layout_v<Motor> && std::is_trivially_copyable_v<Motor>);
static_assert(std::is_standard_layout_v<Counter> && std::is_trivially_copyable_v<Counter>);
static_assert(sizeof(Motor) == sizeof(int) && sizeof(Counter) == sizeof(int));
static_assert(sizeof(DerivedMotor) == sizeof(Motor));
static_assert(alignof(Motor) == alignof(int) && alignof(Counter) == alignof(int));

int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

void crossAdapters() {
    // Every ownership pairing is exercised in both receiver/operand roles.
    for (int receiver = 0; receiver != 3; ++receiver) {
        for (int operand = 0; operand != 3; ++operand) {
            Motor left{1400}, right{700};
            auto leftBorrow = borrow(concrete_tests::Average, left);
            auto leftOwn = adapt(concrete_tests::Average, left);
            auto leftBox = box(concrete_tests::Average, left);
            auto rightBorrow = borrow(concrete_tests::Average, right);
            auto rightOwn = adapt(concrete_tests::Average, right);
            auto rightBox = box(concrete_tests::Average, right);
            Interface* receivers[] = {&leftBorrow, &leftOwn, leftBox.get()};
            const Interface* operands[] = {&rightBorrow, &rightOwn, rightBox.get()};
            const int calls = mappingCalls;
            receivers[receiver]->combine(*operands[operand]);
            check(receivers[receiver]->read() == 1050, "all adapter pairings must average to 1050");
            check(receivers[receiver]->doubled() == 2100, "default must dispatch after concrete mutation");
            check(mappingCalls == calls + 1, "exact matching overload must run once");
            check(right.value == 700 && rightBorrow.read() == 700 &&
                  rightOwn.read() == 700 && rightBox->read() == 700,
                  "const erased operand and its source must remain unchanged");
            check(left.value == (receiver == 0 ? 1050 : 1400),
                  "only borrowing may mutate the receiver source");
        }
    }
}

void aliasAndValueSemantics() {
    Motor source{1400};
    auto owned = adapt(concrete_tests::Average, source);
    auto copied = owned;
    auto moved = std::move(copied);
    auto low = adapt(concrete_tests::Average, Motor{700});
    moved.combine(low);
    check(moved.read() == 1050 && owned.read() == 1400 && source.value == 1400,
          "copied/moved owning adapters must keep independent payloads");
    const Interface& constantOwned = owned;
    owned.combine(constantOwned);
    check(owned.read() == 1400, "owned self-alias must be supported");

    auto borrowed = borrow(concrete_tests::Average, source);
    auto borrowCopy = borrowed;
    auto borrowMove = std::move(borrowCopy);
    borrowMove.combine(low);
    check(source.value == 1050 && borrowed.read() == 1050,
          "copied/moved borrowed adapters must preserve their reference");
    const auto& constWrapper = borrowMove;
    borrowed.combine(constWrapper);
    borrowed.combine(borrowed);
    check(source.value == 1050, "distinct borrowed aliases and direct self must be supported");

    auto boxed = box(concrete_tests::Average, Motor{700});
    auto movedBox = std::move(boxed);
    check(!boxed, "moving a box must transfer ownership");
    movedBox->combine(*movedBox);
    check(movedBox->read() == 700, "boxed self-alias must be supported");
    const auto& constantWrapper = low;
    owned.combine(constantWrapper);
    check(owned.read() == 1050 && constantWrapper.read() == 700,
          "const reference to a mutable-created owning wrapper is a valid operand");
}

void rejectMismatch(Interface& left, const Interface& right) {
    const int before = left.read();
    const int rightBefore = right.read();
    const int calls = mappingCalls;
    bool rejectedType = false;
    try {
        left.combine(right);
    } catch (const std::invalid_argument&) {
        rejectedType = true;
    } catch (...) {
        check(false, "mismatch must throw std::invalid_argument, not another exception");
    }
    check(rejectedType, "different raw mapped types must be rejected");
    check(left.read() == before && right.read() == rightBefore,
          "mismatch must leave both receiver and operand unchanged");
    check(mappingCalls == calls, "mismatch must be rejected before entering the mapping");
}

void mismatchesAndHeterogeneousVector() {
    Motor motor{1400};
    Counter counter{700};
    auto motorBorrow = borrow(concrete_tests::Average, motor);
    auto motorOwn = adapt(concrete_tests::Average, motor);
    auto motorBox = box(concrete_tests::Average, motor);
    auto counterBorrow = borrow(concrete_tests::Average, counter);
    auto counterOwn = adapt(concrete_tests::Average, counter);
    auto counterBox = box(concrete_tests::Average, counter);
    Interface* motors[] = {&motorBorrow, &motorOwn, motorBox.get()};
    Interface* counters[] = {&counterBorrow, &counterOwn, counterBox.get()};
    for (auto* left : motors) {
        for (auto* right : counters) {
            rejectMismatch(*left, *right);
            rejectMismatch(*right, *left);
        }
    }

    std::vector<std::unique_ptr<Interface>> values;
    values.push_back(box(concrete_tests::Average, Motor{1400}));
    values.push_back(box(concrete_tests::Average, Counter{10}));
    values.push_back(box(concrete_tests::Average, DerivedMotor{{1400}}));
    auto motorOperand = adapt(concrete_tests::Average, Motor{700});
    auto counterOperand = adapt(concrete_tests::Average, Counter{20});
    auto derivedOperand = adapt(concrete_tests::Average, DerivedMotor{{700}});
    values[0]->combine(motorOperand);
    values[1]->combine(counterOperand);
    values[2]->combine(derivedOperand);
    check(values[0]->read() == 1050 && values[1]->read() == 30 && values[2]->read() == 1050,
          "heterogeneous container must dispatch each exact raw mapping, including noexcept");
    for (std::size_t left = 0; left != values.size(); ++left) {
        for (std::size_t right = 0; right != values.size(); ++right) {
            if (left != right) {
                // In particular, base/derived raw types are NOT interchangeable.
                rejectMismatch(*values[left], *values[right]);
            }
        }
    }
}

void userExceptions() {
    Motor source{1400};
    auto borrowed = borrow(concrete_tests::Average, source);
    auto owned = adapt(concrete_tests::Average, source);
    auto boxed = box(concrete_tests::Average, source);
    auto operand = adapt(concrete_tests::Average, Motor{-7});
    Interface* receivers[] = {&borrowed, &owned, boxed.get()};
    for (auto* receiver : receivers) {
        const int calls = mappingCalls;
        bool propagated = false;
        try {
            receiver->combine(operand);
        } catch (const MappingError& error) {
            propagated = error.token == -7;
        } catch (...) {
            check(false, "user exceptions must not be translated");
        }
        check(propagated && mappingCalls == calls + 1,
              "same-type mapping exception must propagate with its payload");
        check(receiver->read() == 1400, "throw-before-write mapping must preserve receiver");
    }
}

void twoConcreteMethods() {
    Motor source{1400};
    auto borrowed = borrow(concrete_tests::TwoOperations, source);
    auto owned = adapt(concrete_tests::TwoOperations, source);
    auto boxed = box(concrete_tests::TwoOperations, source);
    auto operand = adapt(concrete_tests::TwoOperations, Motor{700});
    const dyn(concrete_tests::TwoOperations)& constant = operand;
    dyn(concrete_tests::TwoOperations)* receivers[] = {&borrowed, &owned, boxed.get()};
    for (auto* receiver : receivers) {
        receiver->combine(constant);
        check(receiver->read() == 1050 && receiver->doubled() == 2100,
              "first concrete marker must coexist with ordinary methods and defaults");
        receiver->subtract(constant);
        check(receiver->read() == 350 && !receiver->isZero(),
              "second concrete marker must dispatch independently");
        receiver->subtract(*receiver);
        check(receiver->isZero() && receiver->doubled() == 0,
              "second concrete marker must support self-alias and shared defaults");
    }
    check(source.value == 0 && operand.read() == 700,
          "mixed trait must retain borrowing and const operand semantics");
    borrowed.combineTwice(constant);
    check(source.value == 525 && operand.read() == 700,
          "shared default bodies must dispatch through checked concrete methods");
}
} // namespace concrete_tests

int main() {
    using namespace concrete_tests;
    crossAdapters();
    aliasAndValueSemantics();
    mismatchesAndHeterogeneousVector();
    userExceptions();
    twoConcreteMethods();
    return failures == 0 ? 0 : 1;
}
#endif
