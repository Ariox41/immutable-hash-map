// immutable_map.hpp — C++17 immutable hash map
// Keys are immutable after construction; values are mutable.
// Single allocation for data + hash index via user-provided allocator.
// Builder pattern: reserve → emplace → build.

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
#include <tuple>
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
struct is_tuple_impl : std::false_type {};

template <typename... Args>
struct is_tuple_impl<std::tuple<Args...>> : std::true_type {};

template <typename T>
constexpr bool is_tuple_v = is_tuple_impl<std::decay_t<T>>::value;

} // namespace detail

// Forward declarations
template <typename Key, typename Value, typename Hash, typename KeyEq,
          typename Alloc>
class immutable_map;

template <typename Key, typename Value, typename Hash, typename KeyEq,
          typename Alloc>
class immutable_map_builder;

// ============================================================================
// immutable_map
// ============================================================================
// Memory layout (single allocation):
//   [ Key[size_] ] [ padding ] [ Value[size_] ] [ padding ] [ uint32_t[index_size_] ]
//
// Hash table: open addressing, linear probing, load factor ≤ 0.5.
// Index slots store uint32_t offsets into the key/value arrays,
// or EMPTY_SLOT for vacant positions.
// ============================================================================

template <typename Key,
          typename Value,
          typename Hash   = std::hash<Key>,
          typename KeyEq  = std::equal_to<Key>,
          typename Alloc  = std::allocator<std::byte>>
class immutable_map {
public:
    // ----- type aliases -----
    using key_type        = Key;
    using mapped_type     = Value;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher          = Hash;
    using key_equal       = KeyEq;
    using allocator_type  = Alloc;

private:
    using byte_type       = std::byte;
    using byte_alloc      = typename std::allocator_traits<Alloc>::template
                                rebind_alloc<byte_type>;
    using byte_alloc_tr   = std::allocator_traits<byte_alloc>;

    static constexpr std::uint32_t EMPTY = detail::EMPTY_SLOT;

    // ----- data members -----
    byte_type*  data_      = nullptr;
    size_type   size_      = 0;
    size_type   index_size_= 0;   // always a power of two (or 0)
    size_type   mask_      = 0;   // index_size_ - 1
    byte_alloc  alloc_;

    // ----- layout helpers (static, constexpr) -----
    static constexpr size_type keys_offset() noexcept { return 0; }

    static constexpr size_type values_offset(size_type n) noexcept {
        return detail::align_up(n * sizeof(Key), alignof(Value));
    }

    static constexpr size_type index_offset(size_type n) noexcept {
        return detail::align_up(values_offset(n) + n * sizeof(Value),
                                alignof(std::uint32_t));
    }

    static constexpr size_type total_bytes(size_type n, size_type m) noexcept {
        return index_offset(n) + m * sizeof(std::uint32_t);
    }

    static constexpr size_type compute_index_size(size_type n) noexcept {
        if (n == 0) return 0;
        size_type m = 1;
        while (m < 2 * n) m <<= 1;   // load factor ≤ 0.5
        return m;
    }

    // ----- typed pointer accessors -----
    Key* keys() noexcept {
        return reinterpret_cast<Key*>(data_ + keys_offset());
    }
    const Key* keys() const noexcept {
        return reinterpret_cast<const Key*>(data_ + keys_offset());
    }

    Value* vals() noexcept {
        return reinterpret_cast<Value*>(data_ + values_offset(size_));
    }
    const Value* vals() const noexcept {
        return reinterpret_cast<const Value*>(data_ + values_offset(size_));
    }

    std::uint32_t* idx() noexcept {
        return reinterpret_cast<std::uint32_t*>(data_ + index_offset(size_));
    }
    const std::uint32_t* idx() const noexcept {
        return reinterpret_cast<const std::uint32_t*>(data_ + index_offset(size_));
    }

    // ----- internal helpers -----
    void destroy_contents() noexcept {
        if (!data_) return;
        // Destroy in reverse order
        for (size_type i = size_; i > 0; --i) {
            vals()[i - 1].~Value();
            keys()[i - 1].~Key();
        }
        byte_alloc_tr::deallocate(alloc_, data_,
                                  total_bytes(size_, index_size_));
        data_       = nullptr;
        size_       = 0;
        index_size_ = 0;
        mask_       = 0;
    }

    void build_index() noexcept {
        std::uint32_t* ix = idx();
        for (size_type i = 0; i < index_size_; ++i)
            ix[i] = EMPTY;

        Hash hash_fn;
        for (size_type i = 0; i < size_; ++i) {
            size_type slot = hash_fn(keys()[i]) & mask_;
            while (ix[slot] != EMPTY)
                slot = (slot + 1) & mask_;
            ix[slot] = static_cast<std::uint32_t>(i);
        }
    }

    size_type find_slot(const Key& key) const noexcept {
        if (size_ == 0) return static_cast<size_type>(-1);
        const std::uint32_t* ix = idx();
        Hash    hash_fn;
        KeyEq   eq_fn;
        size_type slot = hash_fn(key) & mask_;
        while (ix[slot] != EMPTY) {
            if (eq_fn(keys()[ix[slot]], key))
                return ix[slot];
            slot = (slot + 1) & mask_;
        }
        return static_cast<size_type>(-1);
    }

    // ----- adopted constructor (used by builder) -----
    struct adopt_tag {};

    immutable_map(adopt_tag, byte_type* data, size_type sz,
                  size_type idx_sz, byte_alloc a) noexcept
        : data_(data)
        , size_(sz)
        , index_size_(idx_sz)
        , mask_(idx_sz > 0 ? idx_sz - 1 : 0)
        , alloc_(std::move(a))
    {}

    // Grant builder access to private constructor and layout helpers
    friend class immutable_map_builder<Key, Value, Hash, KeyEq, Alloc>;

public:
    // ========================================================================
    // Constructors / assignment / destructor
    // ========================================================================

    immutable_map() noexcept = default;

    // Move
    immutable_map(immutable_map&& other) noexcept
        : data_(other.data_)
        , size_(other.size_)
        , index_size_(other.index_size_)
        , mask_(other.mask_)
        , alloc_(std::move(other.alloc_))
    {
        other.data_       = nullptr;
        other.size_       = 0;
        other.index_size_ = 0;
        other.mask_       = 0;
    }

    immutable_map& operator=(immutable_map&& other) noexcept {
        if (this != &other) {
            destroy_contents();
            data_       = other.data_;
            size_       = other.size_;
            index_size_ = other.index_size_;
            mask_       = other.mask_;
            alloc_      = std::move(other.alloc_);
            other.data_       = nullptr;
            other.size_       = 0;
            other.index_size_ = 0;
            other.mask_       = 0;
        }
        return *this;
    }

    // Copy — enabled only when Value is copy-constructible
    template <typename V = Value,
              std::enable_if_t<std::is_copy_constructible_v<V>, int> = 0>
    immutable_map(const immutable_map& other)
        : alloc_(other.alloc_)
    {
        if (other.size_ > 0) {
            const size_type n = other.size_;
            const size_type m = other.index_size_;
            data_ = byte_alloc_tr::allocate(alloc_, total_bytes(n, m));
            size_       = n;
            index_size_ = m;
            mask_       = m - 1;

            for (size_type i = 0; i < n; ++i) {
                ::new (static_cast<void*>(keys() + i))
                    Key(other.keys()[i]);
                ::new (static_cast<void*>(vals() + i))
                    Value(other.vals()[i]);
            }
            std::memcpy(idx(), other.idx(), m * sizeof(std::uint32_t));
        }
    }

    template <typename V = Value,
              std::enable_if_t<std::is_copy_constructible_v<V>, int> = 0>
    immutable_map& operator=(const immutable_map& other) {
        if (this != &other) {
            destroy_contents();
            alloc_ = other.alloc_;
            if (other.size_ > 0) {
                const size_type n = other.size_;
                const size_type m = other.index_size_;
                data_ = byte_alloc_tr::allocate(alloc_, total_bytes(n, m));
                size_       = n;
                index_size_ = m;
                mask_       = m - 1;

                for (size_type i = 0; i < n; ++i) {
                    ::new (static_cast<void*>(keys() + i))
                        Key(other.keys()[i]);
                    ::new (static_cast<void*>(vals() + i))
                        Value(other.vals()[i]);
                }
                std::memcpy(idx(), other.idx(),
                            m * sizeof(std::uint32_t));
            }
        }
        return *this;
    }

    ~immutable_map() { destroy_contents(); }

    // ========================================================================
    // Capacity
    // ========================================================================

    size_type size()  const noexcept { return size_; }
    bool      empty() const noexcept { return size_ == 0; }

    // ========================================================================
    // Lookup
    // ========================================================================

    /// Returns pointer to value, or nullptr if not found.
    Value* find(const Key& key) noexcept {
        size_type s = find_slot(key);
        return s == static_cast<size_type>(-1) ? nullptr : &vals()[s];
    }

    const Value* find(const Key& key) const noexcept {
        size_type s = find_slot(key);
        return s == static_cast<size_type>(-1) ? nullptr : &vals()[s];
    }

    /// Returns reference to value; throws std::out_of_range if not found.
    Value& at(const Key& key) {
        Value* p = find(key);
        if (!p) throw std::out_of_range("immutable_map::at: key not found");
        return *p;
    }

    const Value& at(const Key& key) const {
        const Value* p = find(key);
        if (!p) throw std::out_of_range("immutable_map::at: key not found");
        return *p;
    }

    /// Returns true if key is present.
    bool contains(const Key& key) const noexcept {
        return find_slot(key) != static_cast<size_type>(-1);
    }

    // ========================================================================
    // Iterators
    // ========================================================================
    // Proxy reference: { const Key& key, Value& value }
    // Supports structured bindings and range-for.
    // ========================================================================

    template <bool IsConst>
    class basic_iterator {
        friend class immutable_map;

        using map_ptr = std::conditional_t<IsConst,
                                           const immutable_map*,
                                           immutable_map*>;
        map_ptr   map_;
        size_type pos_;

        basic_iterator(map_ptr m, size_type p) noexcept
            : map_(m), pos_(p) {}

    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;

        struct reference {
            const Key& key;
            std::conditional_t<IsConst, const Value&, Value&> value;
        };

        using value_type = reference;
        using pointer    = void;

        reference operator*() const noexcept {
            return { map_->keys()[pos_], map_->vals()[pos_] };
        }

        basic_iterator& operator++() noexcept { ++pos_; return *this; }
        basic_iterator  operator++(int) noexcept {
            auto tmp = *this; ++pos_; return tmp;
        }

        basic_iterator& operator--() noexcept { --pos_; return *this; }
        basic_iterator  operator--(int) noexcept {
            auto tmp = *this; --pos_; return tmp;
        }

        basic_iterator& operator+=(difference_type n) noexcept {
            pos_ = static_cast<size_type>(
                static_cast<difference_type>(pos_) + n);
            return *this;
        }

        basic_iterator operator+(difference_type n) const noexcept {
            auto tmp = *this; tmp += n; return tmp;
        }

        basic_iterator operator-(difference_type n) const noexcept {
            auto tmp = *this; tmp -= n; return tmp;
        }

        difference_type operator-(const basic_iterator& o) const noexcept {
            return static_cast<difference_type>(pos_) -
                   static_cast<difference_type>(o.pos_);
        }

        reference operator[](difference_type n) const noexcept {
            return *(*this + n);
        }

        bool operator==(const basic_iterator& o) const noexcept {
            return pos_ == o.pos_;
        }
        bool operator!=(const basic_iterator& o) const noexcept {
            return pos_ != o.pos_;
        }
        bool operator<(const basic_iterator& o) const noexcept {
            return pos_ < o.pos_;
        }
        bool operator<=(const basic_iterator& o) const noexcept {
            return pos_ <= o.pos_;
        }
        bool operator>(const basic_iterator& o) const noexcept {
            return pos_ > o.pos_;
        }
        bool operator>=(const basic_iterator& o) const noexcept {
            return pos_ >= o.pos_;
        }

        // Allow conversion from iterator to const_iterator
        template <bool Other,
                  std::enable_if_t<IsConst && !Other, int> = 0>
        basic_iterator(const basic_iterator<Other>& o) noexcept
            : map_(o.map_), pos_(o.pos_) {}
    };

    using iterator       = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

    iterator       begin()        noexcept { return iterator(this, 0); }
    iterator       end()          noexcept { return iterator(this, size_); }
    const_iterator begin()  const noexcept { return const_iterator(this, 0); }
    const_iterator end()    const noexcept { return const_iterator(this, size_); }
    const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
    const_iterator cend()   const noexcept { return const_iterator(this, size_); }

    // ========================================================================
    // Direct data access (for advanced use)
    // ========================================================================

    const Key*   data_keys()   const noexcept { return keys(); }
    Value*       data_values() noexcept       { return vals(); }
    const Value* data_values() const noexcept { return vals(); }
};

// ============================================================================
// immutable_map_builder
// ============================================================================
// Usage:
//   immutable_map_builder<K,V> b;
//   b.reserve(n);
//   b.emplace(key1, arg1, arg2, ...);
//   b.emplace(key2, ...);
//   auto m = b.build();
//
// Memory is allocated once in reserve(). Objects are constructed in-place.
// Index is built in build(). Ownership is transferred to immutable_map.
// ============================================================================

template <typename Key,
          typename Value,
          typename Hash   = std::hash<Key>,
          typename KeyEq  = std::equal_to<Key>,
          typename Alloc  = std::allocator<std::byte>>
class immutable_map_builder {
public:
    using map_type    = immutable_map<Key, Value, Hash, KeyEq, Alloc>;
    using size_type   = std::size_t;

private:
    using byte_type     = std::byte;
    using byte_alloc    = typename std::allocator_traits<Alloc>::template
                              rebind_alloc<byte_type>;
    using byte_alloc_tr = std::allocator_traits<byte_alloc>;

    byte_type*  data_       = nullptr;
    size_type   capacity_   = 0;
    size_type   size_       = 0;
    size_type   index_size_ = 0;
    byte_alloc  alloc_;

    // Layout helpers (duplicated from immutable_map for constexpr access)
    static constexpr size_type keys_offset() noexcept { return 0; }

    static constexpr size_type values_offset(size_type n) noexcept {
        return detail::align_up(n * sizeof(Key), alignof(Value));
    }

    static constexpr size_type index_offset(size_type n) noexcept {
        return detail::align_up(values_offset(n) + n * sizeof(Value),
                                alignof(std::uint32_t));
    }

    static constexpr size_type total_bytes(size_type n, size_type m) noexcept {
        return index_offset(n) + m * sizeof(std::uint32_t);
    }

    static constexpr size_type compute_index_size(size_type n) noexcept {
        if (n == 0) return 0;
        size_type m = 1;
        while (m < 2 * n) m <<= 1;
        return m;
    }

    Key* keys() noexcept {
        return reinterpret_cast<Key*>(data_ + keys_offset());
    }
    Value* vals() noexcept {
        return reinterpret_cast<Value*>(data_ + values_offset(capacity_));
    }
    std::uint32_t* idx() noexcept {
        return reinterpret_cast<std::uint32_t*>(
            data_ + index_offset(capacity_));
    }

    void destroy_contents() noexcept {
        if (!data_) return;
        for (size_type i = size_; i > 0; --i) {
            vals()[i - 1].~Value();
            keys()[i - 1].~Key();
        }
        byte_alloc_tr::deallocate(alloc_,
                                  total_bytes(capacity_, index_size_));
        data_       = nullptr;
        capacity_   = 0;
        size_       = 0;
        index_size_ = 0;
    }

public:
    immutable_map_builder() noexcept = default;

    explicit immutable_map_builder(const Alloc& alloc) noexcept
        : alloc_(alloc)
    {}

    // Non-copyable
    immutable_map_builder(const immutable_map_builder&) = delete;
    immutable_map_builder& operator=(const immutable_map_builder&) = delete;

    // Movable
    immutable_map_builder(immutable_map_builder&& other) noexcept
        : data_(other.data_)
        , capacity_(other.capacity_)
        , size_(other.size_)
        , index_size_(other.index_size_)
        , alloc_(std::move(other.alloc_))
    {
        other.data_       = nullptr;
        other.capacity_   = 0;
        other.size_       = 0;
        other.index_size_ = 0;
    }

    immutable_map_builder& operator=(immutable_map_builder&& other) noexcept {
        if (this != &other) {
            destroy_contents();
            data_       = other.data_;
            capacity_   = other.capacity_;
            size_       = other.size_;
            index_size_ = other.index_size_;
            alloc_      = std::move(other.alloc_);
            other.data_       = nullptr;
            other.capacity_   = 0;
            other.size_       = 0;
            other.index_size_ = 0;
        }
        return *this;
    }

    ~immutable_map_builder() { destroy_contents(); }

    /// Reserve space for n elements. Allocates the final memory block.
    /// Must be called before any emplace(). Calling more than once is UB.
    void reserve(size_type n) {
        if (data_) {
            throw std::logic_error(
                "immutable_map_builder::reserve called after allocation");
        }
        capacity_   = n;
        index_size_ = compute_index_size(n);
        if (n == 0) return;
        data_ = byte_alloc_tr::allocate(alloc_, total_bytes(n, index_size_));
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
        ::new (static_cast<void*>(keys() + size_))
            Key(std::forward<K>(key));
        ::new (static_cast<void*>(vals() + size_))
            Value(std::forward<Args>(args)...);
        ++size_;
    }

    /// Current number of elements inserted.
    size_type size() const noexcept { return size_; }

    /// Finalize: build hash index and transfer ownership to immutable_map.
    /// After build(), this builder is empty.
    map_type build() {
        if (!data_ && capacity_ > 0) {
            throw std::logic_error(
                "immutable_map_builder::build: reserve() was not called");
        }

        // Build hash index
        if (size_ > 0) {
            std::uint32_t* ix = idx();
            for (size_type i = 0; i < index_size_; ++i)
                ix[i] = detail::EMPTY_SLOT;

            Hash hash_fn;
            for (size_type i = 0; i < size_; ++i) {
                size_type slot = hash_fn(keys()[i]) & (index_size_ - 1);
                while (ix[slot] != detail::EMPTY_SLOT)
                    slot = (slot + 1) & (index_size_ - 1);
                ix[slot] = static_cast<std::uint32_t>(i);
            }
        }

        // Transfer ownership
        map_type result(typename map_type::adopt_tag{},
                        data_, size_, index_size_, std::move(alloc_));
        data_       = nullptr;
        size_       = 0;
        capacity_   = 0;
        index_size_ = 0;
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

/// Collect from range of {key, value} pairs.
template <typename Key, typename Value,
          typename Hash  = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>,
          typename Alloc = std::allocator<std::byte>,
          typename ForwardIt>
immutable_map<Key, Value, Hash, KeyEq, Alloc>
collect(ForwardIt first, ForwardIt last) {
    using ref_t = decltype(*first);
    using second_t = std::decay_t<decltype(std::declval<ref_t>().second)>;

    const auto n = static_cast<std::size_t>(std::distance(first, last));
    immutable_map_builder<Key, Value, Hash, KeyEq, Alloc> builder;
    builder.reserve(n);

    if constexpr (detail::is_tuple_v<second_t>) {
        // second is a tuple → unpack into Value constructor
        for (; first != last; ++first) {
            std::apply(
                [&](auto&&... args) {
                    builder.emplace(Key((*first).first),
                                    std::forward<decltype(args)>(args)...);
                },
                (*first).second);
        }
    } else {
        // second is directly a Value
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
/// Value is constructed via std::apply from the tuple.
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
