#include <iostream>
#include <memory>
#include <vector>
#include "traits.hpp"

trait(Editable,
    method(double, readRPM, const),
    concrete_method(combine, merge),
    concrete_method(clone, clone)
);

struct Motor { double rpm; };

impl(Editable, Motor) {
    static double readRPM(const Motor& motor) { return motor.rpm; }
    static void combine(Motor& motor, const Motor& other)
    {
        motor.rpm = (motor.rpm + other.rpm) / 2.0;
    }
    static Motor clone(const Motor& motor) { return motor; }
};

static_assert(Editable<Motor>); // The raw type satisfies the mapped contract.

int main()
{
    Motor motor{1400.0};
    Motor other{700.0};
    auto view = borrow(Editable, motor);
    auto otherView = borrow(Editable, other);
    const dyn(Editable)& operand = otherView;
    view.combine(operand); // Recovers matching Motor operands behind interfaces.
    const double mergedRPM = motor.rpm;

    const dyn(Editable)& source = view;
    std::unique_ptr<dyn(Editable)> cloned = source.clone(); // Always owns a Motor.
    motor.rpm = 0.0;
    const double clonedRPM = cloned->readRPM();
    cloned->combine(operand); // Owned and borrowed wrappers interoperate.
    const double combinedCloneRPM = cloned->readRPM();

    std::vector<std::unique_ptr<dyn(Editable)>> copies;
    copies.push_back(cloned->clone());
    copies.push_back(source.clone());
    std::cout << "Merged source: " << mergedRPM << " RPM\n"
              << "Independent owning clone: " << clonedRPM << " RPM\n"
              << "Clone merged through const interface: " << combinedCloneRPM
              << " RPM\n"
              << "Vector clones: " << copies[0]->readRPM() << " and "
              << copies[1]->readRPM() << " RPM\n";
    return mergedRPM == 1050.0 && clonedRPM == 1050.0
        && combinedCloneRPM == 875.0 && motor.rpm == 0.0
        && copies.size() == 2 && copies[0]->readRPM() == 875.0
        && copies[1]->readRPM() == 0.0 ? 0 : 1;
}
