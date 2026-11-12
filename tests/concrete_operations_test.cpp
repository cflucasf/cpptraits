#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>

#include "fixtures/rotatables.h"

namespace {

static_trait(StaticCombinable, T,
    requires(T& object, const T& other) {
        { object.combine(other) } -> std::same_as<void>;
    }
);
static_trait(StaticCloneable, T,
    requires(const T& object) {
        { object.clone() } -> std::same_as<T>;
    }
);

struct PlainMotor {
    float rpm;
    float torque;

    void combine(const PlainMotor& other) {
        rpm = (rpm + other.rpm) / 2;
        torque = (torque + other.torque) / 2;
    }

    PlainMotor clone() const { return *this; }
};

struct PlainCounter {
    int value;

    void combine(const PlainCounter& other) { value += other.value; }
    PlainCounter clone() const { return *this; }
};

struct WrongCombineReturn {
    bool combine(const WrongCombineReturn&) { return true; }
};

struct WrongCombineOperand {
    void combine(const PlainMotor&) {}
};

struct MutableCombineOperand {
    void combine(MutableCombineOperand&) {}
};

struct WrongCloneReturn {
    int clone() const { return 0; }
};

struct ReferenceClone {
    const ReferenceClone& clone() const { return *this; }
};

struct MutableClone {
    MutableClone clone() { return *this; }
};

static_assert(StaticCombinable<PlainMotor> && StaticCloneable<PlainMotor>);
static_assert(StaticCombinable<PlainCounter> && StaticCloneable<PlainCounter>);
static_assert(!std::is_polymorphic_v<PlainMotor>);
static_assert(!std::is_polymorphic_v<PlainCounter>);
static_assert(std::is_standard_layout_v<PlainMotor>);
static_assert(std::is_standard_layout_v<PlainCounter>);
static_assert(std::is_trivially_copyable_v<PlainMotor>);
static_assert(std::is_trivially_copyable_v<PlainCounter>);
static_assert(sizeof(PlainMotor) == 2 * sizeof(float));
static_assert(sizeof(PlainCounter) == sizeof(int));
static_assert(alignof(PlainMotor) == alignof(float));
static_assert(alignof(PlainCounter) == alignof(int));
static_assert(!StaticCombinable<WrongCombineReturn>);
static_assert(!StaticCombinable<WrongCombineOperand>);
static_assert(!StaticCombinable<MutableCombineOperand>);
static_assert(!StaticCloneable<WrongCloneReturn>);
static_assert(!StaticCloneable<ReferenceClone>);
static_assert(!StaticCloneable<MutableClone>);
static_assert(!StaticCombinable<int> && !StaticCloneable<int>);

// Deduction requires both operands to have the same concrete type.
template<StaticCombinable T>
void combineSameType(T& left, const T& right) {
    left.combine(right);
}

template<typename Left, typename Right>
concept CanCombine = requires(Left& left, const Right& right) {
    combineSameType(left, right);
};

static_assert(CanCombine<PlainMotor, PlainMotor>);
static_assert(!CanCombine<PlainMotor, PlainCounter>);

static_assert(Combinable<Motor>);
static_assert(!std::convertible_to<Motor&, dyn(Combinable)&>);
template<typename T>
concept CanCombineErased = requires(T& object, const dyn(Combinable)& other) {
    object.combine(other);
};
static_assert(!CanCombineErased<Motor>);
static_assert(CanCombineErased<dyn(Combinable)>);

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

} // namespace

impl(Rotatable, PlainMotor) {
    static float getRpm(const PlainMotor& object) { return object.rpm; }
};

impl(Rotatable, PlainCounter) {
    static float getRpm(const PlainCounter& object) {
        return static_cast<float>(object.value);
    }
};

// Mapping declarations do not add bases, members or virtual functions to T.
static_assert(Rotatable<PlainMotor> && Rotatable<PlainCounter>);
static_assert(!std::convertible_to<PlainMotor&, dyn(Rotatable)&>);
static_assert(!std::convertible_to<PlainCounter&, dyn(Rotatable)&>);
static_assert(sizeof(PlainMotor) == 2 * sizeof(float));
static_assert(sizeof(PlainCounter) == sizeof(int));
static_assert(!std::is_polymorphic_v<PlainMotor>);
static_assert(!std::is_polymorphic_v<PlainCounter>);

int main() {
    PlainMotor plain{1400, 5};
    auto plainCopy = plain.clone();
    plainCopy.rpm = 700;
    plainCopy.torque = 3;
    check(plain.rpm == 1400 && plain.torque == 5,
          "plain clone changes must not affect the source");
    combineSameType(plain, plainCopy);
    check(plain.rpm == 1050 && plain.torque == 4,
          "plain combine must average the concrete payload");
    check(plainCopy.rpm == 700 && plainCopy.torque == 3,
          "combine must not change its const operand");

    PlainCounter counter{10};
    auto counterCopy = counter.clone();
    counterCopy.value = 20;
    check(counter.value == 10, "counter clone must be independent");
    combineSameType(counter, counterCopy);
    check(counter.value == 30 && counterCopy.value == 20,
          "another inheritance-free type must support concrete operations");

    auto borrowed = borrow(Rotatable, plain);
    auto owned = adapt(Rotatable, plain);
    plain.rpm = 1200;
    check(borrowed.getRpm() == 1200 && owned.getRpm() == 1050,
          "runtime adaptation must preserve borrow and value ownership");
    check(!borrowed.isStopped(), "external mappings must retain shared defaults");

    std::vector<std::unique_ptr<dyn(Rotatable)>> objects;
    objects.push_back(box(Rotatable, plain));
    objects.push_back(box(Rotatable, counter));
    check(objects[0]->getRpm() == 1200 && objects[1]->getRpm() == 30,
          "different inheritance-free types must share a runtime container");
    plain.rpm = 0;
    check(borrowed.isStopped() && objects[0]->getRpm() == 1200,
          "boxed values must remain independent of the source");

    Motor motor{1400, 5, "original"};
    auto motorCopy = motor.clone();
    motorCopy.rpm = 700;
    motorCopy.torque = 3;
    motorCopy.model = "copy";
    check(motor.rpm == 1400 && motor.torque == 5 && motor.model == "original",
          "Motor clone must copy all state independently");
    combineSameType(motor, motorCopy);
    check(motor.rpm == 1050 && motor.torque == 4 && motor.model == "original",
          "Motor combine must preserve the expected 1050 RPM behavior");
    check(motorCopy.rpm == 700 && motorCopy.torque == 3 && motorCopy.model == "copy",
          "Motor combine must preserve its operand");

    const dyn(Rotatable)& rotating = motor;
    const dyn(Powerable)& powered = motor;
    check(rotating.getRpm() == 1050 && powered.getPower() == motor.getPower(),
          "Motor must retain its fixed runtime trait interfaces");

    Motor runtimeMotor{1400, 5, "runtime receiver"};
    auto runtimeReceiver = borrow(Combinable, runtimeMotor);
    auto runtimeOperand = adapt(Combinable, Motor{700, 3, "runtime operand"});
    const dyn(Combinable)& erasedOperand = runtimeOperand;
    runtimeReceiver.combine(erasedOperand);
    check(runtimeMotor.rpm == 1050 && runtimeMotor.torque == 4,
          "a mapped Motor must satisfy Combinable and combine through its runtime adapter");

    return failures == 0 ? 0 : 1;
}
