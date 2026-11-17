# Examples

Each folder contains a self-contained `main.cpp` with its own types and an active
exit-status check (including in Release builds). Standard headers come before
`traits.hpp` because the library exposes global keyword-style macros.

| Example | Focus |
| --- | --- |
| `basic` | Native `implements`, constrained calls, and a fixed runtime interface |
| `external_mapping` | Unmodified vendor type; rad/s-to-RPM mapping and runtime borrowing |
| `ownership` | Owning `adapt` versus `borrow`; heterogeneous owning `box` vector |
| `defaults` | Provided method body, omitted mapping, and external override |
| `static_traits` | Plain compile-time constraints without interface inheritance |
| `composition` | Two native traits combined with `composed_trait`, `borrow`, and `as` |
| `concrete_methods` | Mapped merge and owning clone operations through fixed interfaces |
| `class_template` | One constrained `impl` for a family of class template specializations |
| `associated_types` | A native iterator that binds the shared `Item` of `peek()` and `next()` |
| `iterable` | Extend `std::vector<T, Allocator>` with a read-only `Iterable` mapping, range-for, and runtime adapters |

## Run from the repository root

```sh
./build.sh && ./run.sh basic
./run.sh                  # defaults to basic
./run.sh --list
./run.sh concrete_methods
./run.sh class_template
./run.sh associated_types
./run.sh iterable
```

Or use CMake directly:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --target cpptraits_example_basic
./build/debug/examples/basic
```

Each example target is `cpptraits_example_{name}` and its executable is
`build/debug/examples/{name}`. Build all targets with `cmake --build build/debug`.

For an independent single-file smoke test (no shared example fixtures):

```sh
mkdir -p build/smoke
c++ -std=c++20 -Isrc examples/basic/main.cpp -o build/smoke/basic
./build/smoke/basic
```

Borrowed objects must outlive their views. Composition binds already-exposed
native component interfaces; a mapping alone does not supply those interfaces.
Concrete merge requires the same mapped concrete type on both sides and throws
`std::invalid_argument` otherwise; clone returns an owning interface pointer.
