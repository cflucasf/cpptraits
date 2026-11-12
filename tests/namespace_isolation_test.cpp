#include "traits.hpp"

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

struct returns {};
struct integral {};
static_assert(std::is_class_v<returns> && std::is_class_v<integral>);

namespace app {
// These are user-owned names, not customization points or generated API.
struct TRotatable {};
struct RotatableImpl {};
struct RotatableAdapter {};
struct RotatableRef {};
struct HasRotatableImpl {};
struct CppTraitsRotatableImplCheck {};
inline int makeRotatable(int) { return -1; }
inline int borrowRotatable(int&) { return -2; }
inline int boxRotatable(int) { return -3; }

struct interface_type { float value; };
struct const_interface_type {};
template<typename T> struct implementation { T value; };
template<typename> struct implementation_check {};
template<typename> struct has_implementation {};
template<typename> struct owning_adapter {};
template<typename> struct reference_adapter {};
inline int make_value(int) { return -4; }
inline int make_reference(int&) { return -5; }
inline int make_box(int) { return -6; }

trait(Rotatable, method(float, getRpm, const));
trait(Powerable, method(float, getPower, const));

struct ExternalRotor { float rpm; };

// Keep these types unqualified so generated scopes cannot silently shadow them.
trait(ObjectReader,
    method(interface_type, readObject, (param(implementation<int>, offset)), const),
    method(implementation<int>, readOffset, (param(interface_type, object)), const)
);

impl(ObjectReader, ExternalRotor) {
    static interface_type readObject(const ExternalRotor& rotor,
                                     implementation<int> offset) {
        return {rotor.rpm + offset.value};
    }
    static implementation<int> readOffset(const ExternalRotor& rotor,
                                          interface_type object) {
        return {static_cast<int>(object.value - rotor.rpm)};
    }
};

namespace nested {
struct TLocal {};
struct TPoweredRotatable {};
struct ConstTPoweredRotatable {};
struct PoweredRotatableAdapter {};
inline int makePoweredRotatable(int) { return -7; }
inline int borrowPoweredRotatable(int&) { return -8; }
inline int boxPoweredRotatable(int) { return -9; }

trait(Local, method(int, localValue, const));
// Unqualified components must still resolve in the enclosing namespace.
composed_trait(PoweredRotatable, T, Rotatable, Powerable, Local);

struct Motor : implements(app::Rotatable), implements(app::Powerable),
               implements(app::nested::Local) {
    float rpm = 100;
    float getRpm() const override { return rpm; }
    float getPower() const override { return rpm * 2; }
    int localValue() const override { return 19; }
};

struct ExternalRotor { float rpm; };
}

// Specialize a qualified trait from its enclosing namespace, not its internals.
impl(app::Rotatable, ExternalRotor) {
    static float getRpm(const ExternalRotor& rotor) { return rotor.rpm; }
};
}

namespace other {
trait(Rotatable, method(float, getRpm, const));
struct ExternalRotor { float rpm; };
struct Rotor : implements(other::Rotatable) {
    float getRpm() const override { return 700; }
};
}

// The global namespace encloses both qualified customization points.
impl(other::Rotatable, other::ExternalRotor) {
    static float getRpm(const other::ExternalRotor& rotor) { return rotor.rpm * 3; }
};
impl(app::Rotatable, app::nested::ExternalRotor) {
    static float getRpm(const app::nested::ExternalRotor& rotor) { return rotor.rpm * 2; }
};

struct QualifiedRotor : implements(app::Rotatable) {
    float getRpm() const override { return 500; }
};

template<typename T>
concept CanBorrowApp = requires(T& value) { borrow(app::Rotatable, value); };
template<typename T>
concept CanBorrowOther = requires(T& value) { borrow(other::Rotatable, value); };
template<typename T>
concept CanAdaptApp = requires(T&& value) {
    adapt(app::Rotatable, std::forward<T>(value));
};
template<typename T>
concept CanBorrowBundle = requires(T& value) {
    borrow(app::nested::PoweredRotatable, value);
};

using AppOwned = decltype(adapt(app::Rotatable, std::declval<app::ExternalRotor>()));
using AppBorrow = decltype(borrow(app::Rotatable, std::declval<app::ExternalRotor&>()));
using ConstAppBorrow = decltype(borrow(app::Rotatable,
                                     std::declval<const app::ExternalRotor&>()));
using ConstBundle = decltype(borrow(app::nested::PoweredRotatable,
                                   std::declval<const app::nested::Motor&>()));
using ObjectReaderOwned = decltype(adapt(app::ObjectReader,
                                       std::declval<app::ExternalRotor>()));
using ObjectReaderBorrow = decltype(borrow(app::ObjectReader,
                                         std::declval<app::ExternalRotor&>()));
static_assert(app::ObjectReader<ObjectReaderOwned> && app::ObjectReader<ObjectReaderBorrow>);
static_assert(std::same_as<decltype(std::declval<const dyn(app::ObjectReader)&>()
                                       .readObject(std::declval<app::implementation<int>>())),
                           app::interface_type>);
static_assert(std::same_as<decltype(std::declval<const dyn(app::ObjectReader)&>()
                                       .readOffset(std::declval<app::interface_type>())),
                           app::implementation<int>>);

static_assert(!std::same_as<dyn(app::Rotatable), dyn(other::Rotatable)>);
static_assert(!std::same_as<dyn(app::Rotatable), app::TRotatable>);
static_assert(!std::same_as<AppOwned, app::RotatableAdapter>);
static_assert(!std::same_as<AppBorrow, app::RotatableRef>);
static_assert(app::Rotatable<QualifiedRotor>);
static_assert(!other::Rotatable<QualifiedRotor>);
static_assert(other::Rotatable<other::Rotor>);
static_assert(!app::Rotatable<other::Rotor>);
static_assert(app::Rotatable<AppOwned> && app::Rotatable<AppBorrow>);
static_assert(app::Rotatable<ConstAppBorrow>);
static_assert(CanBorrowApp<app::ExternalRotor>);
static_assert(CanBorrowApp<app::nested::ExternalRotor>);
static_assert(!CanBorrowOther<app::ExternalRotor>);
static_assert(CanBorrowOther<other::ExternalRotor>);
static_assert(!CanBorrowApp<other::ExternalRotor>);
static_assert(!CanBorrowApp<QualifiedRotor> && !CanAdaptApp<QualifiedRotor>);
static_assert(app::nested::PoweredRotatable<app::nested::Motor>);
static_assert(!CanBorrowBundle<app::ExternalRotor>);
static_assert(std::same_as<decltype(std::declval<ConstBundle&>().as<dyn(app::Rotatable)>()),
                           const dyn(app::Rotatable)&>);
static_assert(std::same_as<decltype(box(app::Rotatable, std::declval<app::ExternalRotor>())),
                           std::unique_ptr<dyn(app::Rotatable)>>);

int main()
{
    app::ExternalRotor rotor{100};
    auto owned = adapt(app::Rotatable, rotor);
    auto borrowed = borrow(app::Rotatable, rotor);
    auto boxed = box(app::Rotatable, rotor);
    rotor.rpm = 200;
    const dyn(app::Rotatable)& interface = borrowed;
    if (owned.getRpm() != 100 || boxed->getRpm() != 100 || interface.getRpm() != 200) {
        return 1;
    }

    const app::ExternalRotor constant{300};
    auto constBorrow = borrow(app::Rotatable, constant);
    auto otherOwned = adapt(other::Rotatable, other::ExternalRotor{10});
    auto nestedBox = box(app::Rotatable, app::nested::ExternalRotor{20});
    QualifiedRotor direct;
    other::Rotor otherDirect;
    const dyn(app::Rotatable)& directInterface = direct;
    const dyn(other::Rotatable)& otherInterface = otherDirect;
    if (constBorrow.getRpm() != 300 || otherOwned.getRpm() != 30 ||
        nestedBox->getRpm() != 40 || directInterface.getRpm() != 500 ||
        otherInterface.getRpm() != 700) {
        return 2;
    }

    app::nested::Motor motor;
    auto view = borrow(app::nested::PoweredRotatable, motor);
    auto bundle = adapt(app::nested::PoweredRotatable, motor);
    auto bundleBox = box(app::nested::PoweredRotatable, motor);
    auto constView = borrow(app::nested::PoweredRotatable, std::as_const(motor));
    motor.rpm = 400;
    if (&view.as<dyn(app::Rotatable)>() != static_cast<dyn(app::Rotatable)*>(&motor) ||
        view.as<dyn(app::Powerable)>().getPower() != 800 ||
        constView.as<dyn(app::Rotatable)>().getRpm() != 400 ||
        bundle.as<dyn(app::Rotatable)>().getRpm() != 100 ||
        bundleBox->as<dyn(app::Powerable)>().getPower() != 200 ||
        view.as<dyn(app::nested::Local)>().localValue() != 19) {
        return 3;
    }

    int value = 0;
    if (app::makeRotatable(value) != -1 || app::borrowRotatable(value) != -2 ||
        app::boxRotatable(value) != -3 || app::make_value(value) != -4 ||
        app::make_reference(value) != -5 || app::make_box(value) != -6 ||
        app::nested::makePoweredRotatable(value) != -7 ||
        app::nested::borrowPoweredRotatable(value) != -8 ||
        app::nested::boxPoweredRotatable(value) != -9) {
        return 4;
    }

    app::ExternalRotor objectSource{50};
    auto objectBorrow = borrow(app::ObjectReader, objectSource);
    auto objectOwned = adapt(app::ObjectReader, objectSource);
    auto objectBox = box(app::ObjectReader, objectSource);
    auto constObjectBorrow = borrow(app::ObjectReader, std::as_const(objectSource));
    objectSource.rpm = 60;
    const dyn(app::ObjectReader)& objectInterface = objectBorrow;
    app::implementation<int> offset{3};
    app::interface_type object{70};
    if (objectInterface.readObject(offset).value != 63 ||
        constObjectBorrow.readObject(offset).value != 63 ||
        objectOwned.readObject(offset).value != 53 ||
        objectBox->readObject(offset).value != 53 ||
        objectInterface.readOffset(object).value != 10 ||
        constObjectBorrow.readOffset(object).value != 10 ||
        objectOwned.readOffset(object).value != 20 ||
        objectBox->readOffset(object).value != 20) {
        return 5;
    }
}
