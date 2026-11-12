#include <cmath>
#include <concepts>
#include <memory>
#include <numbers>
#include <type_traits>
#include <utility>
#include <vector>

#include "traits.hpp"

trait(Rotatable, method(float, getRpm, const));

template<typename T>
using OwnedRotatable = decltype(adapt(Rotatable, std::declval<T>()));
template<typename T>
using BorrowedRotatable = decltype(borrow(Rotatable, std::declval<T&>()));
template<typename T>
concept CanBorrowRotatable = requires(T& value) {
    borrow(Rotatable, value);
};

struct PlainRotatable {
    float getRpm() const { return 100; }
};

struct MissingMethod {};
struct WrongReturn {
    double getRpm() const { return 100; }
};
struct WrongConstness {
    float getRpm() { return 100; }
};
struct StaticMethod {
    static float getRpm() { return 100; }
};

static_assert(!Rotatable<PlainRotatable>);
static_assert(!std::derived_from<PlainRotatable, dyn(Rotatable)>);
static_assert(!Rotatable<MissingMethod>);
static_assert(!Rotatable<WrongReturn>);
static_assert(!Rotatable<WrongConstness>);
static_assert(!Rotatable<StaticMethod>);
static_assert(std::has_virtual_destructor_v<dyn(Rotatable)>);

struct Motor : implements(Rotatable) {
    float getRpm() const override { return 1400; }
};

struct Spindle : implements(Rotatable) {
    float getRpm() const override { return 200; }
};

struct Tracked : implements(Rotatable) {
    bool& destroyed;
    explicit Tracked(bool& destroyed) : destroyed(destroyed) {}
    ~Tracked() override { destroyed = true; }
    float getRpm() const override { return 0; }
};

trait(Adjustable,
    method(float, getRpm, const),
    method(void, setRpm, (param(float, rpm)), )
);

template<typename T>
concept CanBorrowAdjustable = requires(T& value) {
    borrow(Adjustable, value);
};

struct AdjustableMotor : implements(Adjustable) {
    float rpm = 0;
    float getRpm() const override { return rpm; }
    void setRpm(float value) override { rpm = value; }
};

struct PrivateRotatable : private dyn(Rotatable) {
    float getRpm() const override { return 100; }
};

struct LeftRotatable : implements(Rotatable) {};
struct RightRotatable : implements(Rotatable) {};
struct AmbiguousRotatable : LeftRotatable, RightRotatable {
    float getRpm() const override { return 100; }
};

static_assert(Rotatable<Motor>);
static_assert(Rotatable<Spindle>);
static_assert(Rotatable<dyn(Rotatable)>);
static_assert(!Rotatable<PrivateRotatable>);
static_assert(!Rotatable<AmbiguousRotatable>);
static_assert(std::convertible_to<Motor&, dyn(Rotatable)&>);
static_assert(std::convertible_to<const Spindle&, const dyn(Rotatable)&>);
static_assert(Adjustable<AdjustableMotor>);
static_assert(!Adjustable<Motor>);

// This foreign type has no trait inheritance or getRpm member.
struct ExternalRotor {
    double radiansPerSecond;
};

impl(Rotatable, ExternalRotor) {
    static float getRpm(const ExternalRotor& rotor) {
        return static_cast<float>(rotor.radiansPerSecond * 60.0 /
                                  (2.0 * std::numbers::pi));
    }
};

// A built-in type can also acquire a trait without being modified.
impl(Rotatable, int) {
    static float getRpm(const int& rpm) { return static_cast<float>(rpm); }
};

struct WrongImplReturn {};
impl(Rotatable, WrongImplReturn) {
    static double getRpm(const WrongImplReturn&) { return 0; }
};

struct WrongImplConstness {};
impl(Rotatable, WrongImplConstness) {
    static float getRpm(WrongImplConstness&) { return 0; }
};

struct WrongImplByValue {};
impl(Rotatable, WrongImplByValue) {
    static float getRpm(WrongImplByValue) { return 0; }
};

struct NonStaticImpl {};
impl(Rotatable, NonStaticImpl) {
    float getRpm(const NonStaticImpl&) { return 0; }
};

static_assert(CanBorrowRotatable<ExternalRotor>);
static_assert(CanBorrowRotatable<int>);
static_assert(Rotatable<ExternalRotor> && Rotatable<const ExternalRotor>);
static_assert(Rotatable<int> && Rotatable<const int>);
static_assert(!Rotatable<volatile ExternalRotor> && !Rotatable<volatile int>);
static_assert(!std::convertible_to<ExternalRotor&, dyn(Rotatable)&>);
static_assert(!std::convertible_to<int&, dyn(Rotatable)&>);
static_assert(!Rotatable<WrongImplReturn>);
static_assert(!Rotatable<WrongImplConstness>);
static_assert(!Rotatable<WrongImplByValue>);
static_assert(!Rotatable<NonStaticImpl>);

template<typename T>
concept HasRpmMember = requires(const T& object) { object.getRpm(); };
static_assert(!HasRpmMember<ExternalRotor> && !HasRpmMember<int>);
static_assert(!CanBorrowRotatable<MissingMethod>);
static_assert(!CanBorrowRotatable<PlainRotatable>);
static_assert(!CanBorrowRotatable<WrongImplReturn>);
static_assert(!CanBorrowRotatable<WrongImplConstness>);
static_assert(!CanBorrowRotatable<WrongImplByValue>);
static_assert(!CanBorrowRotatable<NonStaticImpl>);
static_assert(Rotatable<OwnedRotatable<ExternalRotor>>);
static_assert(Rotatable<BorrowedRotatable<ExternalRotor>>);
static_assert(Rotatable<BorrowedRotatable<const ExternalRotor>>);
static_assert(std::is_constructible_v<BorrowedRotatable<ExternalRotor>, ExternalRotor&>);
static_assert(std::is_constructible_v<BorrowedRotatable<const ExternalRotor>,
                                      const ExternalRotor&>);
static_assert(!std::is_constructible_v<BorrowedRotatable<ExternalRotor>, ExternalRotor&&>);
static_assert(!std::is_constructible_v<BorrowedRotatable<const ExternalRotor>,
                                       ExternalRotor&&>);
static_assert(!std::is_constructible_v<BorrowedRotatable<const ExternalRotor>,
                                       const ExternalRotor&&>);

trait(Controllable,
    method(int, getValue, const noexcept),
    method(void, setValue, (param(int, value)), noexcept),
    method(int, add, (param(int, amount), param(int, multiplier)), ),
    method(void, consume, (param(std::unique_ptr<int>, value)), )
);

template<typename T>
using OwnedControllable = decltype(adapt(Controllable, std::declval<T>()));
template<typename T>
using BorrowedControllable = decltype(borrow(Controllable, std::declval<T&>()));
template<typename T>
concept CanBorrowControllable = requires(T& value) {
    borrow(Controllable, value);
};

struct ExternalControl {
    int value = 0;
};

impl(Controllable, ExternalControl) {
    static int getValue(const ExternalControl& control) noexcept {
        return control.value;
    }
    static void setValue(ExternalControl& control, int value) noexcept {
        control.value = value;
    }
    static int add(ExternalControl& control, int amount, int multiplier) {
        control.value += amount * multiplier;
        return control.value;
    }
    static void consume(ExternalControl& control, std::unique_ptr<int> value) {
        control.value = *value;
    }
};

struct IncompleteControl {};
impl(Controllable, IncompleteControl) {
    static int getValue(const IncompleteControl&) noexcept { return 0; }
};

trait(Reliable, method(int, read, const noexcept));

template<typename T>
using OwnedReliable = decltype(adapt(Reliable, std::declval<T>()));
template<typename T>
using BorrowedReliable = decltype(borrow(Reliable, std::declval<T&>()));
template<typename T>
concept CanBorrowReliable = requires(T& value) {
    borrow(Reliable, value);
};

struct ReliableValue {
    int value;
};
impl(Reliable, ReliableValue) {
    static int read(const ReliableValue& value) noexcept { return value.value; }
};

struct ThrowingValue {};
impl(Reliable, ThrowingValue) {
    static int read(const ThrowingValue&) { return 0; }
};

struct WrongArgumentControl {};
impl(Adjustable, WrongArgumentControl) {
    static float getRpm(const WrongArgumentControl&) { return 0; }
    static void setRpm(WrongArgumentControl&, double) {}
};

struct WrongMutableControl {};
impl(Adjustable, WrongMutableControl) {
    static float getRpm(const WrongMutableControl&) { return 0; }
    static void setRpm(const WrongMutableControl&, float) {}
};

static_assert(Controllable<ExternalControl> && !Controllable<const ExternalControl>);
static_assert(!Controllable<IncompleteControl>);
static_assert(!Adjustable<WrongArgumentControl> && !Adjustable<WrongMutableControl>);
static_assert(Reliable<ReliableValue> && Reliable<const ReliableValue>);
static_assert(!Reliable<ThrowingValue>);
static_assert(CanBorrowControllable<ExternalControl>);
static_assert(!CanBorrowControllable<IncompleteControl>);
static_assert(!CanBorrowAdjustable<WrongArgumentControl>);
static_assert(!CanBorrowAdjustable<WrongMutableControl>);
static_assert(Controllable<OwnedControllable<ExternalControl>>);
static_assert(Controllable<BorrowedControllable<ExternalControl>>);

static_assert(!CanBorrowControllable<const ExternalControl>);
static_assert(!std::is_constructible_v<BorrowedControllable<ExternalControl>,
                                       ExternalControl&&>);
static_assert(CanBorrowReliable<ReliableValue>);
static_assert(!CanBorrowReliable<ThrowingValue>);
static_assert(noexcept(std::declval<const OwnedReliable<ReliableValue>&>().read()));
static_assert(noexcept(std::declval<const BorrowedReliable<ReliableValue>&>().read()));
static_assert(noexcept(std::declval<const OwnedControllable<ExternalControl>&>()
                          .getValue()));
static_assert(noexcept(std::declval<BorrowedControllable<ExternalControl>&>().setValue(1)));

struct MoveOnlyRotor {
    std::unique_ptr<int> rpm;
};
impl(Rotatable, MoveOnlyRotor) {
    static float getRpm(const MoveOnlyRotor& rotor) {
        return static_cast<float>(*rotor.rpm);
    }
};

static_assert(!std::is_copy_constructible_v<MoveOnlyRotor>);
static_assert(std::is_constructible_v<OwnedRotatable<MoveOnlyRotor>, MoveOnlyRotor>);

struct DestructionSentinel {
    bool& destroyed;
    explicit DestructionSentinel(bool& destroyed) : destroyed(destroyed) {}
    ~DestructionSentinel() { destroyed = true; }
};

struct LifetimeRotor {
    std::unique_ptr<DestructionSentinel> sentinel;
};
impl(Rotatable, LifetimeRotor) {
    static float getRpm(const LifetimeRotor&) { return 1500; }
};

float readRpm(const Rotatable auto& object) {
    return object.getRpm();
}

int main()
{
    if (readRpm(Motor{}) != 1400) {
        return 1;
    }

    std::vector<std::unique_ptr<dyn(Rotatable)>> objects;
    objects.push_back(std::make_unique<Motor>());
    objects.push_back(std::make_unique<Spindle>());
    if (objects[0]->getRpm() != 1400 || objects[1]->getRpm() != 200 ||
        readRpm(*objects[0]) != 1400) {
        return 2;
    }

    bool destroyed = false;
    {
        std::unique_ptr<dyn(Rotatable)> object = std::make_unique<Tracked>(destroyed);
    }
    if (!destroyed) {
        return 3;
    }

    AdjustableMotor motor;
    dyn(Adjustable)& interface = motor;
    interface.setRpm(500);
    if (interface.getRpm() != 500) {
        return 4;
    }

    ExternalRotor rotor{2.0 * std::numbers::pi};
    auto ownedRotor = adapt(Rotatable, rotor);
    auto borrowedRotor = borrow(Rotatable, rotor);
    if (std::abs(readRpm(ownedRotor) - 60.0f) > 0.001f ||
        std::abs(readRpm(borrowedRotor) - 60.0f) > 0.001f) {
        return 5;
    }
    rotor.radiansPerSecond *= 2;
    if (std::abs(readRpm(ownedRotor) - 60.0f) > 0.001f ||
        std::abs(readRpm(borrowedRotor) - 120.0f) > 0.001f) {
        return 6;
    }

    const ExternalRotor constRotor{2.0 * std::numbers::pi};
    auto constBorrow = borrow(Rotatable, constRotor);
    const auto& constView = borrowedRotor;
    if (std::abs(readRpm(constBorrow) - 60.0f) > 0.001f ||
        std::abs(readRpm(constView) - 120.0f) > 0.001f) {
        return 7;
    }

    int rpm = 320;
    auto ownedInt = adapt(Rotatable, rpm);
    auto borrowedInt = borrow(Rotatable, rpm);
    rpm = 640;
    if (readRpm(ownedInt) != 320 || readRpm(borrowedInt) != 640) {
        return 8;
    }

    ExternalControl control{10};
    auto ownedControl = adapt(Controllable, control);
    auto borrowedControl = borrow(Controllable, control);
    dyn(Controllable)& borrowedInterface = borrowedControl;
    borrowedInterface.setValue(20);
    if (control.value != 20 || ownedControl.getValue() != 10 ||
        borrowedInterface.add(3, 4) != 32 || control.value != 32) {
        return 9;
    }
    control.value = 40;
    if (borrowedInterface.getValue() != 40) {
        return 10;
    }
    dyn(Controllable)& ownedInterface = ownedControl;
    ownedInterface.setValue(50);
    if (control.value != 40 || ownedInterface.add(2, 3) != 56) {
        return 11;
    }
    auto borrowedValue = std::make_unique<int>(70);
    auto ownedValue = std::make_unique<int>(80);
    borrowedInterface.consume(std::move(borrowedValue));
    ownedInterface.consume(std::move(ownedValue));
    if (borrowedValue || ownedValue || control.value != 70 ||
        borrowedInterface.getValue() != 70 || ownedInterface.getValue() != 80) {
        return 12;
    }

    auto reliable = adapt(Reliable, ReliableValue{42});
    ReliableValue reliableValue{43};
    auto reliableBorrow = borrow(Reliable, std::as_const(reliableValue));
    const dyn(Reliable)& reliableInterface = reliable;
    if (reliableInterface.read() != 42 || reliableBorrow.read() != 43) {
        return 13;
    }

    MoveOnlyRotor moveOnly{std::make_unique<int>(900)};
    auto ownedMoveOnly = adapt(Rotatable, std::move(moveOnly));
    if (moveOnly.rpm || readRpm(ownedMoveOnly) != 900) {
        return 14;
    }
    objects.push_back(box(Rotatable, MoveOnlyRotor{std::make_unique<int>(1200)}));
    objects.push_back(std::make_unique<BorrowedRotatable<ExternalRotor>>(rotor));
    if (objects[2]->getRpm() != 1200 ||
        std::abs(objects[3]->getRpm() - 120.0f) > 0.001f) {
        return 15;
    }

    bool adapterDestroyed = false;
    {
        LifetimeRotor source{std::make_unique<DestructionSentinel>(adapterDestroyed)};
        auto object = box(Rotatable, std::move(source));
        if (source.sentinel || adapterDestroyed || object->getRpm() != 1500) {
            return 16;
        }
    }
    if (!adapterDestroyed) {
        return 17;
    }

    bool borrowedDestroyed = false;
    {
        LifetimeRotor source{std::make_unique<DestructionSentinel>(borrowedDestroyed)};
        {
            std::unique_ptr<dyn(Rotatable)> object =
                std::make_unique<BorrowedRotatable<LifetimeRotor>>(source);
            if (object->getRpm() != 1500) {
                return 18;
            }
        }
        if (borrowedDestroyed || !source.sentinel) {
            return 19;
        }
    }
    if (!borrowedDestroyed) {
        return 20;
    }
}
