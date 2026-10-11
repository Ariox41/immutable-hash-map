# immutable_map

C++17 header-only immutable hash map with mutable values.

## Features

- Immutable structure (keys/topology fixed after build), mutable values
- Single allocation via `std::allocator_traits` with EBO
- Builder pattern: `reserve()` → `emplace()` → `build()`
- Value need not be movable or copyable (constructed in-place)
- Copy operations conditionally enabled via CRTP base
- Heterogeneous lookup (e.g., `find(string_view)` on `map<string, ...>`)
- `std::pair<const Key, Value>` with real `pair&` forward iterators
- Exception safety with key rollback on Value constructor failure

## Quick Start

```cpp
#include <imm/immutable_map.hpp>

imm::immutable_map_builder<std::string, int> b;
b.reserve(3);
b.emplace("one", 1);
b.emplace("two", 2);
b.emplace("three", 3);
auto m = b.build();

assert(m.at("two") == 2);
m.at("two") = 42;  // values are mutable
```

## Build & Test

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest
```

## Documentation

See [docs/README.md](docs/README.md) for full API reference and examples.

Interactive web documentation available in `src/` (React/Vite):
```bash
npm install
npm run dev    # development server
npm run build  # production build → dist/
```

## Project Structure

```
├── include/imm/immutable_map.hpp   # Header-only library
├── tests/                          # C++ tests (CTest)
├── docs/                           # Documentation
├── src/                            # Web docs source (React)
├── CMakeLists.txt                  # C++ build
└── package.json                    # Web docs build
```
