# immutable_map — C++17 Immutable Hash Map

A header-only C++17 hash map with immutable structure (keys and topology) but mutable values.

## Features

- **Immutable structure**: Keys and topology are fixed after `build()`. Values are mutable.
- **Single allocation**: Pairs and hash index in one memory block via user-provided allocator.
- **Builder pattern**: `reserve()` → `emplace()` → `build()` for zero-copy construction.
- **No move/copy required for Value**: Values are constructed in-place and never moved.
- **Conditional copy**: Copy operations enabled via CRTP base only when Value is copy-constructible.
- **Heterogeneous lookup**: `find(string_view)` on `map<string, ...>` — no temporary allocations.
- **Standard pair type**: Uses `std::pair<const Key, Value>` with real `pair&` iterators.
- **Exception safety**: Key rollback if Value constructor throws.
- **Allocator-aware**: All operations via `std::allocator_traits`; EBO via private inheritance.

## Project Structure

```
.
├── include/
│   └── immutable_map.hpp    # Header-only library
├── tests/
│   ├── CMakeLists.txt       # Test build configuration
│   └── test_immutable_map.cpp
├── docs/
│   └── README.md            # This file
├── src/                     # Web documentation source (React/Vite)
├── CMakeLists.txt           # Root CMake configuration
└── README.md                # Project overview
```

## Quick Start

```cpp
#include <imm/immutable_map.hpp>
#include <string>

// Build a map
imm::immutable_map_builder<std::string, int> builder;
builder.reserve(3);
builder.emplace("one", 1);
builder.emplace("two", 2);
builder.emplace("three", 3);
auto map = builder.build();

// Lookup
assert(map.at("two") == 2);
assert(map.contains("one"));

// Mutate values (keys are immutable)
map.at("two") = 42;
```

## Heterogeneous Lookup

```cpp
struct string_hash {
    std::size_t operator()(std::string_view sv) const noexcept {
        // xxh3 or similar
    }
};

struct string_eq {
    bool operator()(std::string_view a, std::string_view b) const noexcept {
        return a == b;
    }
};

imm::immutable_map<std::string, int, string_hash, string_eq> map;
// ... build map ...

// Lookup by string_view — no temporary string!
std::string_view key = "hello";
map.find(key);      // OK
map.at(key);        // OK
map.contains(key);  // OK
```

## Non-Movable Values

```cpp
struct NonMovable {
    std::unique_ptr<int> data;
    NonMovable(int x) : data(std::make_unique<int>(x)) {}
    NonMovable(const NonMovable&) = delete;
    NonMovable(NonMovable&&) = delete;
};

imm::immutable_map_builder<std::string, NonMovable> b;
b.reserve(2);
b.emplace("a", 10);  // constructed in-place
b.emplace("b", 20);
auto m = b.build();  // OK — values never moved
```

## Collect from Range

```cpp
// From pairs
std::vector<std::pair<std::string, int>> pairs = {
    {"one", 1}, {"two", 2}
};
auto m1 = imm::collect<std::string, int>(pairs);

// From tuples (Value constructed from tuple)
struct Point { int x, y; Point(int a, int b) : x(a), y(b) {} };
std::vector<std::pair<std::string, std::tuple<int, int>>> data = {
    {"origin", {0, 0}}, {"unit", {1, 1}}
};
auto m2 = imm::collect_construct<std::string, Point>(data);
```

## Building

### C++ Library (header-only)

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest
```

### Web Documentation

```bash
npm install
npm run build
# Output in dist/
```

## Design

### Memory Layout

```
[ std::pair<const Key, Value> [N] ]  [ padding ]  [ uint32_t [M] ]
```

- `N` = number of elements
- `M` = next power of 2 ≥ 2·N (load factor ≤ 0.5)
- Open addressing, linear probing
- Index slots store offsets into pairs array, or `UINT32_MAX` for empty

### Class Hierarchy

```
map_storage : private Alloc
  ↑ private
immutable_map
  ↑ private
copy_support_base (CRTP, conditional copy ops)

immutable_map_builder
  contains map_storage + capacity_
```

## License

Public domain / MIT (choose your preference)
