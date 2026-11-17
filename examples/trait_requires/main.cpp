#include <cmath>
#include <concepts>
#include <iostream>
#include <string_view>

#include "traits.hpp"

// This reusable constraint alone generates no interface or adapters.
static_trait(Calibrated, T, requires {
    requires T::calibrated;
});

trait(TemperatureSource,
    // Self is a caller-chosen type parameter, not a keyword or library macro.
    trait_requires(Self, Calibrated<Self>),
    // Multiple clauses are ANDed, and the placeholder can have another name.
    trait_requires(Source, std::copy_constructible<Source>),
    method(double, readCelsius, const noexcept)
);

trait(Named,
    method(std::string_view, name, const noexcept)
);
composed_trait(NamedTemperatureSource, T, TemperatureSource, Named);

template<bool IsCalibrated>
struct Sensor : implements(TemperatureSource), implements(Named) {
    static constexpr bool calibrated = IsCalibrated;
    double celsius;

    explicit Sensor(double value) : celsius(value) {}
    double readCelsius() const noexcept override { return celsius; }
    std::string_view name() const noexcept override { return "native sensor"; }
};

namespace vendor {
// A third-party type has neither trait inheritance nor readCelsius().
template<bool IsCalibrated>
struct Probe {
    static constexpr bool calibrated = IsCalibrated;
    double kelvin;
};
}

impl(TemperatureSource,
     (template<bool IsCalibrated>),
     (vendor::Probe<IsCalibrated>)) {
    static double readCelsius(const vendor::Probe<IsCalibrated>& probe) noexcept {
        return probe.kelvin - 273.15;
    }
};

template<class T>
concept CanBorrowTemperature = requires(T& value) {
    borrow(TemperatureSource, value);
};

static_assert(TemperatureSource<Sensor<true>>);
static_assert(TemperatureSource<vendor::Probe<true>>);
// Correct method signatures or a valid mapping alone are not enough.
static_assert(!TemperatureSource<Sensor<false>>);
static_assert(!TemperatureSource<vendor::Probe<false>>);
static_assert(!CanBorrowTemperature<vendor::Probe<false>>);
// Component constraints propagate into the composed concept and its factories.
static_assert(NamedTemperatureSource<Sensor<true>>);
static_assert(!NamedTemperatureSource<Sensor<false>>);

// One generic function works for native objects, mappings, and runtime adapters.
double read(const TemperatureSource auto& source)
{
    return trait_call(TemperatureSource, readCelsius, source);
}

bool closeTo(double actual, double expected)
{
    return std::abs(actual - expected) < 1e-9;
}

int main()
{
    Sensor<true> sensor{21.0};
    vendor::Probe<true> probe{293.15};
    if (!closeTo(read(sensor), 21.0) || !closeTo(read(probe), 20.0)) {
        return 1;
    }

    auto borrowed = borrow(TemperatureSource, probe);
    auto owned = adapt(TemperatureSource, probe);
    auto boxed = box(TemperatureSource, probe);
    // The adapter need not repeat the concrete source's calibration metadata.
    static_assert(!Calibrated<decltype(borrowed)>);
    static_assert(TemperatureSource<decltype(borrowed)>);
    probe.kelvin = 303.15;
    if (!closeTo(read(borrowed), 30.0) || !closeTo(read(owned), 20.0) ||
        !closeTo(read(*boxed), 20.0)) {
        return 2;
    }

    auto capabilities = borrow(NamedTemperatureSource, sensor);
    const auto& erased = capabilities.as<dyn(TemperatureSource)>();
    if (!closeTo(read(erased), 21.0) ||
        capabilities.as<dyn(Named)>().name() != "native sensor") {
        return 3;
    }

    std::cout << "Native sensor: " << read(sensor) << " C\n"
              << "Borrowed probe: " << read(borrowed) << " C\n"
              << "Owned snapshot: " << read(owned) << " C\n"
              << "Uncalibrated sources are rejected at compile time.\n";
    return 0;
}
