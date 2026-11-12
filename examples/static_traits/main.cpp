#include <concepts>
#include <iostream>
#include <type_traits>
#include "traits.hpp"

static_trait(ReadableRPM, T, requires(const T& object) {
    { object.readRPM() } -> std::same_as<double>;
});

// Ordinary types: no generated runtime interface or inherited base.
struct Motor {
    double readRPM() const { return 1200.0; }
};
struct WrongUnits {
    int readRPM() const { return 1200; }
};

static_assert(ReadableRPM<Motor>);
static_assert(!ReadableRPM<WrongUnits>);
static_assert(!std::is_polymorphic_v<Motor>);

double read(const ReadableRPM auto& object)
{
    return object.readRPM();
}

int main()
{
    const double rpm = read(Motor{});
    std::cout << "Static-only constraint: " << rpm << " RPM, no interface base\n";
    return rpm == 1200.0 ? 0 : 1;
}
