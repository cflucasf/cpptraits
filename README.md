# CppTraits

**Rust-inspired traits for C++20, with explicit contracts and keyword-like macros.**
Define an operation once, check implementations at compile time, and use the same
contract for runtime polymorphism—without writing interface, concepts and adapter boilerplate.

CppTraits lets you:

- Require capabilities in generic functions using concepts such as `Rotatable`.
- Use runtime interfaces and heterogeneous collections through `dyn(Trait)`.
- Implement traits for third-party and built-in types without modifying them.
- Provide shared default methods, with native or external overrides.
- Combine traits as compile-time constraints or runtime bundles.
- Choose explicitly between borrowing, owning values, and boxed ownership.

This is an experimental, header-only library, not a complete Rust trait system.
The keyword spellings are deliberate global macros; no code generator or
nonstandard language extension is required.

## A first trait

```cpp
#include <iostream>
#include "traits.hpp"

trait(Rotatable,
    method(float, getRpm, const),
    default_method(bool, isStopped, (), const, {
        return getRpm() == 0.0f;
    })
);

struct Motor : implements(Rotatable) {
    float rpm = 1400;

    float getRpm() const override {
        return rpm;
    }
};

void printRpm(const Rotatable auto& object) {
    std::cout << object.getRpm() << '\n';
}

int main() {
    Motor motor;
    printRpm(motor);                    // 1400
    std::cout << motor.isStopped();      // false (printed as 0)
}
```

`implements(Rotatable)` opts into the contract through public interface inheritance.
Matching method names alone do not satisfy the concept: `Rotatable<T>` accepts
either the exact runtime interface checks or a valid explicit `impl(Rotatable, T)`.
Required native methods use ordinary C++ `override`; default methods are inherited
and can also be overridden. Concept constraints do not inject members: the
`printRpm` body works only when its argument exposes `getRpm()`. Use a runtime
adapter for a mapped type that does not. Calls through interface members retain
normal C++ virtual-dispatch semantics.

## Declaring operations

Use the short form for operations without parameters, and `param(Type, name)`
for parameterized operations:

```cpp
#include <memory>
#include <utility>

trait(Adjustable,
    method(float, getRpm, const),
    method(void, setRpm, (param(float, rpm)), ),
    method(void, reset, )
);

trait(Snapshots,
    method((std::pair<int, int>), snapshot, const),
    method(void, restore,
        (param((const std::pair<int, int>&), snapshot)), ),
    method(void, consume, (param(std::unique_ptr<int>, value)), )
);
```

- **Required operation:** `method(Return, name, qualifiers)` or
  `method(Return, name, (param(...), ...), qualifiers)`.
- **Concrete operation:** `concrete_method(name, merge)` or
  `concrete_method(name, clone)`; see the runtime examples below for their fixed
  signatures and mapping rules.
- **Provided operation:** `default_method(Return, name, (param(...), ...), qualifiers, { body })`.
  Always supply the parameter list; use `()` when it is empty.
- For ordinary methods/defaults, leave qualifiers empty for mutable methods, or
  use `const`, `noexcept`, or `const noexcept`.
- Wrap the **entire type** in parentheses when its spelling contains commas.
  Type aliases work too.
- Adapters forward parameters according to their declared types: values as
  rvalues, `T&` and `const T&` as lvalues, and `T&&` as rvalues.

Default bodies can call required operations or other defaults. These calls honor
virtual overrides, including overrides supplied by external implementations.

## Implementing without inheritance

Use `impl` for your own plain types, third-party types, or built-in types without
changing their layout or adding base classes. Using `Rotatable` from the first
example, map a third-party type:

```cpp
#include <cmath>
#include <numbers>

namespace vendor {
    struct Motor {
        double speed; // Radians per second; no trait inheritance.
    };
}

impl(Rotatable, vendor::Motor) {
    static float getRpm(const vendor::Motor& motor) {
        return static_cast<float>(
            motor.speed * 60.0 / (2.0 * std::numbers::pi));
    }

    // Optional: override the shared exact-zero default with a tolerance.
    static bool isStopped(const vendor::Motor& motor) {
        return std::abs(motor.speed) < 0.01;
    }
};
```

Static mapping functions take the external object first: `const T&` for const
operations, `T&` for mutable operations, followed by the declared parameters.
Supply every required operation with the declared parameter and return types,
receiver constness, and exception specification. A `noexcept` mapping can implement
a potentially throwing operation, but not the reverse.

Omit a default method's name to use its shared body, or provide a matching function
to override it. If several default overloads share that name, either omit the name
entirely or provide every declared default overload; partial overrides are rejected.

The external type stays unchanged and needs no inheritance. A valid explicit
mapping makes `Rotatable<vendor::Motor>` true, just as interface-derived wrappers
satisfy it. This adds no member functions or implicit conversion to `dyn(Rotatable)`:
use a runtime adapter to call `getRpm()` or `isStopped()` through the interface.
For example, `printRpm(borrow(Rotatable, source))` works, while `printRpm(source)`
does not because the vendor type has no `getRpm()` member.

To map a family of class template specializations, pass the template declaration
and target type as separate parenthesized arguments. For a vendor type
`Motor<Speed, Channels>` that exposes an `rpm` field, the mapping can cover every
positive channel count:

```cpp
impl(Rotatable,
     (template<class Speed, std::size_t Channels> requires (Channels > 0)),
     (vendor::Motor<Speed, Channels>)) {
    static float getRpm(const vendor::Motor<Speed, Channels>& motor) {
        return static_cast<float>(motor.rpm);
    }
};
```

Include the mapping before the first concept check or adapter call for a matching
type. A two-argument `impl(Rotatable, ConcreteType)` can specialize an individual
type and takes precedence over a matching partial specialization.

## Borrowing, owning, and runtime dispatch

```cpp
vendor::Motor source{2.0 * std::numbers::pi};

auto reference = borrow(Rotatable, source); // Borrows source.
auto owned = adapt(Rotatable, source);       // Owns a copy.
auto pointer = box(Rotatable, vendor::Motor{0}); // Owns through an interface pointer.

source.speed = 0;
reference.isStopped(); // true: observes source.
owned.isStopped();     // false: retains its copied state.

const dyn(Rotatable)& runtime = reference;
printRpm(runtime);

std::vector<std::unique_ptr<dyn(Rotatable)>> objects;
objects.push_back(box(Rotatable, vendor::Motor{0}));
objects.push_back(std::make_unique<Motor>());
```

Include `<vector>` and `<memory>` for the collection example.

| Keyword | Meaning |
| --- | --- |
| `dyn(Trait)` | Names the runtime interface, without specifying ownership |
| `borrow(Trait, object)` | Borrows an lvalue; the source must outlive the wrapper |
| `adapt(Trait, object)` | Owns an adapted value; copies lvalues and moves rvalues |
| `box(Trait, object)` | Owns an adapted value through `std::unique_ptr<dyn(Trait)>` |

Use `std::move(object)` to transfer a move-only value into `adapt` or `box`.
Borrowing temporaries is rejected. Ordinary trait factories use external `impl`
mappings; native implementations already work as interface references and can be
boxed with `std::make_unique`.

Runtime participation is explicit: interfaces and adapters use ordinary virtual
dispatch and carry polymorphic storage overhead. `adapt` and `borrow` do not
themselves allocate; `box` explicitly allocates an owning adapter. Stored values
and user operations may allocate independently. Compiler devirtualization is an
optimization, not a blanket zero-overhead runtime guarantee.

## Same-concrete-type runtime operations

`concrete_method(name, shape)` selects one of two fixed contracts. The name is
arbitrary; `merge` and `clone` select the shape, not the method name:

| Descriptor | Exact static mapping | Generated runtime member |
| --- | --- | --- |
| `concrete_method(name, merge)` | `static void name(T&, const T&)` | `void name(const dyn(Trait)&)` (mutable receiver) |
| `concrete_method(name, clone)` | `static T name(const T&)` | `std::unique_ptr<dyn(Trait)> name() const` |

Both shapes require explicit `impl` mappings; `implements(Trait)` is disallowed
for any trait containing either shape. A valid mapping makes the raw mapped type
satisfy the trait concept, but injects no members or implicit interface conversion.
Use an explicit runtime adapter for erased calls. Mapping functions are validated
by exact function-pointer signatures: a clone must return `T`, not `T&`,
`const T`, or a merely convertible type. Exceptions from user code propagate.

### Merging the same mapped type

Use the merge shape for a mutable receiver and one read-only operand:

```cpp
trait(Combinable, concrete_method(combine, merge));

struct Total {
    int amount;
    void combine(const Total& other) { amount += other.amount; }
};

impl(Combinable, Total) {
    static void combine(Total& self, const Total& other) {
        self.combine(other); // Delegate to the existing concrete API.
    }
};

Total source{10};
auto receiver = borrow(Combinable, source);
auto owned = adapt(Combinable, Total{20});
auto boxed = box(Combinable, Total{30});

const dyn(Combinable)& operand = owned;
receiver.combine(operand); // source.amount == 30
receiver.combine(*boxed);  // source.amount == 60
owned.combine(receiver);  // owned value is now 80; source is unchanged.
```

The generated interface operation is `void combine(const dyn(Combinable)&)` on
a **mutable receiver**. The mapping must provide
`static void combine(T&, const T&)`. Borrowed, owning, and boxed adapters for the
same underlying `T` interoperate; wrapper/storage type does not determine the
match. A valid mapping makes `Combinable<Total>` true under the same concept rule
as fixed-signature traits. Raw objects remain unchanged: they do not implicitly
convert to runtime operands or gain interface member functions. A class may keep
its concrete method, as above; the mapping delegates to it. `implements(Combinable)`
is unsupported for a trait with a concrete marker: use an explicit `impl` and
adapters for runtime calls.

A different underlying mapped operand type throws `std::invalid_argument`
**before the mapping is called**. Exceptions thrown by user code propagate
unchanged. A const object cannot be borrowed as a receiver for this mutable
contract. A `const dyn(Combinable)&` referring to an existing wrapper created
from a mutable object is valid as the read-only operand.

Merge requires RTTI for the same-underlying-type check. A trait containing any
merge operation cannot borrow a const object, even if it also contains clone
operations. An existing wrapper may still be viewed through a const reference
and used as a merge operand or clone source.

### Cloning into an owning runtime object

```cpp
trait(Cloneable, concrete_method(duplicate, clone));

impl(Cloneable, Total) {
    static Total duplicate(const Total& self) { return self; }
};

const Total source{10};
auto reference = borrow(Cloneable, source);
const dyn(Cloneable)& runtime = reference;
std::unique_ptr<dyn(Cloneable)> copy = runtime.duplicate();
auto another = copy->duplicate();
```

Every clone owns a new raw, unqualified `T`, even when its source is borrowed or
const; the library wraps the returned value, not a copy of the borrowing wrapper.
Whether the new value shares resources or references with the source depends on
`T` and the mapping: cloning does not automatically perform a deep copy. The result
preserves the **whole same trait interface**, including any other clone, merge,
fixed, or default operations, rather than a reduced clone-only interface. The raw
`T` must satisfy `std::move_constructible` so the returned value can be stored in
an owning adapter. Cloning allocates a new owning adapter containing both payload
and interface in one library allocation, with no second library allocation;
the payload and user mapping may allocate independently.

A clone-only trait needs no RTTI and works with `-fno-rtti`; adding a merge shape
requires RTTI. Const borrowing is supported when all fixed/default operations
are const and the concrete operations are clone-only. Mutable fixed operations
still require a mutable source.

Both concrete shapes may coexist, under arbitrary names, with ordinary `method`
and `default_method` operations. All count toward the same **8-operation total**.
These are small fixed-shape generators, not general runtime `Self` machinery:
there are no extra parameters, custom qualifiers, mutable merge operands, or
concrete default bodies.

## Static concrete-type contracts

Use `static_trait` for general operations whose signatures depend on the concrete
type, including concrete return values, without runtime erasure:

```cpp
static_trait(StaticCombinable, T, requires(T& object, const T& other) {
    { object.combine(other) } -> std::same_as<void>;
});
static_trait(StaticCloneable, T, requires(const T& object) {
    { object.clone() } -> std::same_as<T>;
});

struct Value {
    int amount;

    void combine(const Value& other) { amount += other.amount; }
    Value clone() const { return *this; }
};

static_assert(StaticCombinable<Value>);
static_assert(StaticCloneable<Value>);

template<StaticCombinable T>
void combine(T& object, const T& other) {
    object.combine(other);
}

Value first{10}, second{20};
combine(first, second); // One deduced T: both operands have the same type.
Value copy = first.clone();
```

These are ordinary C++20 expression constraints: plain objects can satisfy them
without inheritance, adapters, or runtime machinery. They check the expressions
and their result types, not an exact pointer-to-member signature. No runtime
interface or factories are generated for these static traits. The runtime
`concrete_method` marker above is a separate, explicit narrow contract, not
automatic runtime `Self` signature rewriting.

A type can independently implement runtime traits and satisfy static contracts.
`StaticCloneable` checks a concrete member call; runtime `Cloneable` checks an
explicit trait implementation and provides owning erased clones. Compare the
self-contained [static contracts example](examples/static_traits/main.cpp) with
the [runtime concrete operations example](examples/concrete_methods/main.cpp).
Concrete members remain ordinary C++ methods; erased calls use adapters.

## Combining capabilities

Use `composed_trait` when you want both a concept and a runtime bundle:

```cpp
trait(Powerable, method(float, getPower, const));
composed_trait(PoweredRotatable, T, Rotatable, Powerable);

struct PoweredMotor : Motor, implements(Powerable) {
    float getPower() const override { return 750; }
};

void inspect(const PoweredRotatable auto& object) {
    std::cout << object.getRpm() << ' ' << object.getPower() << '\n';
}
```

The concept is a conjunction of the component concepts, so raw types with valid
mappings for both components can satisfy it. The `inspect` body still needs the
original type to expose both called members; concept satisfaction does not add
them. Use wrappers exposing the needed interfaces when those members are absent.
Runtime bundles select a component explicitly rather than duplicating its methods:

```cpp
PoweredMotor powered;
inspect(powered);

auto capabilities = borrow(PoweredRotatable, powered);
capabilities.as<dyn(Rotatable)>().getRpm();
capabilities.as<dyn(Powerable)>().getPower();

auto ownedCapabilities = adapt(PoweredRotatable, powered);
auto boxedCapabilities = box(PoweredRotatable, PoweredMotor{});
boxedCapabilities->as<dyn(Rotatable)>().isStopped();
```

All selected interfaces refer to one underlying object. Composition factories
require sources that already expose every component runtime interface, not just
satisfy the concept conjunction. Their `requires` constraints reject sources
that only provide mappings; there is no automatic multi-adaptation. Bundles are
runtime handles and do not automatically satisfy the component concepts.

For compile-time-only requirements, use `static_trait` with any C++20 constraint:

```cpp
static_trait(HasRotationAndPower, T, Rotatable<T> && Powerable<T>);
static_trait(CopyableRotation, T, Rotatable<T> && std::copyable<T>);
```

Static traits can also check associated types, constants, static operations, and
`requires` expressions. They do not generate runtime interfaces or factories.

## Supported scope

- Up to **8 operations per trait** (ordinary, default, and concrete combined),
  **8 parameters per ordinary operation**, and **8 distinct
  ordinary traits per composition**. Nested runtime compositions are not supported.
- Ref-qualified and volatile operations are not supported by external adapters.
  Use aliases for complex declarators such as function-pointer types; parameter
  defaults and unnamed parameters are not supported.
- Borrowed objects must outlive every wrapper/reference to them. Const composed
  borrowing produces a read-only view; use `auto` to preserve its type.
- Use normal, non-`final` external mapping structs when relying on default fallback.
- Constraint checks do not cover every malformed mapping or competing required-method
  overload set. Keep mapping calls unambiguous and borrow const objects only for
  read-only traits; see [the development guide](DEVELOPMENT.md) for known edge cases.
- Global keyword macro names are part of the library's opinionated API and may
  conflict with other libraries. The current implementation has been tested with
  GCC; cross-compiler portability validation is still pending.

## Focused examples

Each example is self-contained and includes only `traits.hpp` from the library.
See the [examples index](examples/README.md) for more guidance.

| Example | Focus |
| --- | --- |
| [basic](examples/basic/main.cpp) | Declare a trait and implement its native interface |
| [external_mapping](examples/external_mapping/main.cpp) | Map an unchanged external type with `impl` |
| [ownership](examples/ownership/main.cpp) | Borrow, own, and box adapted values |
| [defaults](examples/defaults/main.cpp) | Shared default methods and overrides |
| [static_traits](examples/static_traits/main.cpp) | Compile-time concrete-type contracts |
| [composition](examples/composition/main.cpp) | Combine capabilities and select runtime interfaces |
| [concrete_methods](examples/concrete_methods/main.cpp) | Same-type merge and owning runtime clone operations |

## Build and run this repository

The example/test build requires a C++20-capable compiler, CMake 3.21+, and Ninja.
Bash is needed only for the convenience scripts. From the repository root:

```sh
./build.sh
./run.sh                       # Defaults to basic.
./run.sh ownership             # Select an example.
./run.sh --list
./run.sh --help
```

Or use CMake directly:

```sh
cmake --preset debug
cmake --build --preset debug
./build/debug/examples/basic
ctest --preset debug
```

`./run.sh [name] [args...]` runs a previously built Debug example and forwards
remaining arguments to it; unknown names are rejected. `build.sh` configures and
builds the Debug preset. CMake example targets are named `cpptraits_example_NAME`
(for example, `cpptraits_example_ownership`); Debug executables are placed in
`build/debug/examples/NAME`.

Replace `debug` with `release` in the CMake commands and executable path for an
optimized build. On Windows, use the corresponding `.exe` executable; MSVC
requires a configured developer shell.

For a top-level build, `CPPTRAITS_BUILD_EXAMPLES` and `CPPTRAITS_BUILD_TESTS`
default to ON. Standard `BUILD_TESTING` controls CTest registration and gates the
dedicated tests; examples can still build with it OFF.
`CPPTRAITS_TEST_CLONE_NO_RTTI=ON` enables an additional test for clone-only traits
with RTTI disabled.

## Use CppTraits in your project

CppTraits is a single-header library. For direct integration, copy only
`src/traits.hpp`, add its directory to your include path, and compile as C++20
or newer:

```cpp
#include "traits.hpp"
```

In complete programs, include required standard and third-party headers **before**
`traits.hpp`. Its global keyword macros, such as `param`, can collide with
names declared by headers included later. The snippets above are fragments;
combine their required includes at the top of your source file.

For example, after vendoring this repository at `third_party/CppTraits`, configure
an existing CMake executable target:

```cmake
add_subdirectory(third_party/CppTraits)
target_link_libraries(my_app PRIVATE CppTraits::CppTraits)
```

The `CppTraits::CppTraits` alias names the header-only `cpptraits` INTERFACE
target, which supplies the build-tree include directory and C++20 requirement.
Repository warning flags are private to its own executables, not propagated to
consumers. Both `CPPTRAITS_BUILD_EXAMPLES` and `CPPTRAITS_BUILD_TESTS` default to
OFF when included with `add_subdirectory`, so consumers do not build them by
default. This is build-tree integration; no install/export package is provided.

Or compile directly:

```sh
c++ -std=c++20 -I third_party/CppTraits/src main.cpp -o my_app
```

Place trait declarations and shared mappings in guarded headers. Namespaced traits
work with qualified keywords, for example `dyn(app::Rotatable)` and
`borrow(app::Rotatable, object)`. Declare `impl` specializations in the trait's
namespace or an enclosing namespace, not an unrelated namespace.

No library binary needs linking: integration consists of including `traits.hpp`
and enabling C++20. The [focused examples](examples/README.md) are the starting
point for usage; headers under `tests/fixtures/` exist only for regression tests
and are not required by consumers.

For implementation details, the roadmap, and contributor workflows, see
[DEVELOPMENT.md](DEVELOPMENT.md).

## AI usage disclaimer

CppTraits is AI-assisted, not "vibe-coded." Its contracts and implementation
machinery were established through iterative design, implementation, and testing
to complete the intended feature set within the library's current scope — not by
accepting generated code without understanding or validating it.

AI was used to help:

- Implement and expand the test suite, including regression and rejection cases.
- Generate and refine the macro machinery supporting the public API.
- Strengthen compile-time checks, ownership handling, and runtime usage safety.
- Identify unintended usage and add safeguards around unsupported operations and
  the boundaries of the documented contracts.

AI assistance does not replace engineering judgment, code understanding, or
validation. Tests and safeguards support the documented behavior; they are not a
guarantee of correctness or safety for every compiler, lifetime pattern, or use
outside the supported scope.
