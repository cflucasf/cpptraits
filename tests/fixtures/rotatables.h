#pragma once

#include <numbers>
#include <string>
#include <utility>

#include "traits.hpp"

trait(Rotatable,
    method(float, getRpm, const),
    default_method(bool, isStopped, (), const, {
        return getRpm() == 0.0f;
    })
);

trait(Powerable,
    method(float, getPower, const)
);

trait(Combinable,
    concrete_method(combine, merge)
);

trait(Cloneable,
    concrete_method(clone, clone)
);

composed_trait(PoweredRotatable, T, Rotatable, Powerable);

struct Motor final
    : implements(Rotatable)
    , implements(Powerable)
{
    float rpm;
    float torque;
    std::string model;

    Motor(float rpm, float torque, std::string model)
        : rpm(rpm), torque(torque), model(std::move(model)) {}

    void combine(const Motor& other) {
        rpm += other.rpm;
        torque += other.torque;
        rpm /= 2;
        torque /= 2;
    }

    Motor clone() const {
        return *this;
    }

    float getRpm() const override {
        return rpm;
    }

    float getPower() const override {
        // Mechanical power in watts from torque in N*m and speed in RPM.
        return torque * rpm * (2.0f * std::numbers::pi_v<float> / 60.0f);
    }
};

impl(Combinable, Motor) {
    static void combine(Motor& obj, const Motor& other){
        obj.combine(other);
    }
};

impl(Cloneable, Motor) {
    static Motor clone(const Motor& object) {
        return object.clone();
    }
};

struct Spindle : implements(Rotatable) {
    float rpm;
    std::string tool;

    Spindle(float rpm, std::string tool)
        : rpm(rpm), tool(std::move(tool)) {}

    float getRpm() const override {
        return rpm;
    }
};

impl(Cloneable, Spindle) {
    static Spindle clone(const Spindle& object) {
        return object;
    }
};

inline float readRpm(const Rotatable auto& object) {
    return object.getRpm();
}

inline float readPower(const PoweredRotatable auto& object) {
    return object.getPower();
}

static_assert(Rotatable<Motor>);
static_assert(Powerable<Motor>);
static_assert(Combinable<Motor>);
static_assert(Cloneable<Motor>);
static_assert(Cloneable<Spindle>);
static_assert(PoweredRotatable<Motor>);
static_assert(!PoweredRotatable<Spindle>);
static_assert(Rotatable<Spindle>);
static_assert(std::convertible_to<Motor&, dyn(Rotatable)&>);
static_assert(std::convertible_to<Spindle&, dyn(Rotatable)&>);
