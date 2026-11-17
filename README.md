# CppTraits

**Rust-inspired traits for C++20. Define a capability once; use it in generic code
and at runtime.**

CppTraits is an experimental, single-header library that generates concepts,
interfaces, and adapters from a shared declaration. It helps you:

- Express what a function needs instead of which concrete class it accepts.
- Give existing, third-party, and built-in types new capabilities.
- Choose between compile-time calls and runtime polymorphism.
- Share behavior, combine capabilities, and make ownership explicit.

Start with a trait below, or explore the [runnable examples](examples/README.md).
You need C++20 and [`traits.hpp`](src/traits.hpp); no library binary or code generator.

## A first trait

Declare the operations you need, then implement them with ordinary C++ methods:

```cpp
#include <iostream>
#include "traits.hpp"

trait(Rotatable,
    method(float, getRpm, const)
);

struct Motor : implements(Rotatable) {
    float rpm = 1400;
    float getRpm() const override { return rpm; }
};

void printRpm(const Rotatable auto& object) {
    std::cout << object.getRpm() << '\n';
}

int main() {
    Motor motor;
    printRpm(motor); // 1400
}
```

`Rotatable` is now a concept you can use in generic functions. `implements`
provides the matching runtime interface, so the same implementation can also
participate in virtual dispatch.[^contracts]

### Operations with parameters

Use `param(Type, name)` to declare arguments. Operations can be mutable, `const`,
`noexcept`, or `const noexcept`:

```cpp
trait(Adjustable,
    method(float, getRpm, const noexcept),
    method(void, setRpm, (param(float, rpm)), ),
    method(void, reset, )
);
```

Types containing commas can use an alias or an extra pair of parentheses:

```cpp
trait(Snapshots,
    method((std::pair<int, int>), snapshot, const),
    method(void, restore, (param((const std::pair<int, int>&), value)), )
);
```

## Extend existing types

Use `impl` to implement a trait **without changing the original type**. Mapping
functions receive the object first, followed by the operation's arguments:

```cpp
namespace vendor {
    struct Motor { double rpm; };
}

impl(Rotatable, vendor::Motor) {
    static float getRpm(const vendor::Motor& motor) {
        return static_cast<float>(motor.rpm);
    }
};

static_assert(Rotatable<vendor::Motor>);
```

Built-in types work too:

```cpp
impl(Rotatable, int) {
    static float getRpm(const int& rpm) { return static_cast<float>(rpm); }
};
```

### One call syntax for every implementation

`trait_call` lets generic code work with native implementations, external
mappings, and runtime adapters alike—without creating an adapter just to call an
operation:[^calls]

```cpp
float readRpm(const Rotatable auto& source) {
    return trait_call(Rotatable, getRpm, source);
}

readRpm(Motor{});             // Native member
readRpm(vendor::Motor{900});  // External mapping
readRpm(1200);                // Built-in type
```

You can also map an entire class-template family with a single `impl`, including
a `requires` clause on the specialization. See the
[class-template example](examples/class_template/main.cpp) and the associated-type
mapping below.[^mappings]

## Choose ownership and runtime dispatch

Use the same trait when the concrete type is known only at runtime:

| API | Use it when you want to… |
| --- | --- |
| `dyn(Trait)` | Name the runtime interface |
| `borrow(Trait, object)` | Work with the original object without copying it |
| `adapt(Trait, object)` | Own a copy of an lvalue or move in an rvalue |
| `box(Trait, object)` | Own an adapted value through a `std::unique_ptr` |

```cpp
vendor::Motor source{900};

auto view  = borrow(Rotatable, source);
auto owned = adapt(Rotatable, source);
auto boxed = box(Rotatable, vendor::Motor{1200});

source.rpm = 0;
view.getRpm();  // 0: observes source
owned.getRpm(); // 900: independent copy

const dyn(Rotatable)& runtime = view;
printRpm(runtime);
```

Borrowing does not extend the source's lifetime. Ordinary trait factories use
`impl` mappings; native objects already expose their interfaces.[^ownership]

For heterogeneous collections, store interface pointers:

```cpp
std::vector<std::unique_ptr<dyn(Rotatable)>> motors;
motors.push_back(std::make_unique<Motor>());
motors.push_back(box(Rotatable, vendor::Motor{900}));

for (const auto& motor : motors) {
    printRpm(*motor);
}
```

## Share default behavior

`default_method` supplies behavior once for every implementation. Its body can
call other trait operations, so implementations only need to provide the
building blocks:

```cpp
trait(Stoppable,
    method(float, getRpm, const),
    default_method(bool, isStopped, (), const, {
        return getRpm() == 0.0f;
    })
);

impl(Stoppable, vendor::Motor) {
    static float getRpm(const vendor::Motor& motor) {
        return static_cast<float>(motor.rpm);
    }
};

vendor::Motor stopped{0};
auto motor = borrow(Stoppable, stopped);
motor.isStopped(); // true: uses the shared default
```

Native types can override the member; mappings can provide a matching static
function instead. Calls inside default bodies honor those overrides.[^defaults]

## Express compile-time requirements

Use `static_trait` when you only need a concept, without an interface or adapters.
It accepts ordinary C++20 constraints, including checks on members, associated
types, and concrete return types:

```cpp
static_trait(StaticCloneable, T, requires(const T& object) {
    { object.clone() } -> std::same_as<T>;
});

struct Value {
    int amount;
    Value clone() const { return *this; }
};

static_assert(StaticCloneable<Value>);
```

Plain types can satisfy these requirements without opting into a runtime trait.
You can also combine existing concepts:

```cpp
static_trait(CopyableRotation, T, Rotatable<T> && std::copy_constructible<T>);
```

## Inline trait requirements

Use `trait_requires` when a runtime trait should also restrict its concrete
implementations. Reuse a `static_trait` or write a constraint directly:

```cpp
static_trait(Copyable, T, std::copy_constructible<T>);

trait(CopyableRotatable,
    trait_requires(Self, Copyable<Self>),
    method(float, getRpm, const)
);

struct CopyableMotor : implements(CopyableRotatable) {
    float getRpm() const override { return 1400; }
};

impl(CopyableRotatable, int) {
    static float getRpm(const int& rpm) { return static_cast<float>(rpm); }
};

static_assert(CopyableRotatable<CopyableMotor>);
static_assert(CopyableRotatable<int>);
```

The additional requirement applies to native types and external mappings, and
follows them through factories, `trait_call`, and composition. Multiple clauses
must all hold.

**`Self` is a name you choose—not a keyword, macro, or predefined alias.** It
represents the concrete object type inside that constraint; `Object` or `T`
would work just as well.[^requirements]

See the [calibrated-sensor example](examples/trait_requires/main.cpp) for accepted
and rejected types, adapters, and composition.

## Let implementations choose associated types

`associated_type` lets each implementation supply types shared by its operations.
For example, an iterator chooses the kind of item it produces:

```cpp
trait(Iterator,
    associated_type(Item),
    method(std::optional<Item>, next, )
);

struct Counter : implements(Iterator, int) {
    int current = 0;

    std::optional<int> next() override {
        if (current == 3) return std::nullopt;
        return current++;
    }
};
```

`implements(Iterator, int)` binds `Item` to `int`. External mappings supply the
alias themselves; this template mapping adapts a standard-library cursor with
copyable items:

```cpp
impl(Iterator, (template<class T>), (std::span<const T>)) {
    using Item = T;

    static std::optional<Item> next(std::span<const T>& cursor) {
        if (cursor.empty()) return std::nullopt;
        Item item = cursor.front();
        cursor = cursor.subspan(1);
        return item;
    }
};

const std::array numbers{10, 20};
std::span<const int> cursor{numbers};
auto view = borrow(Iterator, cursor);
view.next(); // 10

std::unique_ptr<dyn(Iterator, int)> owned = box(Iterator, cursor);
owned->next(); // 20
```

Factories infer associated types from the mapping. `dyn(Iterator, int)` names the
bound interface; multiple bindings follow declaration order.[^associated]
When generic code needs a mapping's aliases, `trait_mapping_t` exposes them:

```cpp
using Item = typename trait_mapping_t(Iterator, std::span<const int>)::Item;
```

Explore [associated iterators](examples/associated_types/main.cpp) or a
[read-only `Iterable` mapping for `std::vector`](examples/iterable/main.cpp).

## Combine capabilities

`composed_trait` groups capabilities into one generic constraint and a runtime
bundle. Use it when an operation needs more than one trait:

```cpp
trait(Powerable, method(float, getPower, const));
composed_trait(PoweredRotatable, T, Rotatable, Powerable);

struct PoweredMotor : Motor, implements(Powerable) {
    float getPower() const override { return 750; }
};

void inspect(const PoweredRotatable auto& object) {
    std::cout << trait_call(Rotatable, getRpm, object) << ' '
              << trait_call(Powerable, getPower, object) << '\n';
}
```

At runtime, select a component with `as<Interface>()`. All components refer to the
same underlying object:

```cpp
PoweredMotor motor;
inspect(motor);

auto capabilities = borrow(PoweredRotatable, motor);
capabilities.as<dyn(Rotatable)>().getRpm();
capabilities.as<dyn(Powerable)>().getPower();

auto owned = adapt(PoweredRotatable, motor);
auto boxed = box(PoweredRotatable, PoweredMotor{});
```

Component `trait_requires` clauses are inherited by the composed concept.
Runtime bundles currently bind existing component interfaces.[^composition]

## Clone and merge through runtime interfaces

`concrete_method` bridges two common operations that depend on the concrete type:
**cloning an owned value** and **merging values of the same type**. The caller
only needs the runtime interface:

```cpp
trait(EditableTotal,
    method(int, amount, const),
    concrete_method(combine, merge),
    concrete_method(duplicate, clone)
);

struct Total { int value; };

impl(EditableTotal, Total) {
    static int amount(const Total& total) { return total.value; }
    static void combine(Total& total, const Total& other) {
        total.value += other.value;
    }
    static Total duplicate(const Total& total) { return total; }
};

Total total{10};
auto view = borrow(EditableTotal, total);
auto other = box(EditableTotal, Total{20});
view.combine(*other); // total.value == 30

auto copy = view.duplicate(); // unique_ptr<dyn(EditableTotal)>
total.value = 0;
copy->amount(); // 30: owns a separate Total
```

The operation names are yours to choose; `merge` and `clone` select their shapes.
Clones retain the whole trait interface, and borrowed, owned, and boxed merge
operands interoperate when they wrap the same concrete type.[^concrete]

## Explore the examples

Each example is a self-contained program with a runtime result check.

| Example | What it shows |
| --- | --- |
| [basic](examples/basic/main.cpp) | A native trait implementation |
| [external_mapping](examples/external_mapping/main.cpp) | Extending an unchanged external type |
| [class_template](examples/class_template/main.cpp) | One mapping for a template family |
| [ownership](examples/ownership/main.cpp) | Borrowed values, owned values, and heterogeneous collections |
| [defaults](examples/defaults/main.cpp) | Shared behavior and implementation-specific overrides |
| [static_traits](examples/static_traits/main.cpp) | Compile-time-only contracts |
| [trait_requires](examples/trait_requires/main.cpp) | Extra requirements on runtime implementations |
| [associated_types](examples/associated_types/main.cpp) | Native and mapped iterators with associated item types |
| [iterable](examples/iterable/main.cpp) | Extending `std::vector` with read-only iteration |
| [composition](examples/composition/main.cpp) | Multiple capabilities on one object |
| [concrete_methods](examples/concrete_methods/main.cpp) | Runtime merge and owning clone operations |

Build and run from the repository root:

```sh
./build.sh
./run.sh trait_requires         # Or basic, ownership, iterable, ...
./run.sh --list
```

Or use CMake directly (CMake 3.21+ and Ninja):

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Use the `release` preset for an optimized build. More commands and contributor
workflows are in [DEVELOPMENT.md](DEVELOPMENT.md).

## Use CppTraits in your project

Copy `src/traits.hpp` into your project and compile as C++20 or newer. Include
standard and third-party headers **before** `traits.hpp`, because its global
keyword macros can collide with names in other headers.[^scope]

The snippets above build on earlier declarations and omit supporting includes
after the first example. The [example programs](examples/README.md) show complete
include lists.

If you vendor the repository, CMake integration is:

```cmake
add_subdirectory(third_party/CppTraits)
target_link_libraries(my_app PRIVATE CppTraits::CppTraits)
```

The target supplies the include path and C++20 requirement. Examples and tests
are off by default when included as a subproject. Alternatively, compile directly:

```sh
c++ -std=c++20 -I third_party/CppTraits/src main.cpp -o my_app
```

## AI usage disclaimer

CppTraits is AI-assisted, not "vibe-coded." AI has helped develop the macro
machinery, tests, and safeguards through iterative design and validation—not as
a replacement for understanding the code or exercising engineering judgment.
Tests support the documented behavior; they do not guarantee correctness for
every compiler, lifetime pattern, or unsupported use.

## Notes and current limits

These notes collect the practical caveats; detailed contracts and implementation
boundaries live in [DEVELOPMENT.md](DEVELOPMENT.md).

[^contracts]: Runtime traits require an explicit `implements` or valid `impl` mapping;
    matching member names alone is not enough. A mapping adds neither members nor
    implicit interface conversions to the original type.

[^calls]: `trait_call` exposes required operations only; use interface members for
    default and concrete operations. Native implementations take precedence over
    mappings. Declared overloads, reference returns, and `noexcept` are preserved;
    native calls retain virtual dispatch.

[^mappings]: Declare mappings before checking or adapting their types, in the
    trait's namespace or an enclosing namespace. Mapping signatures must match the
    declared operation, with `const T&` for const receivers and `T&` for mutable
    receivers. See [template mapping examples](examples/class_template/main.cpp).

[^ownership]: `borrow` accepts lvalues only; the source must outlive every view.
    Const sources need read-only operations. `adapt` and `box` copy lvalues or move
    rvalues; ownership of a value does not imply ownership of everything it refers
    to. `borrow` and `adapt` do not themselves allocate; `box` does. Runtime interfaces
    use ordinary C++ virtual dispatch. Native values can be owned directly with
    `std::make_unique<Concrete>()`.

[^defaults]: To use a mapped default, omit its name entirely or provide all declared
    overloads of that name. Prefer non-`final` mapping structs when relying on default
    fallback. Default bodies always receive an explicit parameter list, including `()`.

[^requirements]: `Self` denotes the cv/ref-unqualified concrete source type, not its
    mapping or an object instance; its scope is local to the constraint. Generated
    adapters check their source type; exact `dyn` interfaces carry the erased
    contract instead. Derived types recheck their own predicates, but upcasting can
    erase concrete properties. Constraints are not a runtime proof. See
    [requirement semantics](DEVELOPMENT.md#inline-trait-requirements).

[^associated]: Associated-type traits support required methods and `trait_requires`,
    but not defaults, concrete merge/clone, or runtime composition yet.
    `trait_mapping_t` is mapping-only; an alias declared solely in a mapping does
    not become a member of the original type.

[^composition]: Runtime composition currently needs public, unambiguous component
    interface bases. Raw mapped types can satisfy the composed concept, but factories
    do not build component adapters automatically. Associated-type components and
    nested runtime bundles are not supported. Const borrowing gives a read-only view.

[^concrete]: Concrete operations require `impl` mappings rather than `implements`.
    Merge expects `void(T&, const T&)`, requires RTTI, and throws
    `std::invalid_argument` for different underlying types. Clone expects `T(const T&)`
    and returns a newly allocated owning adapter; copying resources follows the mapping,
    not an automatic deep-copy policy. Clone-only traits do not require RTTI.

[^scope]: The bounded macros support eight descriptors per trait (including types
    and requirements), eight parameters per ordinary operation, and eight distinct
    ordinary components per composition. External adapters do not support volatile
    or ref-qualified operations. Some malformed mappings and competing overloads
    remain outside constraint-level rejection coverage. GCC has been tested;
    cross-compiler validation is pending. These keyword spellings are global macros,
    not language extensions. For advanced declarators and edge cases, see the
    [development guide](DEVELOPMENT.md).
