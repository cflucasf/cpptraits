#pragma once

#include <cmath>
#include <numbers>
#include <utility>

#include "rotatables.h"
#include "vendor_motor.hpp"

impl(Rotatable, vendor::Motor) {
    static float getRpm(const vendor::Motor& motor) {
        return static_cast<float>(motor.speed * 60.0 / (2.0 * std::numbers::pi));
    }

    static bool isStopped(const vendor::Motor& motor) {
        // Treat small angular velocities as sensor noise (speed is in rad/s).
        return std::abs(motor.speed) < 0.01;
    }
};

static_assert(Rotatable<vendor::Motor>);
static_assert(Rotatable<decltype(adapt(Rotatable, std::declval<vendor::Motor>()))>);
static_assert(Rotatable<decltype(borrow(Rotatable, std::declval<const vendor::Motor&>()))>);
