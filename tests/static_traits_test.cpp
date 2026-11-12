#include <cmath>
#include <concepts>

#include <numbers>
#include <utility>

#include "fixtures/rotatables.h"

struct PowerSupply : implements(Powerable) {
    float getPower() const override { return 750; }
};

struct StructuralMotor {
    float getRpm() const { return 1400; }
    float getPower() const { return 750; }
};

static_assert(PoweredRotatable<Motor>);
static_trait(StaticCloneable, T,
    requires(const T& object) {
        { object.clone() } -> std::same_as<T>;
    }
);
static_assert(StaticCloneable<Motor>);
static_assert(std::same_as<decltype(std::declval<const Motor&>().clone()), Motor>);

static_assert(Rotatable<Spindle>);
static_assert(!PoweredRotatable<Spindle>);
static_assert(Powerable<PowerSupply>);
static_assert(!PoweredRotatable<PowerSupply>);
static_assert(!PoweredRotatable<StructuralMotor>);
static_assert(!PoweredRotatable<int>);
static_assert(std::convertible_to<Motor&, dyn(Rotatable)&>);
static_assert(std::convertible_to<Motor&, dyn(Powerable)&>);

template<typename T>
concept CanReadPower = requires(const T& object) {
    readPower(object);
};

static_assert(CanReadPower<Motor>);
static_assert(!CanReadPower<Spindle>);
static_assert(!CanReadPower<PowerSupply>);

// Commas in constraint expressions are preserved by the variadic macro.
static_trait(IsInt, T, std::same_as<T, int>);
static_assert(IsInt<int>);
static_assert(!IsInt<float>);

static_trait(IntegerBuffer, T,
    requires(T& object, typename T::value_type value) {
        typename T::value_type;
        requires std::integral<typename T::value_type>;
        requires (T::capacity > 0);
        { object.push(value) } -> std::same_as<void>;
        { T::empty() } -> std::same_as<T>;
        { object.template read<int>() } -> std::same_as<int>;
    }
);

template<typename Value, int Capacity>
struct Buffer {
    using value_type = Value;
    static constexpr int capacity = Capacity;
    void push(Value) {}
    static Buffer empty() { return {}; }
    template<typename U>
    U read() const { return {}; }
};

static_assert(IntegerBuffer<Buffer<int, 4>>);
static_assert(!IntegerBuffer<Buffer<float, 4>>);
static_assert(!IntegerBuffer<Buffer<int, 0>>);
static_assert(!IntegerBuffer<int>);

namespace example {
    static_trait(Floating, T, std::floating_point<T>);
}
static_assert(example::Floating<double>);
static_assert(!example::Floating<int>);

int main()
{
    Motor motor{1400, 5, "powered motor"};
    const float expected = 5.0f * 1400.0f * 2.0f * std::numbers::pi_v<float> / 60.0f;
    if (std::abs(readPower(motor) - expected) > 0.001f) {
        return 1;
    }

    const dyn(Rotatable)& rotating = motor;
    const dyn(Powerable)& powered = motor;
    if (rotating.getRpm() != 1400 ||
        std::abs(powered.getPower() - expected) > 0.001f) {
        return 2;
    }

    Motor copy = motor.clone();
    if (copy.rpm != motor.rpm || copy.torque != motor.torque || copy.model != motor.model) {
        return 3;
    }
    copy.rpm = 700;
    if (motor.rpm != 1400) {
        return 4;
    }

}
