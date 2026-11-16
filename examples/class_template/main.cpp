#include <cstddef>

#include "traits.hpp"

trait(Rotatable, method(float, getRpm, const));

template<class Speed, std::size_t Channels>
struct Motor {
    Speed rpm;
};

impl(Rotatable,
     (template<class Speed, std::size_t Channels> requires (Channels > 0)),
     (Motor<Speed, Channels>)) {
    static float getRpm(const Motor<Speed, Channels>& motor) {
        return static_cast<float>(motor.rpm);
    }
};

static_assert(Rotatable<Motor<int, 1>>);
static_assert(Rotatable<Motor<double, 2>>);
static_assert(!Rotatable<Motor<int, 0>>);

int main() {
    Motor<int, 1> single{900};
    Motor<double, 2> dual{1200.5};
    auto borrowed = borrow(Rotatable, single);
    auto boxed = box(Rotatable, dual);
    return borrowed.getRpm() == 900.0f && boxed->getRpm() == 1200.5f ? 0 : 1;
}
