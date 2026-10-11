// immutable_map.hpp — C++17 immutable hash map
//
// Design:
//   - Keys immutable after construction, values mutable.
//   - Single allocation: [ pair<const Key,Value>[N] ][ uint32_t[M] ]
//   - Builder pattern: reserve → emplace → build.
//   - Value need NOT be movable or copyable.
//   - Copy ops enabled via CRTP base only when Value is copy-constructible.
//   - Heterogeneous lookup: find() is templated on CompatibleKey.
//     User provides Hash and Eq that work with both Key and CompatibleKey.
//     E.g. Hash=hash<string_view>, Eq=equal_to<string_view> works for
//     Key=string with find(string_view).
//   - Open addressing, linear probing, load factor ≤ 0.5.
//
// Memory layout:
//   [ std::pair<const Key, Value> [N] ]  [ padding ]  [ uint32_t [M] ]
//   where M = next_power_of_2(2*N), load factor ≤ 0.5
//
// Hash index stores uint32_t offsets into the pairs array,
// or UINT32_MAX for empty slots.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace imm {

// ============================================================================
// detail helpers
// ============================================================================
namespace detail {

constexpr std::size_t align_up(std::size_t n, std::size_t alignment) noexcept {
    return (n + alignment - 1) & ~(alignment - 1);
}

static constexpr std::uint32_t EMPTY_SLOT =
    std::numeric_limits<std::uint32_t>::max();

template <typename T>
struct is_pair_impl : std::false_type {};

template <typename A, typename B>
struct is_pair_impl<std::pair<A, B>> : std::true_type {};

template <typename T>
constexpr bool is_pair_v = is_pair_impl<std::decay_t<T>>::value;

} // namespace detail

// Forward declarations
template <typename Key, typename Value, typename Hash, typename KeyEq,
          typename Alloc>
class immutable_map;

template <typename Key, typename Value, typename Hash, typename KeyEq,
          typename Alloc>
class immutable_map_builder;

// ============================================================================
// map_storage — owns the single allocation, layout computation, lifetime
// ============================================================================
// Shared between immutable_map and immutable_map_builder.
// Contains: raw data pointer, element count, index size, allocator.
// Does NOT contain capacity (that's builder-only).
// ============================================================================

template <typename Key,
          typename Value,
          typename Alloc = std::allocator<std::byte>>
class map_storage {
protected:
    using pair_type     = std::pair<const Key, Value>;
    using byte_type     = std::byte;
    using byte_alloc    = typename std::allocator_traits<Alloc>::template
                              rebind_alloc<byte_type>;
    using byte_alloc_tr = std::allocator_traits<byte_alloc>;

    static constexpr std::uint32_t EMPTY = detail::EMPTY_SLOT;

    byte_type*  data_       = nullptr;
    std::size_t size_       = 0;
    std::size_t index_size_ = 0;
    byte_alloc  alloc_;

    // ----- layout computation (static, constexpr) -----
    static constexpr std::size_t pairs_byte_size(std::size_t n) noexcept {
        return n * sizeof(pair_type);
    }

    static constexpr std::size_t index_byte_offset(std::size_t n) noexcept {
        return detail::align_up(pairs_byte_size(n), alignof(std::uint32_t));
    }

    static constexpr std::size_t total_bytes(std::size_t n,
                                             std::size_t m) noexcept {
        return index_byte_offset(n) + m * sizeof(std::uint32_t);
    }

    static constexpr std::size_t compute_index_size(std::size_t n) noexcept {
        if (n == 0) return 0;
        std::size_t m = 1;
        while (m < 2 * n) m <<= 1;   // load factor ≤ 0.5
        return m;
    }

    // ----- typed accessors -----
    pair_type* pairs() noexcept {
        return reinterpret_cast<pair_type*>(data_);
    }
    const pair_type* pairs() const noexcept {
        return reinterpret_cast<const pair_type*>(data_);
    }

    std::uint32_t* index() noexcept {
        return reinterpret_cast<std::uint32_t*>(
            data_ + index_byte_offset(size_));
    }
    const std::uint32_t* index() const noexcept {
        return reinterpret_cast<const std::uint32_t*>(
            data_ + index_byte_offset(size_));
    }

    // ----- allocation / deallocation -----
    void allocate(std::size_t n, std::size_t m) {
        data_ = byte_alloc_tr::allocate(alloc_, total_bytes(n, m));
    }

    void deallocate() noexcept {
        if (data_) {
            byte_alloc_tr::deallocate(alloc_,
                                      total_bytes(size_, index_size_));
            data_ = nullptr;
        }
    }

    // ----- element lifetime -----
    void destroy_elements() noexcept {
        pair_type* p = pairs();
        for (std::size_t i = size_; i > 0; --i) {
            p[i - 1].~pair_type();
        }
    }

    void destroy_all() noexcept {
        if (!data_) return;
        destroy_elements();
        deallocate();
        size_       = 0;
        index_size_ = 0;
    }

    // ----- index building -----
    void build_index(Hash hash_fn) noexcept {
        std::uint32_t* ix = index();
        const std::size_t mask = index_size_ - 1;
        for (std::size_t i = 0; i < index_size_; ++i)
            ix[i] = EMPTY;

        pair_type* p = pairs();
        for (std::size_t i = 0; i < size_; ++i) {
            std::size_t slot = hash_fn(p[i].first) & mask;
            while (ix[slot] != EMPTY)
                slot = (slot + 1) & mask;
            ix[slot] = static_cast<std::uint32_t>(i);
        }
    }

    // ----- lookup -----
    template <typename CompatibleKey>
    std::size_t find_slot(const CompatibleKey& key,
                          Hash hash_fn, KeyEq eq_fn) const noexcept {
        if (size_ == 0) return static_cast<std::size_t>(-1);
        const std::uint32_t* ix = index();
        const std::size_t mask = index_size_ - 1;
        const pair_type* p = pairs();
        std::size_t slot = hash_fn(key) & mask;
        while (ix[slot] != EMPTY) {
            const std::size_t idx = ix[slot];
            if (eq_fn(p[idx].first, key))
                return idx;
            slot = (slot + 1) & mask;
        }
        return static_cast<std::size_t>(-1);
    }

    // ----- constructors -----
    map_storage() noexcept = default;

    explicit map_storage(const Alloc& alloc) noexcept
        : alloc_(alloc)
    {}

    // Move
    map_storage(map_storage&& other) noexcept
        : data_(other.data_)
        , size_(other.size_)
        , index_size_(other.index_size_)
        , alloc_(std::move(other.alloc_))
    {
        other.data_       = nullptr;
        other.size_       = 0;
        other.index_size_ = 0;
    }

    map_storage& operator=(map_storage&& other) noexcept {
        if (this != &other) {
            destroy_all();
            data_       = other.data_;
            size_       = other.size_;
            index_size_ = other.index_size_;
            alloc_      = std::move(other.alloc_);
            other.data_       = nullptr;
            other.size_       = 0;
            other.index_size_ = 0;
        }
        return *this;
    }

    ~map_storage() { destroy_all(); }

    // Adopt raw allocation (used by builder→map transfer)
    struct adopt_tag {};

    map_storage(adopt_tag, byte_type* data, std::size_t sz,
                std::size_t idx_sz, byte_alloc a) noexcept
        : data_(data)
        , size_(sz)
        , index_size_(idx_sz)
        , alloc_(std::move(a))
    {}

    // Grant access to builder and copy_support_base
    friend class immutable_map_builder<Key, Value, Hash, KeyEq, Alloc>;
    template <typename, typename, bool> friend struct copy_support_base;
};

// ============================================================================
// copy_support_base — CRTP base for conditional copy operations
// ============================================================================
// When Value is copy-constructible, provides copy ctor and copy assignment.
// When not, copy ops are implicitly deleted (no overloads generated).
// This avoids GCC8 issues with SFINAE on non-template operator=.
// ============================================================================

template <typename Derived, typename Value, bool = std::is_copy_constructible_v<Value>>
struct copy_support_base {
    // Value is NOT copy-constructible: copy ops are implicitly deleted.
    // Nothing to do here.
};

template <typename Derived, typename Value>
struct copy_support_base<Derived, Value, true> {
protected:
    // Called by Derived's copy constructor
    void copy_from(const Derived& other) {
        auto& self = static_cast<Derived&>(*this);
        using storage_t = map_storage<typename Derived::key_type,
                                      typename Derived::mapped_type,
                                      typename Derived::allocator_type>;
        if (other.size() > 0) {
            const std::size_t n = other.size();
            const std::size_t m = storage_t::compute_index_size(n);
            self.allocate(n, m);
            self.size_       = n;
            self.index_size_ = m;

            // Copy pairs
            auto* dst = self.pairs();
            const auto* src = other.pairs();
            for (std::size_t i = 0; i < n; ++i) {
                ::new (static_cast<void*>(dst + i))
                    std::pair<const typename Derived::key_type,
                              typename Derived::mapped_type>(
                        src[i].first, src[i].second);
            }
            // Copy index
            std::memcpy(self.index(), other.index(),
                        m * sizeof(std::uint32_t));
        }
    }

public:
    Derived& copy_assign(Derived& self, const Derived& other) {
        if (&self != &other) {
            self.destroy_all();
            self.alloc_ = other.alloc_;
            copy_from(other);
        }
        return self;
    }
};

// ============================================================================
// immutable_map
// ============================================================================

template <typename Key,
          typename Value,
          typename Hash   = std::hash<Key>,
          typename KeyEq  = std::equal_to<Key>,
          typename Alloc  = std::allocator<std::byte>>
class immutable_map
    : private map_storage<Key, Value, Alloc>,
      private copy_support_base<
          immutable_map<Key, Value, Hash, KeyEq, Alloc>, Value>
{
    using storage_t = map_storage<Key, Value, Alloc>;
    using copy_base_t = copy_support_base<immutable_map, Value>;
    friend copy_base_t;

public:
    // ----- type aliases -----
    using key_type        = Key;
    using mapped_type     = Value;
    using value_type      = std::pair<const Key, Value>;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher          = Hash;
    using key_equal       = KeyEq;
    using allocator_type  = Alloc;
    using reference       = value_type&;
    using const_reference = const value_type&;

private:
    using typename storage_t::byte_type;
    using typename storage_t::byte_alloc;
    using typename storage_t::byte_alloc_tr;
    using typename storage_t::pair_type;

public:
    // ========================================================================
    // Constructors / assignment / destructor
    // ========================================================================

    immutable_map() noexcept = default;

    // Move (inherited from storage, but we expose it explicitly)
    immutable_map(immutable_map&& other) noexcept
        : storage_t(std::move(other))
    {}

    immutable_map& operator=(immutable_map&& other) noexcept {
        storage_t::operator=(std::move(other));
        return *this;
    }

    // Copy — only available when Value is copy-constructible
    // (enabled by copy_support_base)
    immutable_map(const immutable_map& other)
        : storage_t(other.alloc_)
    {
        copy_base_t::copy_from(other);
    }

    immutable_map& operator=(const immutable_map& other) {
        return copy_base_t::copy_assign(*this, other);
    }

    ~immutable_map() = default;  // storage handles cleanup

    // ========================================================================
    // Capacity
    // ========================================================================

    size_type size()  const noexcept { return storage_t::size_; }
    bool      empty() const noexcept { return storage_t::size_ == 0; }

    // ========================================================================
    // Lookup — heterogeneous: accepts any CompatibleKey
    // ========================================================================
    // User must ensure Hash and KeyEq work with CompatibleKey.
    // E.g. Key=string, Hash=hash<string_view>, KeyEq=equal_to<string_view>
    // allows find(string_view) without constructing a string.
    // ========================================================================

    template <typename CompatibleKey>
    value_type* find(const CompatibleKey& key) noexcept {
        size_type s = storage_t::find_slot(key, Hash{}, KeyEq{});
        return s == static_cast<size_type>(-1)
            ? nullptr
            : &storage_t::pairs()[s];
    }

    template <typename CompatibleKey>
    const value_type* find(const CompatibleKey& key) const noexcept {
        size_type s = storage_t::find_slot(key, Hash{}, KeyEq{});
        return s == static_cast<size_type>(-1)
            ? nullptr
            : &storage_t::pairs()[s];
    }

    template <typename CompatibleKey>
    Value& at(const CompatibleKey& key) {
        value_type* p = find(key);
        if (!p) throw std::out_of_range("immutable_map::at: key not found");
        return p->second;
    }

    template <typename CompatibleKey>
    const Value& at(const CompatibleKey& key) const {
        const value_type* p = find(key);
        if (!p) throw std::out_of_range("immutable_map::at: key not found");
        return p->second;
    }

    template <typename CompatibleKey>
    bool contains(const CompatibleKey& key) const noexcept {
        return storage_t::find_slot(key, Hash{}, KeyEq{})
               != static_cast<size_type>(-1);
    }

    // ========================================================================
    // Iterators — LegacyForwardIterator over value_type&
    // ========================================================================

    template <bool IsConst>
    class basic_iterator {
        friend class immutable_map;

        using ptr = std::conditional_t<IsConst, const value_type*, value_type*>;
        ptr     p_;
        size_type pos_;  // for operator- between iterators

        basic_iterator(ptr p, size_type pos) noexcept
            : p_(p), pos_(pos) {}

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = immutable_map::value_type;
        using difference_type   = std::ptrdiff_t;
        using reference         = std::conditional_t<IsConst,
                                                     const value_type&,
                                                     value_type&>;
        using pointer           = std::conditional_t<IsConst,
                                                     const value_type*,
                                                     value_type*>;

        reference operator*() const noexcept { return *p_; }
        pointer   operator->() const noexcept { return p_; }

        basic_iterator& operator++() noexcept { ++p_; ++pos_; return *this; }
        basic_iterator  operator++(int) noexcept {
            auto tmp = *this; ++p_; ++pos_; return tmp;
        }

        bool operator==(const basic_iterator& o) const noexcept {
            return pos_ == o.pos_;
        }
        bool operator!=(const basic_iterator& o) const noexcept {
            return pos_ != o.pos_;
        }

        // Allow conversion iterator → const_iterator
        template <bool Other,
                  std::enable_if_t<IsConst && !Other, int> = 0>
        basic_iterator(const basic_iterator<Other>& o) noexcept
            : p_(o.p_), pos_(o.pos_) {}
    };

    using iterator       = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

    iterator       begin()        noexcept { return iterator(storage_t::pairs(), 0); }
    iterator       end()          noexcept { return iterator(storage_t::pairs() + storage_t::size_, storage_t::size_); }
    const_iterator begin()  const noexcept { return const_iterator(storage_t::pairs(), 0); }
    const_iterator end()    const noexcept { return const_iterator(storage_t::pairs() + storage_t::size_, storage_t::size_); }
    const_iterator cbegin() const noexcept { return const_iterator(storage_t::pairs(), 0); }
    const_iterator cend()   const noexcept { return const_iterator(storage_t::pairs() + storage_t::size_, storage_t::size_); }
};

// ============================================================================
// immutable_map_builder
// ============================================================================

template <typename Key,
          typename Value,
          typename Hash   = std::hash<Key>,
          typename KeyEq  = std::equal_to<Key>,
          typename Alloc  = std::allocator<std::byte>>
class immutable_map_builder {
public:
    using map_type    = immutable_map<Key, Value, Hash, KeyEq, Alloc>;
    using value_type  = std::pair<const Key, Value>;
    using size_type   = std::size_t;

private:
    using storage_t   = map_storage<Key, Value, Alloc>;
    using byte_type   = std::byte;
    using byte_alloc  = typename storage_t::byte_alloc;
    using byte_alloc_tr = typename storage_t::byte_alloc_tr;

    storage_t   storage_;
    size_type   capacity_ = 0;
    size_type   size_     = 0;

    value_type* pairs() noexcept { return storage_.pairs(); }

public:
    immutable_map_builder() noexcept = default;

    explicit immutable_map_builder(const Alloc& alloc) noexcept
        : storage_(alloc)
    {}

    // Non-copyable
    immutable_map_builder(const immutable_map_builder&) = delete;
    immutable_map_builder& operator=(const immutable_map_builder&) = delete;

    // Movable
    immutable_map_builder(immutable_map_builder&& other) noexcept
        : storage_(std::move(other.storage_))
        , capacity_(other.capacity_)
        , size_(other.size_)
    {
        other.capacity_ = 0;
        other.size_     = 0;
    }

    immutable_map_builder& operator=(immutable_map_builder&& other) noexcept {
        if (this != &other) {
            storage_.destroy_all();
            storage_  = std::move(other.storage_);
            capacity_ = other.capacity_;
            size_     = other.size_;
            other.capacity_ = 0;
            other.size_     = 0;
        }
        return *this;
    }

    ~immutable_map_builder() = default;

    /// Reserve space for n elements. Allocates the final memory block.
    /// Must be called exactly once, before any emplace().
    void reserve(size_type n) {
        if (storage_.data_) {
            throw std::logic_error(
                "immutable_map_builder::reserve called after allocation");
        }
        capacity_ = n;
        storage_.index_size_ = storage_t::compute_index_size(n);
        if (n == 0) return;
        storage_.allocate(n, storage_.index_size_);
    }

    /// Construct a key-value pair in-place.
    /// Key is move-constructed (or copy-constructed from lvalue).
    /// Value is constructed from forwarded args.
    template <typename K, typename... Args>
    void emplace(K&& key, Args&&... args) {
        if (size_ >= capacity_) {
            throw std::logic_error(
                "immutable_map_builder::emplace: capacity exceeded, "
                "call reserve() first");
        }
        ::new (static_cast<void*>(pairs() + size_))
            value_type(std::piecewise_construct,
                       std::forward_as_tuple(std::forward<K>(key)),
                       std::forward_as_tuple(std::forward<Args>(args)...));
        ++size_;
    }

    /// Current number of elements inserted.
    size_type size() const noexcept { return size_; }

    /// Finalize: build hash index and transfer ownership to immutable_map.
    /// After build(), this builder is empty.
    map_type build() {
        if (!storage_.data_ && capacity_ > 0) {
            throw std::logic_error(
                "immutable_map_builder::build: reserve() was not called");
        }

        // Build hash index
        storage_.size_ = size_;
        if (size_ > 0) {
            storage_.build_index(Hash{});
        }

        // Transfer ownership to immutable_map
        map_type result(typename storage_t::adopt_tag{},
                        storage_.data_, size_, storage_.index_size_,
                        std::move(storage_.alloc_));
        storage_.data_       = nullptr;
        storage_.size_       = 0;
        storage_.index_size_ = 0;
        capacity_ = 0;
        size_     = 0;
        return result;
    }
};

// ============================================================================
// collect — construct immutable_map from a forward-iterator range
// ============================================================================
// Overload 1: range of pair-like { .first → Key, .second → Value }
// Overload 2: range of pair-like { .first → Key, .second → tuple<Args...> }
//             (Value is constructed from unpacked tuple)
// ============================================================================

namespace detail {

template <typename T>
struct is_tuple_impl : std::false_type {};

template <typename... Args>
struct is_tuple_impl<std::tuple<Args...>> : std::true_type {};

template <typename T>
constexpr bool is_tuple_v = is_tuple_impl<std::decay_t<T>>::value;

} // namespace detail

/// Collect from range of {key, value} pairs.
/// If .second is a tuple, Value is constructed from unpacked tuple elements.
template <typename Key, typename Value,
          typename Hash  = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>,
          typename Alloc = std::allocator<std::byte>,
          typename ForwardIt>
immutable_map<Key, Value, Hash, KeyEq, Alloc>
collect(ForwardIt first, ForwardIt last) {
    using ref_t    = decltype(*first);
    using second_t = std::decay_t<decltype(std::declval<ref_t>().second)>;

    const auto n = static_cast<std::size_t>(std::distance(first, last));
    immutable_map_builder<Key, Value, Hash, KeyEq, Alloc> builder;
    builder.reserve(n);

    if constexpr (detail::is_tuple_v<second_t>) {
        for (; first != last; ++first) {
            std::apply(
                [&](auto&&... args) {
                    builder.emplace(Key((*first).first),
                                    std::forward<decltype(args)>(args)...);
                },
                (*first).second);
        }
    } else {
        for (; first != last; ++first) {
            builder.emplace(Key((*first).first), (*first).second);
        }
    }

    return builder.build();
}

/// Convenience: collect from a range object (has begin/end).
template <typename Key, typename Value,
          typename Hash  = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>,
          typename Alloc = std::allocator<std::byte>,
          typename Range>
auto collect(const Range& range)
    -> decltype(collect<Key, Value, Hash, KeyEq, Alloc>(
            std::begin(range), std::end(range)))
{
    return collect<Key, Value, Hash, KeyEq, Alloc>(
        std::begin(range), std::end(range));
}

/// Collect from range of {key, tuple<Args...>} pairs.
/// Value is always constructed via std::apply from the tuple.
template <typename Key, typename Value,
          typename Hash  = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>,
          typename Alloc = std::allocator<std::byte>,
          typename ForwardIt>
immutable_map<Key, Value, Hash, KeyEq, Alloc>
collect_construct(ForwardIt first, ForwardIt last) {
    const auto n = static_cast<std::size_t>(std::distance(first, last));
    immutable_map_builder<Key, Value, Hash, KeyEq, Alloc> builder;
    builder.reserve(n);

    for (; first != last; ++first) {
        std::apply(
            [&](auto&&... args) {
                builder.emplace(Key((*first).first),
                                std::forward<decltype(args)>(args)...);
            },
            (*first).second);
    }

    return builder.build();
}

/// Convenience: collect_construct from a range object.
template <typename Key, typename Value,
          typename Hash  = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>,
          typename Alloc = std::allocator<std::byte>,
          typename Range>
auto collect_construct(const Range& range)
    -> decltype(collect_construct<Key, Value, Hash, KeyEq, Alloc>(
            std::begin(range), std::end(range)))
{
    return collect_construct<Key, Value, Hash, KeyEq, Alloc>(
        std::begin(range), std::end(range));
}

} // namespace imm
