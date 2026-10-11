// tests/test_immutable_map.cpp
// Basic tests for immutable_map

#include <imm/immutable_map.hpp>

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

// ============================================================================
// Test helpers
// ============================================================================

#define TEST(name) \
    static void test_##name(); \
    struct test_##name##_reg { \
        test_##name##_reg() { \
            std::cout << "  " #name "... "; \
            test_##name(); \
            std::cout << "OK\n"; \
        } \
    } test_##name##_instance; \
    static void test_##name()

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "\n  ASSERTION FAILED: " #cond \
                      << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::abort(); \
        } \
    } while (0)

// ============================================================================
// Non-movable, non-copyable value type
// ============================================================================

struct NonMovable {
    std::unique_ptr<int> data;
    explicit NonMovable(int x) : data(std::make_unique<int>(x)) {}
    NonMovable(const NonMovable&) = delete;
    NonMovable(NonMovable&&) = delete;
    NonMovable& operator=(const NonMovable&) = delete;
    NonMovable& operator=(NonMovable&&) = delete;
};

// ============================================================================
// Heterogeneous lookup helpers
// ============================================================================

struct string_hash {
    using is_transparent = void;
    std::size_t operator()(std::string_view sv) const noexcept {
        // Simple hash for testing
        std::size_t h = 0;
        for (char c : sv) h = h * 131 + c;
        return h;
    }
};

struct string_eq {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept {
        return a == b;
    }
};

// ============================================================================
// Tests
// ============================================================================

TEST(basic_builder) {
    imm::immutable_map_builder<int, std::string> b;
    b.reserve(3);
    b.emplace(1, "one");
    b.emplace(2, "two");
    b.emplace(3, "three");
    auto m = b.build();

    ASSERT(m.size() == 3);
    ASSERT(!m.empty());
    ASSERT(m.at(1) == "one");
    ASSERT(m.at(2) == "two");
    ASSERT(m.at(3) == "three");
}

TEST(find_and_contains) {
    imm::immutable_map_builder<std::string, int> b;
    b.reserve(2);
    b.emplace("hello", 42);
    b.emplace("world", 99);
    auto m = b.build();

    ASSERT(m.contains("hello"));
    ASSERT(m.contains("world"));
    ASSERT(!m.contains("missing"));

    auto p = m.find("hello");
    ASSERT(p != m.end());
    ASSERT(p->second == 42);

    auto q = m.find("missing");
    ASSERT(q == m.end());
}

TEST(heterogeneous_lookup) {
    imm::immutable_map_builder<std::string, int, string_hash, string_eq> b;
    b.reserve(2);
    b.emplace("hello", 1);
    b.emplace("world", 2);
    auto m = b.build();

    // Lookup by string_view — no temporary string
    std::string_view sv = "hello";
    ASSERT(m.contains(sv));
    ASSERT(m.at(sv) == 1);

    auto p = m.find(std::string_view("world"));
    ASSERT(p != m.end());
    ASSERT(p->second == 2);

    // const char* also works
    ASSERT(m.contains("hello"));
}

TEST(value_mutation) {
    imm::immutable_map_builder<int, int> b;
    b.reserve(2);
    b.emplace(1, 10);
    b.emplace(2, 20);
    auto m = b.build();

    // Values are mutable
    m.at(1) = 100;
    m.at(2) = 200;
    ASSERT(m.at(1) == 100);
    ASSERT(m.at(2) == 200);

    // Keys are immutable (compile-time check — commented out)
    // m.begin()->first = 999;  // ERROR: const int&
}

TEST(non_movable_value) {
    imm::immutable_map_builder<std::string, NonMovable> b;
    b.reserve(2);
    b.emplace("a", 10);
    b.emplace("b", 20);
    auto m = b.build();

    ASSERT(m.size() == 2);
    ASSERT(*m.at("a").data == 10);
    ASSERT(*m.at("b").data == 20);
}

TEST(move_semantics) {
    imm::immutable_map_builder<int, int> b;
    b.reserve(3);
    b.emplace(1, 10);
    b.emplace(2, 20);
    b.emplace(3, 30);
    auto m1 = b.build();

    // Move constructor
    auto m2 = std::move(m1);
    ASSERT(m1.empty());
    ASSERT(m2.size() == 3);
    ASSERT(m2.at(1) == 10);

    // Move assignment
    imm::immutable_map<int, int> m3;
    m3 = std::move(m2);
    ASSERT(m2.empty());
    ASSERT(m3.size() == 3);
}

TEST(copy_semantics) {
    imm::immutable_map_builder<int, int> b;
    b.reserve(2);
    b.emplace(1, 10);
    b.emplace(2, 20);
    auto m1 = b.build();

    // Copy constructor
    auto m2 = m1;
    ASSERT(m2.size() == 2);
    ASSERT(m2.at(1) == 10);
    ASSERT(m2.at(2) == 20);

    // Modify copy — original unchanged
    m2.at(1) = 999;
    ASSERT(m1.at(1) == 10);
    ASSERT(m2.at(1) == 999);

    // Copy assignment
    imm::immutable_map<int, int> m3;
    m3 = m1;
    ASSERT(m3.size() == 2);
    ASSERT(m3.at(1) == 10);
}

TEST(empty_map) {
    imm::immutable_map<int, int> m;
    ASSERT(m.empty());
    ASSERT(m.size() == 0);
    ASSERT(m.find(42) == m.end());
    ASSERT(!m.contains(42));
    ASSERT(m.begin() == m.end());
}

TEST(empty_builder) {
    imm::immutable_map_builder<int, int> b;
    b.reserve(0);
    auto m = b.build();
    ASSERT(m.empty());
    ASSERT(m.size() == 0);
}

TEST(try_emplace_piecewise) {
    imm::immutable_map_builder<std::string, std::string> b;
    b.reserve(2);
    b.try_emplace(std::piecewise_construct,
                  std::forward_as_tuple("key1"),
                  std::forward_as_tuple("value1"));
    b.try_emplace(std::piecewise_construct,
                  std::forward_as_tuple("key2"),
                  std::forward_as_tuple("value2"));
    auto m = b.build();

    ASSERT(m.size() == 2);
    ASSERT(m.at("key1") == "value1");
    ASSERT(m.at("key2") == "value2");
}

TEST(try_emplace_convenience) {
    imm::immutable_map_builder<int, std::string> b;
    b.reserve(2);
    b.try_emplace(1, "one");
    b.try_emplace(2, "two");
    auto m = b.build();

    ASSERT(m.size() == 2);
    ASSERT(m.at(1) == "one");
    ASSERT(m.at(2) == "two");
}

TEST(iteration) {
    imm::immutable_map_builder<int, int> b;
    b.reserve(3);
    b.emplace(1, 10);
    b.emplace(2, 20);
    b.emplace(3, 30);
    auto m = b.build();

    // Forward iteration
    int sum = 0;
    for (auto&& [key, value] : m) {
        sum += value;
        // key is const int&, value is int&
        value *= 2;  // mutable
    }
    ASSERT(sum == 60);

    // Check mutation took effect
    ASSERT(m.at(1) == 20);
    ASSERT(m.at(2) == 40);
    ASSERT(m.at(3) == 60);

    // Const iteration
    const auto& cm = m;
    int count = 0;
    for (auto it = cm.begin(); it != cm.end(); ++it) {
        ++count;
    }
    ASSERT(count == 3);
}

TEST(collect_pairs) {
    std::vector<std::pair<std::string, int>> pairs = {
        {"one", 1}, {"two", 2}, {"three", 3}
    };
    auto m = imm::collect<std::string, int>(pairs);
    ASSERT(m.size() == 3);
    ASSERT(m.at("one") == 1);
    ASSERT(m.at("two") == 2);
    ASSERT(m.at("three") == 3);
}

TEST(collect_piecewise) {
    struct Point {
        int x, y;
        Point(int a, int b) : x(a), y(b) {}
    };

    std::vector<std::pair<std::string, std::tuple<int, int>>> data = {
        {"origin", {0, 0}},
        {"unit", {1, 1}},
        {"other", {5, 10}},
    };
    auto m = imm::collect_piecewise<std::string, Point>(data);
    ASSERT(m.size() == 3);
    ASSERT(m.at("origin").x == 0);
    ASSERT(m.at("origin").y == 0);
    ASSERT(m.at("unit").x == 1);
    ASSERT(m.at("other").x == 5);
}

TEST(at_throws) {
    imm::immutable_map<int, int> m;
    bool threw = false;
    try {
        m.at(42);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    ASSERT(threw);
}

TEST(builder_reserve_twice_throws) {
    imm::immutable_map_builder<int, int> b;
    b.reserve(10);
    bool threw = false;
    try {
        b.reserve(20);
    } catch (const std::logic_error&) {
        threw = true;
    }
    ASSERT(threw);
}

TEST(builder_emplace_overflow_throws) {
    imm::immutable_map_builder<int, int> b;
    b.reserve(1);
    b.emplace(1, 10);
    bool threw = false;
    try {
        b.emplace(2, 20);
    } catch (const std::logic_error&) {
        threw = true;
    }
    ASSERT(threw);
}

TEST(large_map) {
    const int N = 10000;
    imm::immutable_map_builder<int, int> b;
    b.reserve(N);
    for (int i = 0; i < N; ++i) {
        b.emplace(i, i * 10);
    }
    auto m = b.build();

    ASSERT(m.size() == static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        ASSERT(m.at(i) == i * 10);
    }
}

TEST(iterator_traits) {
    imm::immutable_map_builder<int, int> b;
    b.reserve(2);
    b.emplace(1, 10);
    b.emplace(2, 20);
    auto m = b.build();

    auto it = m.begin();
    auto it2 = it;  // copyable
    ASSERT(it == it2);
    ++it;
    ASSERT(it != it2);

    // const_iterator from iterator
    imm::immutable_map<int, int>::const_iterator cit = m.begin();
    ASSERT(cit == m.cbegin());
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "Running immutable_map tests...\n";
    // Tests are auto-registered via static initialization
    std::cout << "All tests passed!\n";
    return 0;
}
