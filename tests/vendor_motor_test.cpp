#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <type_traits>
#include <utility>
#include <vector>

#include "fixtures/external_rotatable.h"

composed_trait(VendorRotation, T, Rotatable);

template<typename T>
concept CanAdapt = requires(T&& object) {
    adapt(Rotatable, std::forward<T>(object));
};

template<typename T>
concept CanBorrow = requires(T&& object) {
    borrow(Rotatable, std::forward<T>(object));
};

template<typename T>
concept CanBox = requires(T&& object) {
    box(Rotatable, std::forward<T>(object));
};

template<class T>
concept CanCallGetRpm = requires(T& object) {
    { trait_call(Rotatable, getRpm, object) } -> std::same_as<float>;
};

using MoveOnlyRpm = std::unique_ptr<int>;
impl(Rotatable, MoveOnlyRpm) {
    static float getRpm(const MoveOnlyRpm& rpm) {
        return static_cast<float>(*rpm);
    }
};

struct SharedMotor {
    std::shared_ptr<int> rpm;
};
impl(Rotatable, SharedMotor) {
    static float getRpm(const SharedMotor& motor) {
        return static_cast<float>(*motor.rpm);
    }
};

namespace vendor {
template<class Speed, std::size_t Channels>
struct MultiChannelMotor {
    Speed rpm;
};
}

impl(Rotatable,
     (template<class Speed, std::size_t Channels> requires (Channels > 0)),
     (vendor::MultiChannelMotor<Speed, Channels>)) {
    static float getRpm(const vendor::MultiChannelMotor<Speed, Channels>& motor) {
        return static_cast<float>(motor.rpm);
    }
    static float getRpm(vendor::MultiChannelMotor<Speed, Channels>&) { return -1.0f; }
};

using CalibratedMotor = vendor::MultiChannelMotor<int, 2>;
impl(Rotatable, CalibratedMotor) {
    static float getRpm(const CalibratedMotor& motor) {
        return static_cast<float>(motor.rpm + 100);
    }
};

using UnmappedMotor = vendor::MultiChannelMotor<int, 0>;
static_assert(Rotatable<vendor::MultiChannelMotor<double, 1>>);
static_assert(Rotatable<CalibratedMotor> && Rotatable<const CalibratedMotor>);
static_assert(!Rotatable<UnmappedMotor>);
static_assert(!CanAdapt<UnmappedMotor> && !CanBorrow<UnmappedMotor&> &&
              !CanBox<UnmappedMotor>);
static_assert(CanCallGetRpm<vendor::Motor> && CanCallGetRpm<const CalibratedMotor>);
static_assert(CanCallGetRpm<vendor::MultiChannelMotor<double, 1>>);
static_assert(!CanCallGetRpm<UnmappedMotor> && CanCallGetRpm<dyn(Rotatable)>);

trait(Mutable, method(void, set, (param(int, value)), ));
impl(Mutable, int) {
    static void set(int& object, int value) { object = value; }
};

template<typename T>
concept CanBorrowMutable = requires(T&& object) {
    borrow(Mutable, std::forward<T>(object));
};

static_assert(Rotatable<vendor::Motor> && Rotatable<const vendor::Motor>);
static_assert(VendorRotation<vendor::Motor> && VendorRotation<const vendor::Motor>);
static_assert(!std::convertible_to<vendor::Motor&, dyn(Rotatable)&>);
static_assert(Mutable<int> && !Mutable<const int>);
static_assert(!std::is_polymorphic_v<vendor::Motor>);
static_assert(std::has_virtual_destructor_v<dyn(Rotatable)>);
static_assert(CanAdapt<vendor::Motor&>);
static_assert(CanAdapt<const vendor::Motor&>);
static_assert(CanAdapt<vendor::Motor>);
static_assert(CanBorrow<vendor::Motor&>);
static_assert(CanBorrow<const vendor::Motor&>);
static_assert(!CanBorrow<vendor::Motor>);
static_assert(!CanBorrow<const vendor::Motor>);
static_assert(!CanAdapt<int>);
static_assert(!CanBorrow<int&>);
static_assert(!CanBox<int>);
static_assert(CanAdapt<MoveOnlyRpm>);
static_assert(CanBox<MoveOnlyRpm>);
static_assert(!CanAdapt<MoveOnlyRpm&>);
static_assert(!CanBox<MoveOnlyRpm&>);
static_assert(!CanAdapt<const MoveOnlyRpm&>);
static_assert(CanBorrowMutable<int&>);
static_assert(!CanBorrowMutable<const int&>);
static_assert(noexcept(borrow(Rotatable, std::declval<vendor::Motor&>())));
static_assert(std::same_as<decltype(box(Rotatable, std::declval<vendor::Motor>())),
                           std::unique_ptr<dyn(Rotatable)>>);
using ConstMotorBorrow = decltype(borrow(Rotatable,
                                        std::declval<const vendor::Motor&>()));
static_assert(Rotatable<ConstMotorBorrow>);
static_assert(std::convertible_to<ConstMotorBorrow&, dyn(Rotatable)&>);
static_assert(!std::is_constructible_v<ConstMotorBorrow, vendor::Motor&&>);
static_assert(!std::is_constructible_v<ConstMotorBorrow, const vendor::Motor&&>);

bool optionalVendorOverride()
{
    for (double speed : {0.005, -0.005, 0.02, -0.02, 0.01, -0.01, 0.0}) {
        const bool stopped = std::abs(speed) < 0.01;
        const vendor::Motor source{speed};
        auto owned = adapt(Rotatable, source);
        auto borrowed = borrow(Rotatable, source);
        auto boxed = box(Rotatable, source);
        auto matches = [=](const dyn(Rotatable)& object) {
            return object.isStopped() == stopped &&
                   (speed == 0.0 ? object.getRpm() == 0.0f : object.getRpm() != 0.0f);
        };
        // The small nonzero cases distinguish the vendor override from the
        // interface default, which only accepts exactly zero RPM.
        if (owned.isStopped() != stopped || borrowed.isStopped() != stopped ||
            !matches(owned) || !matches(borrowed) || !matches(*boxed)) {
            return false;
        }

        // Composition still requires runtime interfaces, not just mapping satisfaction.
        auto composedBorrow = borrow(VendorRotation, borrowed);
        auto composedOwned = adapt(VendorRotation, owned);
        auto composedBox = box(VendorRotation, owned);
        if (!matches(composedBorrow.as<dyn(Rotatable)>()) ||
            !matches(composedOwned.as<dyn(Rotatable)>()) ||
            !matches(composedBox->as<dyn(Rotatable)>())) {
            return false;
        }
    }
    // A required-only mapping still inherits the exact-zero interface default.
    SharedMotor source{std::make_shared<int>(1)};
    auto fallback = borrow(Rotatable, source);
    const dyn(Rotatable)& interface = fallback;
    if (interface.isStopped()) {
        return false;
    }
    *source.rpm = 0;
    return interface.isStopped();
}

int main()
{
    if (!optionalVendorOverride()) {
        return 9;
    }

    vendor::MultiChannelMotor<double, 1> genericMotor{250.5};
    auto genericBorrow = borrow(Rotatable, genericMotor);
    auto genericOwned = adapt(Rotatable, genericMotor);
    auto genericBox = box(Rotatable, genericMotor);
    genericMotor.rpm = 500;
    const CalibratedMotor calibrated{250};
    if (genericBorrow.getRpm() != 500.0f || genericOwned.getRpm() != 250.5f ||
        genericBox->getRpm() != 250.5f || borrow(Rotatable, calibrated).getRpm() != 350.0f ||
        trait_call(Rotatable, getRpm, genericMotor) != 500.0f ||
        trait_call(Rotatable, getRpm, calibrated) != 350.0f) {
        return 10;
    }

    vendor::Motor motor{2.0 * std::numbers::pi};
    auto owned = adapt(Rotatable, motor);
    auto borrowed = borrow(Rotatable, motor);
    const vendor::Motor stopped{0};
    auto stoppedRef = borrow(Rotatable, stopped);

    if (std::abs(readRpm(owned) - 60.0f) > 0.001f ||
        std::abs(readRpm(borrowed) - 60.0f) > 0.001f ||
        readRpm(stoppedRef) != 0) {
        return 1;
    }

    motor.speed *= 2;
    std::vector<std::unique_ptr<dyn(Rotatable)>> objects;
    objects.push_back(box(Rotatable, vendor::Motor{2.0 * std::numbers::pi}));
    objects.push_back(box(Rotatable, motor));
    const dyn(Rotatable)& borrowedInterface = borrowed;
    if (std::abs(objects[0]->getRpm() - 60.0f) > 0.001f ||
        std::abs(objects[1]->getRpm() - 120.0f) > 0.001f ||
        std::abs(readRpm(owned) - 60.0f) > 0.001f ||
        std::abs(borrowedInterface.getRpm() - 120.0f) > 0.001f) {
        return 2;
    }
    motor.speed = 0;
    if (borrowed.getRpm() != 0 ||
        std::abs(objects[1]->getRpm() - 120.0f) > 0.001f) {
        return 3;
    }

    auto source = std::make_unique<int>(900);
    auto moved = adapt(Rotatable, std::move(source));
    auto boxedSource = std::make_unique<int>(1200);
    auto boxed = box(Rotatable, std::move(boxedSource));
    if (source || boxedSource || readRpm(moved) != 900 || boxed->getRpm() != 1200) {
        return 4;
    }

    std::weak_ptr<int> lifetime;
    {
        SharedMotor original{std::make_shared<int>(500)};
        lifetime = original.rpm;
        auto object = box(Rotatable, std::move(original));
        if (original.rpm || lifetime.expired() || object->getRpm() != 500) {
            return 5;
        }
    }
    if (!lifetime.expired()) {
        return 6;
    }

    int value = 10;
    auto mutableRef = borrow(Mutable, value);
    auto mutableOwned = adapt(Mutable, value);
    auto mutableBox = box(Mutable, value);
    mutableRef.set(20);
    mutableOwned.set(30);
    mutableBox->set(40);
    if (value != 20) {
        return 7;
    }

    // Variadic macros must preserve commas in initialization and evaluate once.
    int evaluations = 0;
    auto once = adapt(Rotatable, vendor::Motor{(++evaluations, 0.0)});
    auto boxedOnce = box(Rotatable, vendor::Motor{(++evaluations, 0.0)});
    if (evaluations != 2 || once.getRpm() != 0 || boxedOnce->getRpm() != 0) {
        return 8;
    }
}
