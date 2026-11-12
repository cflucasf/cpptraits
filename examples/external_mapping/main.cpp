#include <cmath>
#include <iostream>
#include <numbers>
#include "traits.hpp"

trait(Rotatable,
    method(double, readRPM, const)
);

// A vendor type with no trait inheritance or trait-named methods.
namespace vendor {
struct Motor { double speed; }; // radians per second
}

impl(Rotatable, vendor::Motor) {
    static double readRPM(const vendor::Motor& motor)
    {
        return motor.speed * 60.0 / (2.0 * std::numbers::pi);
    }
};

static_assert(Rotatable<vendor::Motor>);

int main()
{
    vendor::Motor motor{2.0 * std::numbers::pi};
    auto view = borrow(Rotatable, motor);
    const dyn(Rotatable)& runtime = view;
    const double rpm = runtime.readRPM();
    std::cout << "Vendor motor: " << motor.speed << " rad/s -> "
              << rpm << " RPM (borrowed)\n";
    return std::abs(rpm - 60.0) < 1e-9 ? 0 : 1;
}
