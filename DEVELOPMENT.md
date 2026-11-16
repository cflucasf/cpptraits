# CppTraits development guide

This guide describes implementation contracts, development workflows, and planned
work. See [README.md](README.md) for the user API, declaration examples, default
methods, external mappings, ownership choices, and practical lifetime caveats.
The single library header is the source of truth; generated names below explain
the design, not an additional supported API.

## Handwritten C++ comparison

See [benchmarks/README.md](benchmarks/README.md) for the standalone comparison
suite, methodology, limitations, and reproduction commands. The current
[measured snapshot](benchmarks/RESULTS.md) and [raw samples](benchmarks/results.json)
cover fixed interfaces, clone-only traits without RTTI, and combined merge/clone
traits with RTTI. Runtime throughput, storage, ordinary allocations/requested bytes,
compilation/linking, and binary size are reported separately over 35 cases per
backend. Minimal merge/clone cases distinguish operation costs from result getters.
The current local comparison identifies successful checked merge as the largest
consistent relative overhead; see the benchmark guide for measured ranges,
assembly evidence, and limitations. Normal CMake builds do not run benchmarks.

## Build, test, and editor workflow

Development requires a C++20 compiler, CMake 3.21 or newer, and Ninja for the
provided presets. GCC has been used for development; cross-compiler validation
remains pending. Warning settings for Clang and MSVC are not evidence of validated
support. Bash is needed only for the convenience scripts.

From the project root:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/examples/basic
```

Replace `debug` with `release` for the optimized configuration. Alternatively,
`./build.sh && ./run.sh` builds Debug and runs `basic`. The scripts resolve paths
relative to themselves and stop on errors. `run.sh [name] [args...]` selects a
built Debug example and forwards the remaining arguments; `--list` lists names,
`-h`/`--help` shows usage, and unknown names are rejected. Example targets are
`cpptraits_example_NAME`, with Debug executables at `build/debug/examples/NAME`.

The root CMake project defines the header-only `cpptraits` INTERFACE target and
its `CppTraits::CppTraits` alias. It propagates the build-tree `src` include
directory and `cxx_std_20`. Repository-owned executables disable C++ extensions
and receive private warning flags, not promoted to errors; those settings do not
leak to consumers. `CPPTRAITS_BUILD_EXAMPLES` and `CPPTRAITS_BUILD_TESTS` both
default to `PROJECT_IS_TOP_LEVEL`: ON for standalone builds, OFF for consumers
using `add_subdirectory`. Standard `BUILD_TESTING` controls CTest registration
and also gates dedicated test builds; example builds are independent of it.
Do not assume install/export targets or an external test framework. Document
actual validation results rather than inferring success from test registration
or an earlier implementation's results.

### Regression coverage

A default top-level build registers **19 tests**: ten dedicated test executables
and nine example smoke tests. Dedicated tests require both `BUILD_TESTING` and
`CPPTRAITS_BUILD_TESTS`; example smoke tests require both `BUILD_TESTING` and
`CPPTRAITS_BUILD_EXAMPLES`. The optional `CPPTRAITS_TEST_CLONE_NO_RTTI` CMake
option is OFF by default; enabling it adds `concrete_clone_no_rtti` when dedicated
tests are enabled, for **20 tests** with the other defaults unchanged. The three
opt-in compile-failure checks below remain part of the manual validation workflow.

| File / CTest name | Main responsibilities |
| --- | --- |
| `examples/*/main.cpp` / `example_NAME` | Nine self-contained usage examples, each registered as a smoke test |
| `tests/traits_test.cpp` / `traits` | Interface and explicit-mapping checks, rejected unmapped structural/private/ambiguous types, mutable and multiple operations, heterogeneous dispatch, destruction, runtime factories |
| `tests/vendor_motor_test.cpp` / `vendor_motor` | External signatures and `noexcept`, const borrowing, rejected temporaries, built-in types, move-only values/arguments, vendor conversion |
| `tests/static_traits_test.cpp` / `static_traits` | Arbitrary static requirements, compile-time composition, and associated mapping checks |
| `tests/composed_traits_test.cpp` / `composed_traits` | Component selection, constness, storage, copying/moving, rebinding, composition limits |
| `tests/namespace_isolation_test.cpp` / `namespace_isolation` | Generated-name isolation and qualified macro arguments |
| `tests/method_declarations_test.cpp` / `method_declarations` | Short/four-field forms, named parameters, forwarding, comma-containing types, method/parameter limits |
| `tests/default_methods_test.cpp` / `default_methods` | Native/external defaults, nested dispatch, all-default opt-in, parameterized/comma-containing bodies, invalid optional names and overload families |
| `tests/concrete_operations_test.cpp` / `concrete_operations` | Static combine/clone contracts and concrete-type operations without automatic runtime erasure |
| `tests/concrete_methods_test.cpp` / `concrete_methods` | Merge mappings, same-underlying-type adapter interoperability, mismatch handling, constness, and coexistence with ordinary operations/defaults |
| `tests/concrete_clone_test.cpp` / `concrete_clone` | Exact clone signatures, owning results from borrowed/const sources, whole-interface preservation, mixed merge/clone traits, and mapping/constness constraints |

The shared test headers `rotatables.h`, `external_rotatable.h`, and
`vendor_motor.hpp` live unchanged under `tests/fixtures/` solely as regression
fixtures. They are not the primary usage examples or part of the library.
See [examples/README.md](examples/README.md) for the focused examples.

Factory tests also check deduction, copy/move behavior, single evaluation,
mutable borrowing, boxed ownership/destruction, and invalid arguments. Keep
negative cases in requires-expressions/static assertions when rejection is
supposed to occur through constraints, rather than breaking ordinary builds.
For a focused iteration after building:

```sh
ctest --preset debug -R '^(default_methods|concrete_operations|concrete_methods|concrete_clone)$'
```

### Opt-in compile-failure fixtures

Three test translation units have deliberately ill-formed declarations behind
preprocessor flags. These are manual checks, not registered compile-fail tests:

```sh
g++ -std=c++20 -Isrc -fsyntax-only -DCPPTRAITS_TEST_BAD_METHOD_ARITY tests/method_declarations_test.cpp
g++ -std=c++20 -Isrc -fsyntax-only -DCPPTRAITS_TEST_DUPLICATE_COMPONENT tests/composed_traits_test.cpp
g++ -std=c++20 -Isrc -fsyntax-only -DCPPTRAITS_TEST_NATIVE_CONCRETE tests/concrete_methods_test.cpp
```

All three commands must fail: the first checks `method(...)` arity; the second
rejects a duplicate component; the third rejects native `implements` for a trait
with a concrete marker.
Check the failure reason, not merely the nonzero exit status. Run the normal
build/tests without these flags to ensure positive cases remain well formed.
Expanded positive and compile-failure coverage remains roadmap work.

### Fixed and clone-only traits without RTTI

The concrete-method fixture has a separate fixed-trait branch to verify that
including the library does not force RTTI on ordinary users:

```sh
c++ -std=c++20 -O2 -DNDEBUG -fno-rtti -Isrc \
    -DCPPTRAITS_TEST_ORDINARY_NO_RTTI tests/concrete_methods_test.cpp \
    -o build/ordinary_no_rtti
./build/ordinary_no_rtti
```

Clone-only traits also require no RTTI. Enable the optional CMake target to build
the clone fixture's no-RTTI branch (`CPPTRAITS_TEST_CLONE_NO_RTTI` preprocessor
definition), using `-fno-rtti` on GCC/Clang or `/GR-` on MSVC:

```sh
cmake --preset debug -DCPPTRAITS_TEST_CLONE_NO_RTTI=ON
cmake --build --preset debug
ctest --preset debug -R '^concrete_clone_no_rtti$'
```

With all other defaults unchanged, this adds a twentieth test only while the
option is enabled; it is not an extra default test. Any trait containing merge
still requires RTTI. The fixed and clone-only benchmark suites build both backends
with `-fno-rtti`; the merge/clone suite uses `-frtti`. Retained snapshots cover all
three suites, including concrete dispatch, cloning, and heterogeneous containers.

### Editor support

Each configured build directory contains `compile_commands.json`. `.clangd`
points clangd, including Zed's C++ language support, at `build/debug`. Configure
Debug before editing so the language server has compiler flags and include paths.
Language-server diagnostics do not replace a real build or negative checks.

## Implementation architecture

### Direct fixed interfaces and namespace boundaries

The entire library lives in `src/traits.hpp`, the only file under `src/`.
It contains both the declaration generators and runtime composition machinery.
Copy only `traits.hpp` when vendoring the library.
`trait(Name, ...)` emits a public concept `Name` and a sibling namespace
`Name_cpptraits_detail` in the declaration's scope. That namespace contains:

- `cpptraits_interface_type`: a direct virtual interface with virtual destruction
  and the `cpptraits_ordinary_interface` marker.
- `cpptraits_satisfied`: the runtime-interface branch, checking public unambiguous
  inheritance plus exact member signatures.
- `cpptraits_implementation<T>`: an incomplete primary template specialized by
  `impl`; mappings are explicit opt-ins, not structural discovery.
- `cpptraits_implementation_check` and `cpptraits_has_implementation`: mapping
  completeness, object/cv restrictions, and required/optional validation.
- `cpptraits_owning_adapter<T>` and `cpptraits_reference_adapter<T>`: interface
  implementations storing a value or an object pointer.
- `cpptraits_make_value`, `cpptraits_make_reference`, `cpptraits_make_box`:
  constrained construction entry points for the runtime macros.

The public `Name<T>` concept accepts either `cpptraits_satisfied<T>` or a valid
explicit `cpptraits_has_implementation<T>` mapping. This rule applies uniformly
to fixed-signature traits and traits containing concrete markers. Mapping
satisfaction does not add members, interface inheritance, or implicit conversion;
runtime calls on mapped sources still require adapters.

Required methods are pure virtual; default methods have ordinary virtual bodies.
Descriptors generate these declarations, exact concept/mapping checks, and adapter
overrides directly. `concrete_method(name, merge)` and
`concrete_method(name, clone)` use small fixed-shape generators for checked erased
operands and owning erased results, not general `Self` machinery or automatic
signature rewriting. There is no
concrete-type registration, runtime type-token protocol, signature metadata, or
library `Result`/mismatch-result wrapper.

Traits containing merge use a common typed adapter base shared by owning and
borrowing adapters for the same underlying mapped `T`. Merge dispatch uses
`dynamic_cast` on the operand to that base, so owning, borrowed, and boxed
operands match independently of storage kind. A failed cast throws
`std::invalid_argument` before mapping entry. Clone dispatch calls the mapping on
a const payload reference and returns a newly allocated owning adapter for the
same whole trait. Const concrete borrowing validates both the requested const
mapping and the unqualified mapping needed by that owning result; this prevents
accepting a borrow whose clone would have invalid operations or lose a default
override. Clone needs no operand check or RTTI unless the trait also contains
merge. Fixed and clone-only traits remain usable with `-fno-rtti`. User exceptions
propagate unchanged. Both shapes use the same single header without adding
`Self` metadata or changing the composition core's behavior.

`dyn(Name)` selects the interface. `implements(Name)` expands to public interface
inheritance; callers supply the colon. `impl(Name, Type)` emits a qualified
explicit specialization of the mapping. `adapt`, `borrow`, and `box` select the
corresponding factory. Their variadic expression tails permit initializer-list
commas, and each expression is evaluated once.

Shared machinery lives in `::cpptraits_detail`, including
`implementation_signature`, `invalid_method_arity`, `unique_types`,
`interface_view`, and `owned_bundle`. Generated names are not access privacy;
compiler diagnostics can expose them. Prefer `auto`, interface references, or
factory `decltype` over naming wrappers in application APIs.

Declare traits with simple names inside their intended namespaces. Qualified
trait arguments work with `dyn`, `implements`, `impl`, and ordinary factories.
Mapping specializations must obey C++ explicit-specialization scope rules and
appear before checking/adapting the type. Shared declarations and mappings belong
in guarded headers; use a type alias for a comma-containing `impl` type argument.
There is no helper concept namespace: use ordinary C++20 constraints and `std::`.

### Descriptor expansion and macro limits

Ordinary descriptors carry required/default kind, return type, method name,
parameter declarations, forwarding expressions, qualifiers, and body. The concrete
descriptor carries an arbitrary operation name and a `merge` or `clone` shape,
selecting one of two fixed contracts.
Separate expansions declare interface methods, check signatures/mappings, and
emit adapter overrides.

- Traits accept 1–8 descriptors total: ordinary required, default, and concrete
  markers combined. All-default traits are supported; empty declarations are not.
- `concrete_method(name, merge)` maps `static void name(T&, const T&)` to mutable
  `void name(const dyn(Trait)&)`.
- `concrete_method(name, clone)` maps `static T name(const T&)` to
  `std::unique_ptr<dyn(Trait)> name() const`.
- Both shapes may coexist under arbitrary method names with fixed/default
  operations, within the shared eight-operation limit. Neither accepts custom
  return types, parameter lists, qualifiers, or a concrete default body.
- Methods accept up to eight named parameters, with an independent iteration chain.
- `method(Return, name, qualifiers)` is the no-parameter short form. The full form
  is `method(Return, name, (param(Type, name), ...), qualifiers)`; use `()` for none.
- Defaults always use `default_method(Return, name, parameters, qualifiers,
  { body })`. The variadic body tail preserves ordinary commas and braced return
  initializers; do not parenthesize the entire body.
- `param` emits `Type name` and `std::forward<decltype(name)>(name)`. Values forward
  as rvalues, lvalue references stay lvalues, and declared `T&&` is not a deduced
  forwarding reference. Transformations/reordering belong in mapping bodies.
- Whole-type parentheses protect comma-containing return/parameter types. Aliases
  remain useful for function-pointer/array declarators. Raw parameter lists,
  default arguments, unnamed parameters, explicit forwarding lists, and C-style
  varargs are unsupported.
- Runtime descriptors represent fixed-signature virtual-compatible members, not
  static operations, member templates, or arbitrary concept expressions.

The global macros are `trait`, `method`, `default_method`, `concrete_method`, `param`, `impl`,
`implements`, `dyn`, `adapt`, `borrow`, `box`, `static_trait`, and `composed_trait`.
These deliberate short names can conflict with other libraries. Namespace
isolation does not eliminate preprocessor collisions; there are no opt-in
prefixed aliases.

## Validation, defaults, and mapping constraints

### Interface or explicit-mapping contracts and exact signatures

For every generated trait, `Name<T>` accepts the existing exact runtime-interface
checks **or** a valid explicit `impl(Name, T)`. The interface branch uses
`std::derived_from` plus explicit pointer-to-member casts for every descriptor.
That branch requires public, unambiguous interface inheritance; matching methods
alone is insufficient. Private/ambiguous inheritance and incompatible hiding can
fail that branch, though an independently valid mapping can satisfy the concept.
Interfaces and abstract derived types may satisfy it: satisfaction does not
promise instantiability or copying. The mapping branch validates the explicit
mapping rather than discovering matching source members structurally.

Pointer casts select declared signatures and overloads rather than testing
convertible call expressions. Return/parameter types must match. Normal C++
rules govern access, inheritance, hiding, and overload resolution. A nonthrowing
operation may satisfy a throwing contract; a `noexcept` contract requires a
nonthrowing operation.

`cpptraits_detail::implementation_signature<Member, T>` translates member-pointer
types into static mapping-function-pointer types:

| Interface member | Required mapping shape |
| --- | --- |
| `R (C::*)(Args...)` | `R (*)(T&, Args...)` |
| `R (C::*)(Args...) const` | `R (*)(const T&, Args...)` |
| Either with `noexcept` | Corresponding function pointer with `noexcept` |

Only empty, `const`, `noexcept`, and `const noexcept` qualifier combinations have
external translations. Ref-qualified and volatile external operations are
unsupported. Mapping functions must be accessible static functions. Required
names must all pass exact pointer-cast validation.

Mappings are stateless dispatch definitions; adapters store objects. A valid
explicit mapping makes an unchanged vendor or built-in type satisfy the concept;
its interface-derived wrappers also satisfy it. For example,
`Rotatable<vendor::Motor>` is true through its mapping. This neither injects member
functions nor creates an implicit interface conversion. A concept-constrained
body using `object.getRpm()` needs that member on the original type, or an adapter
that exposes it; the concept alone does not make the expression valid. Ordinary
factories require valid external mappings and do not automatically wrap existing
native implementations/adapters. All-default traits still need explicit complete
mappings, possibly empty.

### Concrete merge and clone contracts

| Descriptor | Exact static mapping | Runtime member |
| --- | --- | --- |
| `concrete_method(name, merge)` | `static void name(T&, const T&)` | `void name(const dyn(Trait)&)` (mutable) |
| `concrete_method(name, clone)` | `static T name(const T&)` | `std::unique_ptr<dyn(Trait)> name() const` |

Both shapes are map-only: `implements(Trait)` is disallowed when either shape is
present. A class may retain ordinary concrete members and delegate to them from
an explicit `impl(Trait, T)`. A valid mapping makes the raw mapped type satisfy
the trait, but adds no members or implicit interface conversion. Erased calls
require explicit adapters. Mapping validation selects an exact static function
pointer; clone returns exactly `T`, not `T&`, `const T`, or a convertible type.
The raw unqualified `T` must be `std::move_constructible` for clone's owning
wrapper. Exceptions from mappings, delegated methods, and payload construction
propagate unchanged; allocation failures also propagate.

Merge matches the underlying mapped `T`, not the owning/reference/boxed wrapper
type. Its RTTI check throws `std::invalid_argument` on mismatch before mapping
entry. Clone requires no RTTI unless its trait also contains merge. It creates
an owning adapter holding a raw unqualified `T`, even from a borrowed or const
source, and preserves the same whole trait interface, not merely its clone
operations. The new adapter includes payload and interface in one library
allocation, with no second library allocation. Payload/user allocations are
independent.

Const borrowing is supported for clone-only concrete traits whose fixed/default
operations are all const. Any merge or mutable fixed operation forbids const
borrowing. A const reference to an existing wrapper can still be a merge operand
or clone source. Both shapes may coexist under arbitrary names with fixed and
default methods within the shared eight-operation limit. No additional parameters,
mutable merge operands, custom qualifiers, concrete default bodies, or general
`Self` signature rewriting are supported.

### Default dispatch and optional-name detection

A default body is defined once as a virtual interface method. Native types inherit
it or override it normally. Calls inside defaults honor virtual overrides,
including external ones; constness and exception rules remain ordinary C++.

For ordinary required/default descriptors, adapters override both kinds. A default
override calls the selected static mapping function or makes a qualified call to
the interface body. Qualified
entry avoids recursion; virtual calls inside that body still reach the adapter.
Composition selects existing interfaces rather than copying default bodies.

Optional-name validation must distinguish absence from an invalid supplied name.
Address-of failure alone cannot distinguish absence, access failure, and unmatched
overloads. `CPPTRAITS_HAS_IMPL_NAME` derives a probe from the mapping and a fallback
with a same-name data member; ambiguous lookup detects presence independently of
signature compatibility.

- Missing optional names select interface fallback.
- Supplied names must match the static signature. Wrong types/constness/`noexcept`,
  private/deleted/nonstatic functions, data members, and unmatched overloads reject
  mapping validity and factory constraints; they must not silently select fallback.
- Presence is per name, not overload. Supplying a name requires matching every
  optional descriptor in that overload family.
- Incomplete mappings fail completeness checking. Presence detection conservatively
  reports presence for incomplete or `final` mappings rather than deriving them.
- `final` mappings must provide matching overrides for every optional default.
- Other rare non-derivable mappings, such as those with a `final` virtual
  destructor, remain a hardening task.

Prefer non-`final` mapping structs. These restrictions concern the mapping, not
the adapted type. Preserve constraint-level rejection tests and instantiation order.

## Ownership, dispatch costs, and composition policy

Runtime participation is explicit. Interfaces have ordinary virtual methods and
virtual destruction. The ordinary-interface marker, not abstractness, identifies
an interface: an all-default interface can be concrete. Avoid storing interfaces
by value or implying that slicing preserves polymorphism.

Owning adapters store one `T` inline; borrowing adapters store one `T*`. Both carry
polymorphic overhead. Adaptation copies lvalues and moves rvalues subject to
construction constraints. Inline adaptation/borrowing does not itself allocate;
`box` allocates an owning adapter. A concrete clone also allocates one owning
adapter with its raw unqualified payload inline and the same whole trait interface;
there is no second library allocation. Stored objects and user operations can allocate
independently. Do not promise ABI-specific sizes, vtable-pointer counts, or
zero-overhead runtime dispatch. Devirtualization is an optimization, not semantics.

Borrowing rejects temporaries and volatile objects and never extends lifetimes.
Read-only ordinary traits may borrow const objects; mutable contracts require
mutable ones. Owning a borrowed wrapper does not own its referent. There is no
implicit cloning or shared ownership. Static `clone` contracts still return
concrete values and generate no erased operations; explicit
`concrete_method(name, clone)` is the separate runtime owning-clone contract.

Native types may inherit several interfaces. Multiple-inheritance layout, pointer
adjustment, access, and ambiguous lookup follow ordinary C++ rules. Constructors
may be needed where virtual members prevent aggregate use. See
[examples/composition/main.cpp](examples/composition/main.cpp) for runtime
component selection and [examples/concrete_methods/main.cpp](examples/concrete_methods/main.cpp)
for mapped merge/clone operations. Mappings may delegate to existing concrete
members; those remain ordinary C++ methods. Raw mapped types satisfy their traits,
while runtime calls require adapters. Independent static-trait clone examples
live in [examples/static_traits/main.cpp](examples/static_traits/main.cpp),
separate from runtime declarations.

### Static contracts and concrete-type operations

`static_trait` emits only a concept from its supplied constraint. It can express
associated types/constants, static operations, member-template specializations,
and concrete-type relationships without runtime machinery:

```cpp
static_trait(StaticCombinable, T, requires(T& object, const T& other) {
    { object.combine(other) } -> std::same_as<void>;
});
static_trait(Cloneable, T, requires(const T& object) {
    { object.clone() } -> std::same_as<T>;
});
```

Plain objects can satisfy both with no inheritance or adapters. These constraints
check expression validity and exact expression result types, not exact member
signatures; ordinary overload resolution and argument conversions still apply.
They do not generate interfaces or factories. Checking one member-template
specialization proves nothing about all specializations. Adapters do not
implicitly forward arbitrary static properties. Combining generated trait concepts
retains each component's interface-or-valid-explicit-mapping rule; it does not
ensure that a generic body can call interface members directly on a raw source.

General automatic runtime `Self` handling remains out of scope. The two explicit
`concrete_method` shapes are small generators for checked same-type merge and
owning erased clone results. They do not infer arbitrary concrete relationships
or rewrite arbitrary signatures. Keep static combine/clone examples independent
of runtime declarations: `StaticCombinable` avoids runtime `Combinable`, and the
static `Cloneable` example belongs in a separate scope from runtime `Cloneable`.

### Runtime composition

`composed_trait` emits a conjunction of component concepts and factories for
`cpptraits_detail::interface_view<ReadOnly, Interfaces...>` and
`owned_bundle<T, Interfaces...>`. It does not add a combined nominal base.
`dyn(Composition)` names the mutable view, not an abstract component interface.

Views store a tuple with one pointer per component plus polymorphic bookkeeping
for boxed destruction. `as<Interface>()` selects a reference without `dynamic_cast`
or per-operation forwarding. Every component points into the same object.
Unselected components fail at compile time. Const view references select const
interfaces; borrowing a const object yields a distinct read-only view that cannot
convert to the mutable view. Rvalue `as` is deleted to discourage dangling references.

Owners store one inline object, not one per component. Copy/move construction
must rebind pointers to the destination's object. Assignment updates that object
when supported. The virtual `restore_owned_binding` hook prevents base-view
assignment from redirecting owners to unrelated objects. Preserve this invariant.

Plain views are copyable borrows. Copying/slicing an owner into a view does not
transfer ownership; the owner must outlive the view. Inline views/owners do not
allocate themselves; boxing allocates one owning bundle. Move-only objects are
supported subject to construction/assignment constraints.

Composition boundaries:

- Use 1–8 distinct ordinary runtime traits with simple unqualified component names
  visible at the declaration. Duplicate interfaces and nested compositions fail.
- The concept conjunction can accept raw types with valid component mappings.
  Runtime factories additionally require already-exposed component interfaces;
  their `requires` constraints explicitly reject raw mapped sources rather than
  failing inside a factory body. There is no automatic multi-adaptation.
- Bundles expose `.as<dyn(Component)>()`, not direct operation methods, and do not
  themselves satisfy component/composed concepts.
- `implements(Composition)` and `impl(Composition, T)` are unsupported. Implement
  or map ordinary components instead.
- Selection does not resolve conflicting contracts or ambiguous concrete lookup;
  concrete types may still need `using` declarations.

## Development roadmap and priorities

1. [x] **Check interface or explicit-mapping participation.** Accept exact public,
   unambiguous runtime-interface checks or a valid explicit mapping, uniformly for
   fixed and concrete traits. Permit abstract interface references without
   promising instantiability; concept satisfaction alone does not expose members.
2. [x] **Support external implementations.** Explicit mappings make unchanged
   third-party and built-in types satisfy the concepts. Owning/reference adapters
   expose runtime interfaces; raw sources gain no members or implicit conversions.
3. [x] **Separate static and runtime requirements.** Fixed virtual contracts use
   `trait`; arbitrary constraints and concrete-type combine/clone use `static_trait`.
   Explicit `concrete_method(name, merge)` and `concrete_method(name, clone)`
   add checked same-type operands and owning erased clones through mappings.
   General automatic runtime `Self` machinery remains
   out of scope, not pending feature work.
4. [ ] **Strengthen requirement checking.** Expand targeted coverage for signature
   versus expression matching, overloads, conversions, access, inheritance, and
   semantic contracts. Do not confuse type checks with behavioral guarantees.
   Known hardening cases include exact required-overload selection during adapter
   forwarding and rejecting invalid mutable mappings even when borrowing a const
   object; both predate the fixed-interface simplification.
5. [ ] **Manage inheritance and ownership costs.** Keep borrowing, copying/moving,
   dispatch, allocation, and multiple-inheritance policies explicit. Handwritten
   comparisons now cover fixed traits, clone-only and merge/clone adapters, plain
   baselines, and container cloning. Investigate the extra typed-adapter RTTI level
   on successful merges with a separate controlled experiment; larger-working-set
   and cross-compiler measurements remain future work.
6. [x] **Support composition and defaults.** Compile-time conjunctions, explicit
   runtime interface selection, shared virtual defaults, native/external overrides,
   all-default mappings, nested dispatch, and constraint-level optional-name checks.
7. [x] **Keep declaration ergonomics and isolate machinery.** Per-trait namespaces,
   qualified keywords, short/full methods, named parameters, forwarding,
   comma-containing types, and bounded method/parameter counts.
8. [ ] **Harden diagnostics and robustness.** Improve invalid-declaration/mapping/
   composition diagnostics, macro hygiene, cross-compiler validation, expanded
   positive/negative coverage, and rare non-derivable optional-name probes without
   changing the deliberate global keyword API.

## API maintenance boundaries

The library is unreleased. Document the current API, not a migration or
compatibility story for development experiments. Keep fixed interfaces direct;
do not reintroduce automatic concrete-type dispatch machinery through metadata.

Examples should use inline `method`, `param`, and `default_method` descriptors,
`concrete_method(name, merge)` and `concrete_method(name, clone)` for the two
fixed mapped concrete contracts,
`static_trait` for arbitrary constraints, and `composed_trait` for runtime bundles.
Put conversions and argument transformations in mapping bodies. Use public
keywords rather than generated names. Infer wrappers with `auto` or factory
`decltype`. Interface descriptor changes may require synchronized updates to
native overrides, mappings, examples, and tests.

## Repository layout

- `src/traits.hpp`: the sole library header and sole file in `src/`; descriptors,
  direct ordinary/static/composed declarations, exact validation, external
  mappings, owning/reference adapters, macros, interface views, uniqueness checks,
  inline owners, and binding restoration.
- `examples/*/main.cpp`: nine self-contained examples using only the public library header.
- `examples/README.md`: focused example index.
- `tests/fixtures/rotatables.h`, `tests/fixtures/vendor_motor.hpp`, and
  `tests/fixtures/external_rotatable.h`: unchanged legacy native traits, vendor
  motor, and external mappings retained solely for regression tests.
- `tests/`: ten dedicated translation units listed above, including
  `concrete_operations_test.cpp` for concrete-type static contracts,
  `concrete_methods_test.cpp` for runtime merge, and `concrete_clone_test.cpp`
  for runtime clone and its optional no-RTTI branch.
- `CMakeLists.txt`: `cpptraits` INTERFACE library and `CppTraits::CppTraits` alias,
  optional example/test executables, private executable warnings, 19 default
  top-level CTest tests, and the OFF-by-default `CPPTRAITS_TEST_CLONE_NO_RTTI`
  option for a twentieth test.
- `CMakePresets.json`: Debug/Release configure, build, and test presets.
- `build.sh`, `run.sh`: convenience scripts; `.clangd`: Debug compilation database.
- `build/`: generated files, ignored by Git.
- `benchmarks/`: standalone handwritten comparisons and measurement tooling.
- `README.md`: user guide; `DEVELOPMENT.md`: implementation and contributor guide.
