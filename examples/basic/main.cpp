#include <iostream>
#include "traits.hpp"

trait(Rotatable,
    method(double, readRPM, const)
);

struct Motor : implements(Rotatable) {
    double readRPM() const override { return 1400.0; }
};

static_assert(Rotatable<Motor>);

double readStatic(const Rotatable auto& object)
{
    return object.readRPM();
}

double readRuntime(const dyn(Rotatable)& object)
{
    return object.readRPM();
}

int main()
{
    Motor motor;
    const double direct = readStatic(motor);
    const double erased = readRuntime(motor);
    std::cout << "Native motor: static " << direct
              << " RPM, runtime " << erased << " RPM\n";
    return direct == 1400.0 && erased == 1400.0 ? 0 : 1;
}
