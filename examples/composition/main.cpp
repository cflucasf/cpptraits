#include <iostream>
#include "traits.hpp"

trait(Rotatable,
    method(double, readRPM, const)
);
trait(Powered,
    method(double, readWatts, const)
);
composed_trait(PoweredRotatable, T, Rotatable, Powered);

struct Motor : implements(Rotatable), implements(Powered) {
    double rpm = 1400.0;
    double readRPM() const override { return rpm; }
    double readWatts() const override { return 500.0; }
};

static_assert(PoweredRotatable<Motor>);

int main()
{
    Motor motor;
    auto view = borrow(PoweredRotatable, motor);
    auto owned = adapt(PoweredRotatable, motor);
    motor.rpm = 700.0;
    const double rpm = view.as<dyn(Rotatable)>().readRPM();
    const double watts = view.as<dyn(Powered)>().readWatts();
    const double copiedRPM = owned.as<dyn(Rotatable)>().readRPM();
    std::cout << "Composed view: " << rpm << " RPM / " << watts << " watts\n"
              << "Owned bundle retains " << copiedRPM << " RPM\n";
    return rpm == 700.0 && watts == 500.0 && copiedRPM == 1400.0 ? 0 : 1;
}
