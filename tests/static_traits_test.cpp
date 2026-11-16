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

namespace example {
trait(Associated,
    method(Item, read, (param(Offset, index)), const noexcept),
    associated_type(Item),
    associated_type(Offset),
    method(void, write, (param(Item, value)), )
);
}

struct ValidMapping {
    int value = 0;
};
struct MissingAlias {};
struct WrongSignature {};
struct PrivateAlias {};
struct MissingMapping {};

impl(example::Associated, ValidMapping) {
    using Item = int;
    using Offset = unsigned;
    static int read(const ValidMapping& object, unsigned) noexcept { return object.value; }
    static int read(ValidMapping&, unsigned) noexcept { return -1; }
    static void write(ValidMapping& object, int value) { object.value = value; }
    static void write(const ValidMapping&, int) {}
};

impl(example::Associated, MissingAlias) {
    using Item = int;
    static int read(const MissingAlias&, unsigned) noexcept { return 0; }
    static void write(MissingAlias&, int) {}
};

impl(example::Associated, WrongSignature) {
    using Item = int;
    using Offset = unsigned;
    static int read(const WrongSignature&, unsigned) { return 0; }
    static void write(WrongSignature&, double) {}
};

impl(example::Associated, PrivateAlias) {
    using Offset = unsigned;
    static int read(const PrivateAlias&, unsigned) noexcept { return 0; }
    static void write(PrivateAlias&, int) {}
private:
    using Item = int;
};

template<class T>
concept HasAssociatedMapping = requires { typename trait_mapping_t(example::Associated, T); };

template<class T>
concept CanCallMappedRead = requires(T& object) {
    { trait_call(example::Associated, read, object, 0u) } noexcept -> std::same_as<int>;
};

template<class T>
concept CanCallMappedWrite = requires(T& object) {
    trait_call(example::Associated, write, object, 42);
};

static_assert(example::Associated<ValidMapping>);
static_assert(!example::Associated<const ValidMapping>);
static_assert(!example::Associated<MissingAlias>);
static_assert(!example::Associated<WrongSignature>);
static_assert(!example::Associated<PrivateAlias>);
static_assert(!example::Associated<MissingMapping>);
static_assert(HasAssociatedMapping<ValidMapping> && !HasAssociatedMapping<MissingAlias>);
static_assert(std::same_as<trait_mapping_t(example::Associated, ValidMapping)::Item, int>);
static_assert(CanCallMappedRead<ValidMapping> && CanCallMappedRead<const ValidMapping>);
static_assert(!CanCallMappedRead<MissingAlias> && !CanCallMappedRead<WrongSignature>);
static_assert(CanCallMappedWrite<ValidMapping> && !CanCallMappedWrite<const ValidMapping>);

template<class Element>
struct NativeAssociated : implements(example::Associated, Element, unsigned) {
    Element read(unsigned) const noexcept override { return {}; }
    void write(Element) override {}
};

struct ShadowedBinding : NativeAssociated<int> {
    using Item = double;
};

struct PlainAssociated {
    using Item = int;
    using Offset = unsigned;
    int read(unsigned) const noexcept { return 0; }
    void write(int) {}
};

static_assert(example::Associated<NativeAssociated<int>>);
static_assert(example::Associated<NativeAssociated<std::pair<int, bool>>>);
static_assert(std::same_as<NativeAssociated<int>::Item, int>);
static_assert(std::same_as<NativeAssociated<int>::Offset, unsigned>);
static_assert(!example::Associated<const NativeAssociated<int>>);
static_assert(!example::Associated<ShadowedBinding>);
static_assert(!example::Associated<PlainAssociated>);
static_assert(!HasAssociatedMapping<NativeAssociated<int>>);

int main()
{
    ValidMapping mapped;
    int evaluations = 0;
    trait_call(example::Associated, write, (++evaluations, mapped), 42);
    if (evaluations != 1 || trait_call(example::Associated, read, mapped, 0u) != 42) {
        return 5;
    }

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
