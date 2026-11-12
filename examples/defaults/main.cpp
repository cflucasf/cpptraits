#include <cmath>
#include <iostream>
#include "traits.hpp"

trait(Rotatable,
    method(double, readRPM, const),
    default_method(bool, isStopped, (), const, {
        return readRPM() == 0.0;
    })
);

struct Motor { double rpm; };
struct Sensor { double rpm; };

impl(Rotatable, Motor) {
    static double readRPM(const Motor& motor) { return motor.rpm; }
    // Omitting isStopped selects the provided trait body.
};
impl(Rotatable, Sensor) {
    static double readRPM(const Sensor& sensor) { return sensor.rpm; }
    static bool isStopped(const Sensor& sensor)
    {
        return std::abs(sensor.rpm) < 1.0; // Sensor-specific deadband.
    }
};

static_assert(Rotatable<Motor> && Rotatable<Sensor>);

int main()
{
    Motor motor{0.5};
    Sensor sensor{0.5};
    auto normal = borrow(Rotatable, motor);
    auto overridden = borrow(Rotatable, sensor);
    const bool moving = !normal.isStopped();
    const bool stopped = overridden.isStopped();
    motor.rpm = 0.0;
    const bool zeroStopped = normal.isStopped();
    std::cout << std::boolalpha
              << "At 0.5 RPM: default stopped=" << !moving
              << ", external override stopped=" << stopped << '\n'
              << "At zero RPM: default stopped=" << zeroStopped << '\n';
    return moving && stopped && zeroStopped ? 0 : 1;
}
