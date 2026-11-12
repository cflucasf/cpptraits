#include "fixtures/rotatables.h"

#include <cmath>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

// Count only allocations made during the explicitly scoped box check below.
namespace allocation_probe {
inline bool enabled = false;
inline std::size_t count = 0;
}

void* operator new(std::size_t size)
{
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        if (allocation_probe::enabled) {
            ++allocation_probe::count;
        }
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

trait(AdjustableRpm, method(void, setRpm, (param(float, value)), ));
composed_trait(AdjustablePowered, T, Rotatable, Powerable, AdjustableRpm);

struct ExtensibleMotor : implements(Rotatable), implements(Powerable) {
    Motor motor;

    ExtensibleMotor(float rpm, float torque, std::string model)
        : motor(rpm, torque, std::move(model)) {}

    float getRpm() const override { return motor.getRpm(); }
    float getPower() const override { return motor.getPower(); }
};

struct AdjustableMotor : ExtensibleMotor, implements(AdjustableRpm) {
    using ExtensibleMotor::ExtensibleMotor;
    void setRpm(float value) override { motor.rpm = value; }
};

trait(TagOne, method(int, tag, const));
trait(TagTwo, method(int, tag, const));
trait(TagThree, method(int, tag, const));
trait(TagFour, method(int, tag, const));
trait(TagFive, method(int, tag, const));
trait(TagSix, method(int, tag, const));

composed_trait(OneComponent, T, Rotatable);
composed_trait(ThreeComponents, T, Rotatable, Powerable, TagOne);
composed_trait(FourComponents, T, Rotatable, Powerable, TagOne, TagTwo);
composed_trait(FiveComponents, T, Rotatable, Powerable, TagOne, TagTwo, TagThree);
composed_trait(SixComponents, T, Rotatable, Powerable, TagOne, TagTwo, TagThree, TagFour);
composed_trait(SevenComponents, T, Rotatable, Powerable, TagOne, TagTwo, TagThree, TagFour, TagFive);
composed_trait(EightComponents, T, Rotatable, Powerable, TagOne, TagTwo, TagThree, TagFour, TagFive, TagSix);

// Compile this translation unit separately with this definition to verify the
// declaration is rejected; the ordinary executable must remain well formed.
#if defined(CPPTRAITS_TEST_DUPLICATE_COMPONENT)
composed_trait(DuplicateComponents, T, Rotatable, Rotatable);
#endif

struct TaggedMotor : ExtensibleMotor, implements(TagOne), implements(TagTwo),
                     implements(TagThree), implements(TagFour),
                     implements(TagFive), implements(TagSix) {
    using ExtensibleMotor::ExtensibleMotor;
    int tag() const override { return 73; }
};

// Different implementations of the same signature must remain selectable.
struct FirstTag : implements(TagOne) {
    int tag() const override { return 11; }
};
struct SecondTag : implements(TagTwo) {
    int tag() const override { return 22; }
};
struct CollidingTags : FirstTag, SecondTag {
    // Keep concrete member lookup unambiguous for the existing signature checks.
    using FirstTag::tag;
};
composed_trait(TagPair, T, TagOne, TagTwo);

namespace namespaced {
trait(Local, method(int, localValue, const));
composed_trait(LocalRotatable, T, Local, Rotatable);

struct Rotor : implements(Local), implements(Rotatable) {
    int localValue() const override { return 19; }
    float getRpm() const override { return 190; }
};

inline bool check()
{
    Rotor rotor;
    auto view = borrow(LocalRotatable, rotor);
    auto owned = adapt(LocalRotatable, Rotor{});
    auto boxed = box(LocalRotatable, Rotor{});
    static_assert(LocalRotatable<Rotor>);
    static_assert(!LocalRotatable<dyn(LocalRotatable)>);
    return &view.as<dyn(Local)>() == static_cast<dyn(Local)*>(&rotor) &&
           view.as<dyn(Rotatable)>().getRpm() == 190 &&
           owned.as<dyn(Local)>().localValue() == 19 &&
           boxed->as<dyn(Rotatable)>().getRpm() == 190;
}
}

struct PowerOnly : implements(Powerable) {
    float getPower() const override { return 10; }
};

struct StructuralMotor {
    float getRpm() const { return 100; }
    float getPower() const { return 20; }
};

struct ExternalMotor { float rpm = 100; };
impl(Rotatable, ExternalMotor) {
    static float getRpm(const ExternalMotor& motor) { return motor.rpm; }
};
impl(Powerable, ExternalMotor) {
    static float getPower(const ExternalMotor& motor) { return motor.rpm * 2; }
};

struct MoveOnlyMotor : implements(Rotatable), implements(Powerable) {
    std::unique_ptr<int> rpm;
    explicit MoveOnlyMotor(int value) : rpm(std::make_unique<int>(value)) {}
    MoveOnlyMotor(MoveOnlyMotor&&) = default;
    MoveOnlyMotor& operator=(MoveOnlyMotor&&) = default;
    float getRpm() const override { return static_cast<float>(*rpm); }
    float getPower() const override { return getRpm() * 2; }
};

struct LifetimeToken {
    int& destructions;
    explicit LifetimeToken(int& destructions) : destructions(destructions) {}
    ~LifetimeToken() { ++destructions; }
};

struct TrackedMotor : implements(Rotatable), implements(Powerable) {
    std::unique_ptr<LifetimeToken> token;
    explicit TrackedMotor(int& destructions)
        : token(std::make_unique<LifetimeToken>(destructions)) {}
    TrackedMotor(TrackedMotor&&) = default;
    TrackedMotor& operator=(TrackedMotor&&) = default;
    float getRpm() const override { return 321; }
    float getPower() const override { return 654; }
};

template<typename T>
concept CanBorrow = requires(T&& object) {
    borrow(PoweredRotatable, std::forward<T>(object));
};
template<typename T>
concept CanAdapt = requires(T&& object) {
    adapt(PoweredRotatable, std::forward<T>(object));
};
template<typename T>
concept CanBox = requires(T&& object) {
    box(PoweredRotatable, std::forward<T>(object));
};
template<typename Bundle, typename Interface>
concept CanAs = requires(Bundle&& bundle) {
    std::forward<Bundle>(bundle).template as<Interface>();
};
template<typename Bundle>
concept CanSetRpm = requires(Bundle&& bundle) {
    std::forward<Bundle>(bundle).template as<dyn(AdjustableRpm)>().setRpm(1);
};

using View = dyn(PoweredRotatable);
using ConstView = decltype(borrow(PoweredRotatable, std::declval<const Motor&>()));
using Owned = decltype(adapt(PoweredRotatable, std::declval<Motor>()));
using MoveOwned = decltype(adapt(PoweredRotatable, std::declval<MoveOnlyMotor>()));
using MutableView = decltype(borrow(AdjustablePowered, std::declval<AdjustableMotor&>()));
using ConstMutableView = decltype(borrow(AdjustablePowered, std::declval<const AdjustableMotor&>()));
using MutableOwned = decltype(adapt(AdjustablePowered, std::declval<AdjustableMotor>()));


static_assert(!std::same_as<View, ConstView>);
static_assert(std::same_as<decltype(borrow(PoweredRotatable, std::declval<Motor&>())), View>);
static_assert(std::same_as<decltype(box(PoweredRotatable, std::declval<Motor>())),
                           std::unique_ptr<View>>);
static_assert(std::has_virtual_destructor_v<View>);
static_assert(std::is_copy_constructible_v<View> && std::is_copy_assignable_v<View>);
static_assert(std::is_copy_constructible_v<ConstView> && std::is_copy_assignable_v<ConstView>);
static_assert(std::is_copy_constructible_v<Owned> && std::is_copy_assignable_v<Owned>);
static_assert(std::is_move_constructible_v<Owned> && std::is_move_assignable_v<Owned>);
static_assert(std::is_move_constructible_v<MoveOwned> && std::is_move_assignable_v<MoveOwned>);
static_assert(!std::is_copy_constructible_v<MoveOwned>);
static_assert(!std::is_copy_assignable_v<MoveOwned>);
static_assert(!std::same_as<Owned, View>);

static_assert(PoweredRotatable<Motor>);
static_assert(PoweredRotatable<MoveOnlyMotor>);
static_assert(!PoweredRotatable<Spindle> && !PoweredRotatable<PowerOnly>);
static_assert(!PoweredRotatable<StructuralMotor> && !PoweredRotatable<int>);
template<typename T>
concept CanBorrowComponents = requires(T& value) {
    borrow(Rotatable, value);
    borrow(Powerable, value);
};
static_assert(CanBorrowComponents<ExternalMotor>);
static_assert(Rotatable<ExternalMotor> && Powerable<ExternalMotor>);
static_assert(PoweredRotatable<ExternalMotor>);
static_assert(PoweredRotatable<const ExternalMotor>);
static_assert(!std::convertible_to<ExternalMotor&, dyn(Rotatable)&>);
static_assert(!std::convertible_to<ExternalMotor&, dyn(Powerable)&>);
static_assert(!PoweredRotatable<View> && !PoweredRotatable<ConstView>);
static_assert(!PoweredRotatable<Owned> && !PoweredRotatable<MoveOwned>);
static_assert(!Rotatable<View> && !Powerable<View>);
static_assert(!std::convertible_to<Motor&, View&>);

static_assert(CanBorrow<Motor&> && CanBorrow<const Motor&>);
static_assert(!CanBorrow<Motor> && !CanBorrow<const Motor>);
static_assert(CanAdapt<Motor&> && CanAdapt<const Motor&> && CanAdapt<Motor>);
static_assert(CanBox<Motor&> && CanBox<const Motor&> && CanBox<Motor>);
static_assert(CanBorrow<MoveOnlyMotor&> && CanBorrow<const MoveOnlyMotor&>);
static_assert(!CanBorrow<MoveOnlyMotor> && !CanBorrow<const MoveOnlyMotor>);
static_assert(CanAdapt<MoveOnlyMotor> && CanBox<MoveOnlyMotor>);
static_assert(!CanAdapt<MoveOnlyMotor&> && !CanBox<MoveOnlyMotor&>);
static_assert(!CanAdapt<const MoveOnlyMotor&> && !CanBox<const MoveOnlyMotor&>);
static_assert(!CanAdapt<const MoveOnlyMotor> && !CanBox<const MoveOnlyMotor>);

template<typename T>
constexpr bool rejectedByFactories = !CanBorrow<T&> && !CanBorrow<const T&> &&
    !CanAdapt<T&> && !CanAdapt<const T&> && !CanAdapt<T> &&
    !CanBox<T&> && !CanBox<const T&> && !CanBox<T>;
static_assert(rejectedByFactories<Spindle>);
static_assert(rejectedByFactories<PowerOnly>);
static_assert(rejectedByFactories<StructuralMotor>);
static_assert(rejectedByFactories<ExternalMotor>);
static_assert(rejectedByFactories<int>);
static_assert(rejectedByFactories<View>);

template<typename Bundle>
constexpr bool selectionContract =
    std::same_as<decltype(std::declval<Bundle&>().template as<dyn(Rotatable)>()), dyn(Rotatable)&> &&
    std::same_as<decltype(std::declval<Bundle&>().template as<dyn(Powerable)>()), dyn(Powerable)&> &&
    std::same_as<decltype(std::declval<const Bundle&>().template as<dyn(Rotatable)>()), const dyn(Rotatable)&> &&
    std::same_as<decltype(std::declval<const Bundle&>().template as<dyn(Powerable)>()), const dyn(Powerable)&> &&
    !CanAs<Bundle&, dyn(TagOne)> && !CanAs<const Bundle&, dyn(TagOne)> &&
    !CanAs<Bundle&, Motor> && !CanAs<Bundle&, int> &&
    !CanAs<Bundle, dyn(Rotatable)> && !CanAs<const Bundle, dyn(Rotatable)> &&
    !CanAs<Bundle, dyn(Powerable)> && !CanAs<const Bundle, dyn(Powerable)>;
static_assert(selectionContract<View>);
static_assert(selectionContract<Owned>);
static_assert(selectionContract<MoveOwned>);
static_assert(std::same_as<decltype(std::declval<ConstView&>().as<dyn(Rotatable)>()), const dyn(Rotatable)&>);
static_assert(std::same_as<decltype(std::declval<ConstView&>().as<dyn(Powerable)>()), const dyn(Powerable)&>);
static_assert(std::same_as<decltype(std::declval<const ConstView&>().as<dyn(Rotatable)>()), const dyn(Rotatable)&>);
static_assert(std::same_as<decltype(std::declval<const ConstView&>().as<dyn(Powerable)>()), const dyn(Powerable)&>);
static_assert(!CanAs<ConstView, dyn(Rotatable)> && !CanAs<const ConstView, dyn(Rotatable)>);
static_assert(!CanAs<ConstView, dyn(Powerable)> && !CanAs<const ConstView, dyn(Powerable)>);
static_assert(!CanAs<ConstView&, dyn(TagOne)> && !CanAs<const ConstView&, dyn(TagOne)>);
static_assert(CanSetRpm<MutableView&> && CanSetRpm<MutableOwned&>);
static_assert(!CanSetRpm<const MutableView&> && !CanSetRpm<const MutableOwned&>);
static_assert(!CanSetRpm<ConstMutableView&> && !CanSetRpm<const ConstMutableView&>);
static_assert(std::same_as<decltype(std::declval<ConstMutableView&>().as<dyn(AdjustableRpm)>()),
                           const dyn(AdjustableRpm)&>);
static_assert(std::same_as<decltype(std::declval<const MutableView&>().as<dyn(AdjustableRpm)>()),
                           const dyn(AdjustableRpm)&>);
static_assert(!CanAs<MutableView, dyn(AdjustableRpm)>);
static_assert(!CanAs<MutableOwned, dyn(AdjustableRpm)>);

static_assert(OneComponent<Motor>);
static_assert(ThreeComponents<TaggedMotor> && FourComponents<TaggedMotor>);
static_assert(FiveComponents<TaggedMotor> && SixComponents<TaggedMotor>);
static_assert(SevenComponents<TaggedMotor> && EightComponents<TaggedMotor>);
static_assert(!ThreeComponents<Motor> && !EightComponents<Motor>);
static_assert(TagPair<CollidingTags>);

namespace {
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "composed_traits_test: %s\n", message);
        ++failures;
    }
}

bool near(float left, float right)
{
    return std::abs(left - right) <= 0.001f;
}

template<typename Bundle>
const void* completeObject(const Bundle& bundle)
{
    const auto& rotating = bundle.template as<dyn(Rotatable)>();
    const auto& powered = bundle.template as<dyn(Powerable)>();
    const void* object = dynamic_cast<const void*>(&rotating);
    check(object == dynamic_cast<const void*>(&powered),
          "selected interfaces must belong to the same complete object");
    return object;
}

void borrowedViews()
{
    Motor motor{1200, 3, "borrowed"};
    auto view = borrow(PoweredRotatable, motor);
    check(&view.as<dyn(Rotatable)>() == static_cast<dyn(Rotatable)*>(&motor),
          "borrow must preserve Rotatable subobject identity");
    check(&view.as<dyn(Powerable)>() == static_cast<dyn(Powerable)*>(&motor),
          "borrow must preserve Powerable subobject identity");
    check(completeObject(view) == &motor, "borrow must not own a copy");

    auto copied = view;
    auto moved = std::move(copied);
    Motor other{10, 1, "other"};
    auto assigned = borrow(PoweredRotatable, other);
    assigned = view;
    auto moveAssigned = borrow(PoweredRotatable, other);
    moveAssigned = std::move(assigned);
    motor.rpm = 2400;
    motor.torque = 4;
    for (const View* bundle : {&view, &moved, &moveAssigned}) {
        check(completeObject(*bundle) == &motor, "view copies and moves retain the original identity");
        check(bundle->as<dyn(Rotatable)>().getRpm() == 2400, "view observes source mutation");
        check(near(bundle->as<dyn(Powerable)>().getPower(), motor.getPower()),
              "both components observe source mutation");
    }

    const Motor stopped{0, 4, "const source"};
    auto constBorrow = borrow(PoweredRotatable, stopped);
    auto constCopy = constBorrow;
    auto constAssigned = borrow(PoweredRotatable, std::as_const(other));
    constAssigned = constCopy;
    check(completeObject(constAssigned) == &stopped, "const view assignment retains identity");
    check(constBorrow.as<dyn(Rotatable)>().getRpm() == 0, "const source is readable");
    check(&constBorrow.as<dyn(Powerable)>() == static_cast<const dyn(Powerable)*>(&stopped),
          "const borrow selects the original const subobject");

    int destructions = 0;
    {
        TrackedMotor source{destructions};
        {
            auto borrowed = borrow(PoweredRotatable, source);
            auto another = borrowed;
            check(completeObject(another) == &source, "tracked borrow retains identity");
        }
        check(destructions == 0, "destroying borrowed views must not destroy the source");
    }
    check(destructions == 1, "borrowed source is destroyed exactly once by its owner");
}

void owningRebinding()
{
    Motor source{100, 2, "source"};
    auto original = adapt(PoweredRotatable, source);
    auto copied = original;
    check(completeObject(original) != &source, "adapt of an lvalue owns a copy");
    check(completeObject(copied) != completeObject(original), "owning copy rebinds both pointers");
    source.rpm = 999;
    check(original.as<dyn(Rotatable)>().getRpm() == 100, "adapt is independent of source mutation");

    const void* oldAddress = completeObject(original);
    auto moved = std::move(original);
    check(completeObject(moved) != oldAddress, "owning move rebinds to inline destination storage");
    check(moved.as<dyn(Rotatable)>().getRpm() == 100, "owning move preserves value");
    check(completeObject(moved) != completeObject(copied), "moved and copied owners are distinct");

    auto copyAssigned = adapt(PoweredRotatable, Motor{1, 1, "copy target"});
    const void* copyTarget = completeObject(copyAssigned);
    copyAssigned = copied;
    check(completeObject(copyAssigned) == copyTarget, "copy assignment retains destination inline object");
    check(completeObject(copyAssigned) != completeObject(copied), "copy assignment must not borrow source storage");
    check(copyAssigned.as<dyn(Rotatable)>().getRpm() == 100, "copy assignment preserves value");
    auto moveAssigned = adapt(PoweredRotatable, Motor{2, 1, "move target"});
    const void* moveTarget = completeObject(moveAssigned);
    moveAssigned = std::move(moved);
    check(completeObject(moveAssigned) == moveTarget, "move assignment retains destination inline object");
    check(moveAssigned.as<dyn(Rotatable)>().getRpm() == 100, "move assignment preserves value");

    std::optional<Owned> copySurvivor;
    std::optional<Owned> moveSurvivor;
    {
        auto temporary = adapt(PoweredRotatable, Motor{700, 5, "temporary"});
        copySurvivor.emplace(temporary);
        moveSurvivor.emplace(std::move(temporary));
    }
    check(copySurvivor->as<dyn(Rotatable)>().getRpm() == 700,
          "copy remains valid after original owner destruction");
    check(moveSurvivor->as<dyn(Rotatable)>().getRpm() == 700,
          "move remains valid after original owner destruction");
    check(near(copySurvivor->as<dyn(Powerable)>().getPower(),
               moveSurvivor->as<dyn(Powerable)>().getPower()),
          "both surviving components remain bound to live objects");
    {
        auto temporary = adapt(PoweredRotatable, Motor{800, 6, "assignment source"});
        copyAssigned = temporary;
        moveAssigned = std::move(temporary);
    }
    check(copyAssigned.as<dyn(Rotatable)>().getRpm() == 800 &&
          moveAssigned.as<dyn(Rotatable)>().getRpm() == 800,
          "assigned owners survive destruction of their source");
    check(near(copyAssigned.as<dyn(Powerable)>().getPower(),
               moveAssigned.as<dyn(Powerable)>().getPower()),
          "assignment rebinds both components, not just the first");

    const Motor constant{400, 2, "const lvalue"};
    auto fromConst = adapt(PoweredRotatable, constant);
    check(completeObject(fromConst) != &constant &&
          fromConst.as<dyn(Rotatable)>().getRpm() == 400,
          "const lvalue adapt owns a separate copy");
}

void baseAssignmentPreservesOwnership()
{
    for (bool moveAssignment : {false, true}) {
        Motor inlineValue{100, 2, "inline owner"};
        Motor boxedValue{200, 3, "boxed owner"};
        auto owned = adapt(PoweredRotatable, inlineValue);
        auto boxed = box(PoweredRotatable, boxedValue);
        View& inlineBase = owned;
        const void* inlineIdentity = completeObject(owned);
        const void* boxedIdentity = completeObject(*boxed);

        auto checkBindings = [&] {
            check(completeObject(owned) == inlineIdentity,
                  "base assignment preserves inline owner's component identity");
            check(completeObject(*boxed) == boxedIdentity,
                  "base assignment preserves boxed owner's component identity");
        };
        auto checkOriginalValues = [&] {
            check(owned.as<dyn(Rotatable)>().getRpm() == inlineValue.getRpm() &&
                  near(owned.as<dyn(Powerable)>().getPower(), inlineValue.getPower()),
                  moveAssignment ? "base move assignment preserves inline owned values"
                                 : "base copy assignment preserves inline owned values");
            check(boxed->as<dyn(Rotatable)>().getRpm() == boxedValue.getRpm() &&
                  near(boxed->as<dyn(Powerable)>().getPower(), boxedValue.getPower()),
                  moveAssignment ? "base move assignment preserves boxed owned values"
                                 : "base copy assignment preserves boxed owned values");
        };

        int destructions = 0;
        {
            TrackedMotor source{destructions};
            auto inlineSource = borrow(PoweredRotatable, source);
            auto boxedSource = borrow(PoweredRotatable, source);
            if (moveAssignment) {
                inlineBase = std::move(inlineSource);
                *boxed = std::move(boxedSource);
            } else {
                inlineBase = inlineSource;
                *boxed = boxedSource;
            }
            checkBindings();
            checkOriginalValues();
        }
        check(destructions == 1, "base assignment's borrowed source has been destroyed");
        checkBindings();
        checkOriginalValues();

        // box stores the same concrete owning bundle type returned by adapt.
        auto& boxedConcrete = dynamic_cast<Owned&>(*boxed);
        Motor replacementValue{700, 5, "replacement"};
        {
            auto replacement = adapt(PoweredRotatable, replacementValue);
            owned = replacement;
            boxedConcrete = replacement;
        }
        checkBindings();
        check(owned.as<dyn(Rotatable)>().getRpm() == 700 &&
              boxed->as<dyn(Rotatable)>().getRpm() == 700 &&
              near(owned.as<dyn(Powerable)>().getPower(), replacementValue.getPower()) &&
              near(boxed->as<dyn(Powerable)>().getPower(), replacementValue.getPower()),
              "concrete copy assignment still works after base assignment and source destruction");

        replacementValue.rpm = 800;
        {
            auto inlineReplacement = adapt(PoweredRotatable, replacementValue);
            auto boxedReplacement = adapt(PoweredRotatable, replacementValue);
            owned = std::move(inlineReplacement);
            boxedConcrete = std::move(boxedReplacement);
        }
        checkBindings();
        check(owned.as<dyn(Rotatable)>().getRpm() == 800 &&
              boxed->as<dyn(Rotatable)>().getRpm() == 800 &&
              near(owned.as<dyn(Powerable)>().getPower(), replacementValue.getPower()) &&
              near(boxed->as<dyn(Powerable)>().getPower(), replacementValue.getPower()),
              "concrete move assignment still works after base assignment and source destruction");
    }
}

void moveOnlyOwnership()
{
    MoveOnlyMotor source{900};
    auto first = adapt(PoweredRotatable, std::move(source));
    check(!source.rpm, "adapt moves an rvalue implementation");
    const void* oldAddress = completeObject(first);
    auto second = std::move(first);
    check(completeObject(second) != oldAddress, "move-only owner move rebinds inline storage");
    auto assigned = adapt(PoweredRotatable, MoveOnlyMotor{1});
    {
        auto temporary = std::move(second);
        assigned = std::move(temporary);
    }
    check(assigned.as<dyn(Rotatable)>().getRpm() == 900 &&
          assigned.as<dyn(Powerable)>().getPower() == 1800,
          "move-only owner survives move assignment and source destruction");
    completeObject(assigned);

    MoveOnlyMotor boxSource{1200};
    auto boxed = box(PoweredRotatable, std::move(boxSource));
    check(!boxSource.rpm && boxed->as<dyn(Rotatable)>().getRpm() == 1200 &&
          boxed->as<dyn(Powerable)>().getPower() == 2400,
          "box owns a moved move-only implementation");
    check(completeObject(*boxed) != &boxSource, "boxed implementation is not borrowed");
}

void boxedOwnership()
{
    int destructions = 0;
    {
        TrackedMotor source{destructions};
        // Keep the optimizer from eliding the allocation being measured.
        using Factory = std::unique_ptr<View> (*)(TrackedMotor&&);
        Factory volatile factory = +[](TrackedMotor&& value) -> std::unique_ptr<View> {
                    return box(PoweredRotatable, std::move(value));
                };
        allocation_probe::count = 0;
        allocation_probe::enabled = true;
        std::unique_ptr<View> boxed = factory(std::move(source));
        allocation_probe::enabled = false;
        check(allocation_probe::count == 1, "box allocates the bundle and implementation together once");
        check(!source.token && destructions == 0, "box takes ownership without premature destruction");
        check(boxed->as<dyn(Rotatable)>().getRpm() == 321 &&
              boxed->as<dyn(Powerable)>().getPower() == 654, "boxed interfaces dispatch correctly");
        check(completeObject(*boxed) != &source, "box contains a distinct concrete implementation");
        boxed.reset();
        check(destructions == 1, "deletion through dyn bundle destroys its concrete owned object");
    }
    check(destructions == 1, "moved-from source does not destroy the owned resource twice");

    Motor source{600, 3, "boxed copy"};
    auto copied = box(PoweredRotatable, source);
    auto constCopied = box(PoweredRotatable, std::as_const(source));
    source.rpm = 0;
    check(copied->as<dyn(Rotatable)>().getRpm() == 600 &&
          constCopied->as<dyn(Rotatable)>().getRpm() == 600,
          "box copies mutable and const lvalues");
    check(completeObject(*copied) != &source && completeObject(*constCopied) != &source,
          "boxed lvalue copies have independent identity");
    const View& constBox = *copied;
    check(near(constBox.as<dyn(Powerable)>().getPower(),
               constCopied->as<dyn(Powerable)>().getPower()), "const boxed view dispatches");
}

void mutableSelection()
{
    AdjustableMotor motor{100, 2, "adjustable"};
    auto borrowed = borrow(AdjustablePowered, motor);
    borrowed.as<dyn(AdjustableRpm)>().setRpm(500);
    check(motor.motor.rpm == 500 && borrowed.as<dyn(Rotatable)>().getRpm() == 500,
          "selected mutable interface changes the original object");
    check(near(borrowed.as<dyn(Powerable)>().getPower(), motor.getPower()),
          "mutation is visible through the other selected interface");
    auto constant = borrow(AdjustablePowered, std::as_const(motor));
    check(&constant.as<dyn(AdjustableRpm)>() == static_cast<const dyn(AdjustableRpm)*>(&motor),
          "const borrow accepts a mutable component but exposes its const interface");

    auto owned = adapt(AdjustablePowered, motor);
    auto copied = owned;
    owned.as<dyn(AdjustableRpm)>().setRpm(700);
    copied.as<dyn(AdjustableRpm)>().setRpm(800);
    check(motor.motor.rpm == 500 && owned.as<dyn(Rotatable)>().getRpm() == 700 &&
          copied.as<dyn(Rotatable)>().getRpm() == 800,
          "owning copies mutate independently through selected interfaces");
    auto boxed = box(AdjustablePowered, motor);
    boxed->as<dyn(AdjustableRpm)>().setRpm(900);
    check(motor.motor.rpm == 500 && boxed->as<dyn(Rotatable)>().getRpm() == 900,
          "boxed mutable selection changes only the owned implementation");
}

void flatComposition()
{
    TaggedMotor motor{300, 2, "tagged"};
    auto one = borrow(OneComponent, motor);
    auto three = borrow(ThreeComponents, motor);
    auto four = borrow(FourComponents, motor);
    auto five = borrow(FiveComponents, motor);
    auto six = borrow(SixComponents, motor);
    auto seven = borrow(SevenComponents, motor);
    auto eight = borrow(EightComponents, motor);
    check(one.as<dyn(Rotatable)>().getRpm() == 300, "one-component composition");
    check(completeObject(three) == &motor && three.as<dyn(TagOne)>().tag() == 73,
          "three-component composition preserves identity and dispatch");
    check(four.as<dyn(TagTwo)>().tag() == 73 && five.as<dyn(TagThree)>().tag() == 73 &&
          six.as<dyn(TagFour)>().tag() == 73 && seven.as<dyn(TagFive)>().tag() == 73,
          "four through seven components select their last interface");
    check(&eight.as<dyn(TagSix)>() == static_cast<dyn(TagSix)*>(&motor),
          "eight-component composition retains last interface identity");
    auto owned = adapt(EightComponents, motor);
    auto boxed = box(EightComponents, motor);
    check(owned.as<dyn(TagSix)>().tag() == 73 && boxed->as<dyn(TagSix)>().tag() == 73,
          "owning factories support eight components");

    CollidingTags tags;
    auto collision = borrow(TagPair, tags);
    auto ownedCollision = adapt(TagPair, tags);
    auto boxedCollision = box(TagPair, CollidingTags{});
    check(collision.as<dyn(TagOne)>().tag() == 11 && collision.as<dyn(TagTwo)>().tag() == 22 &&
          ownedCollision.as<dyn(TagOne)>().tag() == 11 && ownedCollision.as<dyn(TagTwo)>().tag() == 22 &&
          boxedCollision->as<dyn(TagOne)>().tag() == 11 && boxedCollision->as<dyn(TagTwo)>().tag() == 22,
          "colliding method names are resolved by selecting an interface");
    check(namespaced::check(), "namespaced trait and composition declarations");
}
}

int main()
{
    borrowedViews();
    owningRebinding();
    baseAssignmentPreservesOwnership();
    moveOnlyOwnership();
    boxedOwnership();
    mutableSelection();
    flatComposition();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
