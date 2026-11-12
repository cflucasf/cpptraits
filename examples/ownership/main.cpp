#include <iostream>
#include <memory>
#include <vector>
#include "traits.hpp"

trait(Rotatable,
    method(double, readRPM, const)
);

struct Motor { double rpm; };
struct Spindle { double rpm; };

impl(Rotatable, Motor) {
    static double readRPM(const Motor& motor) { return motor.rpm; }
};
impl(Rotatable, Spindle) {
    static double readRPM(const Spindle& spindle) { return spindle.rpm; }
};

int main()
{
    Motor motor{700.0};
    auto owned = adapt(Rotatable, motor); // Owns a copy of this lvalue.
    auto borrowed = borrow(Rotatable, motor); // motor must outlive this view.
    motor.rpm = 1400.0;
    std::cout << "After source mutation: owned " << owned.readRPM()
              << " RPM, borrowed " << borrowed.readRPM() << " RPM\n";

    std::vector<std::unique_ptr<dyn(Rotatable)>> objects;
    objects.push_back(box(Rotatable, Motor{900.0}));
    objects.push_back(box(Rotatable, Spindle{300.0}));
    double total = 0.0;
    for (const auto& object : objects) {
        std::cout << "Boxed object: " << object->readRPM() << " RPM\n";
        total += object->readRPM();
    }
    return owned.readRPM() == 700.0 && borrowed.readRPM() == 1400.0
        && objects.size() == 2 && total == 1200.0 ? 0 : 1;
}
