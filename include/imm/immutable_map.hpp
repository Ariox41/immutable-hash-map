// immutable_map.hpp — C++17 immutable hash map
//
// Design:
//   - Keys immutable after construction, values mutable.
//   - Single allocation: [ pair<const Key,Value>[N] ][ uint32_t[M] ]
//   - Builder pattern: reserve → emplace → build.
//   - Value need NOT be movable or copyable.
//   - Copy ops enabled via CRTP base only when Value is copy-constructible.
//   - Heterogeneous lookup: find() is templated on CompatibleKey.
//   - Open addressing, linear probing, load factor ≤ 0.5.
//   - Allocator propagated via allocator_traits; EBO via private inheritance.
//   - Proper alignment via align_block; std::launder for strict aliasing.
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

} // namespace detail

// Forward declarations
template <typename Key, typename Value, typename Hash, typename KeyEq,
          typename Alloc>
class immutable_map;

template <typename Key, typename Value, typename Hash, typename KeyEq,
          typename Alloc>
class immutable_map_builder;

template <typename Derived, typename Value, bool>
struct copy_support_base;

// ============================================================================
// map_storage — owns the single allocation, layout computation, lifetime
// ============================================================================

template <typename Key,
          typename Value,
          typename Alloc = std::allocator<std::byte>>
class map_storage : private Alloc {
protected:
    using pair_type       = std::pair<const Key, Value>;
    using byte_type       = std::byte;
    using alloc_traits    = std::allocator_traits<Alloc>;

    static constexpr std::uint32_t EMPTY = detail::EMPTY_SLOT;

    // ----- alignment -----
    // We need alignment sufficient for both pair_type and uint32_t.
    static constexpr std::size_t required_alignment =
        alignof(pair_type) > alignof(std::uint32_t)
            ? alignof(pair_type)
            : alignof(std::uint32_t);

    // Type with required alignment for allocation
    struct alignas(required_alignment) align_block {
        std::byte data[1];
    };

    using storage_alloc    = typename alloc_traits::template rebind_alloc<align_block>;
    using storage_alloc_tr = std::allocator_traits<storage_alloc>;

    byte_type*  data_       = nullptr;
    std::size_t size_       = 0;
    std::size_t index_size_ = 0;

    // ----- allocator access (EBO) -----
    Alloc&       get_alloc()       noexcept { return *this; }
    const Alloc& get_alloc() const noexcept { return *this; }

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
        while (m < 2 * n) m <<= 1;
        return m;
    }

    // ----- centralized pointer access with std::launder -----
    // pair_type is constructed via piecewise constructor, so the object
    // is formally created. std::launder is used for correct strict aliasing.
    
    pair_type* pair_at(std::size_t i) noexcept {
        return std::launder(reinterpret_cast<pair_type*>(
            data_ + i * sizeof(pair_type)));
    }

    const pair_type* pair_at(std::size_t i) const noexcept {
        return std::launder(reinterpret_cast<const pair_type*>(
            data_ + i * sizeof(pair_type)));
    }

    // pairs(): pointer to first pair (valid when size_ > 0, or for empty map)
    pair_type* pairs() noexcept {
        return pair_at(0);
    }

    const pair_type* pairs() const noexcept {
        return pair_at(0);
    }

    // end_ptr(): past-the-end pointer (no std::launder — no object there)
    pair_type* end_ptr() noexcept {
        return reinterpret_cast<pair_type*>(
            data_ + size_ * sizeof(pair_type));
    }

    const pair_type* end_ptr() const noexcept {
        return reinterpret_cast<const pair_type*>(
            data_ + size_ * sizeof(pair_type));
    }

    // index(): pointer to hash index array
    // uint32_t is trivially constructible, std::launder for correctness
    std::uint32_t* index() noexcept {
        return std::launder(reinterpret_cast<std::uint32_t*>(
            data_ + index_byte_offset(size_)));
    }

    const std::uint32_t* index() const noexcept {
        return std::launder(reinterpret_cast<const std::uint32_t*>(
            data_ + index_byte_offset(size_)));
    }

    // ----- allocation / deallocation -----
    void allocate(std::size_t n, std::size_t m) {
        storage_alloc sa(get_alloc());
        const std::size_t bytes = total_bytes(n, m);
        // Allocate enough align_blocks to cover bytes
        const std::size_t count =
            (bytes + sizeof(align_block) - 1) / sizeof(align_block);
        auto* p = storage_alloc_tr::allocate(sa, count);
        data_ = reinterpret_cast<std::byte*>(p);
    }

    void deallocate() noexcept {
        if (data_) {
            storage_alloc sa(get_alloc());
            const std::size_t bytes = total_bytes(size_, index_size_);
            const std::size_t count =
                (bytes + sizeof(align_block) - 1) / sizeof(align_block);
            storage_alloc_tr::deallocate(
                sa, reinterpret_cast<align_block*>(data_), count);
            data_ = nullptr;
        }
    }

    // ----- element lifetime via allocator_traits -----
    using pair_alloc    = typename alloc_traits::template rebind_alloc<pair_type>;
    using pair_alloc_tr = std::allocator_traits<pair_alloc>;

    void destroy_elements() noexcept {
        pair_alloc pa(get_alloc());
        for (std::size_t i = size_; i > 0; --i) {
            pair_alloc_tr::destroy(pa, pair_at(i - 1));
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
    template <typename HashFn>
    void build_index(HashFn& hash_fn) {
        std::uint32_t* ix = index();
        const std::size_t mask = index_size_ - 1;
        for (std::size_t i = 0; i < index_size_; ++i)
            ix[i] = EMPTY;

        for (std::size_t i = 0; i < size_; ++i) {
            std::size_t slot = hash_fn(pair_at(i)->first) & mask;
            while (ix[slot] != EMPTY)
                slot = (slot + 1) & mask;
            ix[slot] = static_cast<std::uint32_t>(i);
        }
    }

    // ----- lookup -----
    using size_type = std::size_t;

    template <typename CompatibleKey, typename HashFn, typename EqFn>
    std::size_t find_slot(const CompatibleKey& key,
                          HashFn& hash_fn, EqFn& eq_fn) const
        noexcept(noexcept(hash_fn(key)) && noexcept(eq_fn(key, key)))
    {
        if (size_ == 0) return static_cast<std::size_t>(-1);
        const std::uint32_t* ix = index();
        const std::size_t mask = index_size_ - 1;
        std::size_t slot = hash_fn(key) & mask;
        while (ix[slot] != EMPTY) {
            const std::size_t idx = ix[slot];
            if (eq_fn(pair_at(idx)->first, key))
                return idx;
            slot = (slot + 1) & mask;
        }
        return static_cast<std::size_t>(-1);
    }

    // ----- constructors -----
    map_storage() noexcept(noexcept(Alloc())) = default;

    explicit map_storage(const Alloc& alloc) noexcept
        : Alloc(alloc)
    {}

    map_storage(map_storage&& other) noexcept
        : Alloc(std::move(other.get_alloc()))
        , data_(other.data_)
        , size_(other.size_)
        , index_size_(other.index_size_)
    {
        other.data_       = nullptr;
        other.size_       = 0;
        other.index_size_ = 0;
    }

    map_storage& operator=(map_storage&& other) noexcept {
        if (this != &other) {
            destroy_all();
            get_alloc() = std::move(other.get_alloc());
            data_       = other.data_;
            size_       = other.size_;
            index_size_ = other.index_size_;
            other.data_       = nullptr;
            other.size_       = 0;
            other.index_size_ = 0;
        }
        return *this;
    }

    ~map_storage() { destroy_all(); }

    struct adopt_tag {};

    map_storage(adopt_tag, byte_type* data, std::size_t sz,
                std::size_t idx_sz, Alloc a) noexcept
        : Alloc(std::move(a))
        , data_(data)
        , size_(sz)
        , index_size_(idx_sz)
    {}

    friend class immutable_map_builder<Key, Value, Hash, KeyEq, Alloc>;
    template <typename, typename, bool> friend struct copy_support_base;
};

// ============================================================================
// copy_support_base — CRTP base for conditional copy operations
// ============================================================================

template <typename Derived, typename Value, bool>
struct copy_support_base {
    // Value is NOT copy-constructible: copy ops are implicitly deleted.
};

template <typename Derived, typename Value>
struct copy_support_base<Derived, Value, true> {
protected:
    void copy_from(const Derived& other) {
        auto& self = static_cast<Derived&>(*this);
        if (other.size() > 0) {
            const std::size_t n = other.size();
            const std::size_t m = Derived::storage_t::compute_index_size(n);
            self.allocate(n, m);
            self.size_       = n;
            self.index_size_ = m;

            using pair_alloc    = typename Derived::storage_t::pair_alloc;
            using pair_alloc_tr = typename Derived::storage_t::pair_alloc_tr;
            pair_alloc pa(self.get_alloc());

            std::size_t i = 0;
            try {
                for (; i < n; ++i) {
                    pair_alloc_tr::construct(pa, self.pair_at(i),
                                             other.pair_at(i)->first,
                                             other.pair_at(i)->second);
                }
            } catch (...) {
                self.size_ = i;
                throw;
            }
            std::memcpy(self.index(), other.index(),
                        m * sizeof(std::uint32_t));
        }
    }

public:
    Derived& copy_assign(Derived& self, const Derived& other) {
        if (&self != &other) {
            self.destroy_all();
            self.get_alloc() = other.get_alloc();
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
    using storage_t   = map_storage<Key, Value, Alloc>;
    using copy_base_t = copy_support_base<immutable_map, Value>;
    friend copy_base_t;

public:
    using storage_t = map_storage<Key, Value, Alloc>;

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

public:
    immutable_map() noexcept(noexcept(storage_t())) = default;

    immutable_map(immutable_map&& other) noexcept
        : storage_t(std::move(other))
    {}

    immutable_map& operator=(immutable_map&& other) noexcept {
        storage_t::operator=(std::move(other));
        return *this;
    }

    immutable_map(const immutable_map& other)
        : storage_t(other.get_alloc())
    {
        copy_base_t::copy_from(other);
    }

    immutable_map& operator=(const immutable_map& other) {
        return copy_base_t::copy_assign(*this, other);
    }

    ~immutable_map() = default;

    size_type size()  const noexcept { return storage_t::size_; }
    bool      empty() const noexcept { return storage_t::size_ == 0; }

    allocator_type get_allocator() const noexcept {
        return storage_t::get_alloc();
    }

    template <typename CompatibleKey>
    value_type* find(const CompatibleKey& key)
        noexcept(noexcept(Hash{}(key)) && noexcept(KeyEq{}(key, key)))
    {
        Hash hash_fn;
        KeyEq eq_fn;
        size_type s = storage_t::find_slot(key, hash_fn, eq_fn);
        return s == static_cast<size_type>(-1)
            ? nullptr
            : storage_t::pair_at(s);
    }

    template <typename CompatibleKey>
    const value_type* find(const CompatibleKey& key) const
        noexcept(noexcept(Hash{}(key)) && noexcept(KeyEq{}(key, key)))
    {
        Hash hash_fn;
        KeyEq eq_fn;
        size_type s = storage_t::find_slot(key, hash_fn, eq_fn);
        return s == static_cast<size_type>(-1)
            ? nullptr
            : storage_t::pair_at(s);
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
    bool contains(const CompatibleKey& key) const
        noexcept(noexcept(Hash{}(key)) && noexcept(KeyEq{}(key, key)))
    {
        Hash hash_fn;
        KeyEq eq_fn;
        return storage_t::find_slot(key, hash_fn, eq_fn)
               != static_cast<size_type>(-1);
    }

    template <bool IsConst>
    class basic_iterator {
        friend class immutable_map;

        using ptr = std::conditional_t<IsConst, const value_type*, value_type*>;
        ptr       p_;
        size_type pos_;

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

        reference operator*()  const noexcept { return *p_; }
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

        template <bool Other,
                  std::enable_if_t<IsConst && !Other, int> = 0>
        basic_iterator(const basic_iterator<Other>& o) noexcept
            : p_(o.p_), pos_(o.pos_) {}
    };

    using iterator       = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

    iterator       begin()        noexcept { return iterator(storage_t::pairs(), 0); }
    iterator       end()          noexcept { return iterator(storage_t::end_ptr(), storage_t::size_); }
    const_iterator begin()  const noexcept { return const_iterator(storage_t::pairs(), 0); }
    const_iterator end()    const noexcept { return const_iterator(storage_t::end_ptr(), storage_t::size_); }
    const_iterator cbegin() const noexcept { return const_iterator(storage_t::pairs(), 0); }
    const_iterator cend()   const noexcept { return const_iterator(storage_t::end_ptr(), storage_t::size_); }
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
    using allocator_type = Alloc;

private:
    using storage_t     = map_storage<Key, Value, Alloc>;
    using byte_type     = std::byte;
    using alloc_traits  = std::allocator_traits<Alloc>;

    storage_t storage_;
    size_type capacity_ = 0;
    size_type size_     = 0;

public:
    immutable_map_builder() noexcept(noexcept(storage_t())) = default;

    explicit immutable_map_builder(const Alloc& alloc) noexcept
        : storage_(alloc)
    {}

    immutable_map_builder(const immutable_map_builder&) = delete;
    immutable_map_builder& operator=(const immutable_map_builder&) = delete;

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

    allocator_type get_allocator() const noexcept {
        return storage_.get_alloc();
    }

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

    // ----- Core insertion method -----
    // try_emplace with piecewise_construct is the fundamental operation.
    // All other insertion methods are expressed through it.
    template <typename... KeyArgs, typename... ValueArgs>
    void try_emplace(std::piecewise_construct_t,
                     std::tuple<KeyArgs...> key_args,
                     std::tuple<ValueArgs...> value_args) {
        if (size_ >= capacity_) {
            throw std::logic_error(
                "immutable_map_builder::try_emplace: capacity exceeded, "
                "call reserve() first");
        }

        using pair_alloc    = typename storage_t::pair_alloc;
        using pair_alloc_tr = typename storage_t::pair_alloc_tr;
        pair_alloc pa(storage_.get_alloc());

        pair_alloc_tr::construct(pa, storage_.pair_at(size_),
                                 std::piecewise_construct,
                                 std::move(key_args),
                                 std::move(value_args));

        ++size_;
    }

    // ----- Convenience wrappers -----
    
    // try_emplace(key, args...) — convenience wrapper
    template <typename K, typename... ValueArgs>
    void try_emplace(K&& key, ValueArgs&&... value_args) {
        try_emplace(std::piecewise_construct,
                    std::forward_as_tuple(std::forward<K>(key)),
                    std::forward_as_tuple(std::forward<ValueArgs>(value_args)...));
    }

    // emplace(key, args...) — alias for try_emplace (standard compatibility)
    template <typename K, typename... Args>
    void emplace(K&& key, Args&&... args) {
        try_emplace(std::forward<K>(key), std::forward<Args>(args)...);
    }

    // ----- Range insertion -----
    
    // insert(first, last) — insert range of pairs
    template <typename ForwardIt>
    void insert(ForwardIt first, ForwardIt last) {
        for (; first != last; ++first) {
            emplace((*first).first, (*first).second);
        }
    }

    // insert_piecewise(first, last) — insert range of {key, tuple<args...>}
    // No std::apply needed — tuple passed directly to piecewise_construct
    template <typename ForwardIt>
    void insert_piecewise(ForwardIt first, ForwardIt last) {
        for (; first != last; ++first) {
            try_emplace(std::piecewise_construct,
                        std::forward_as_tuple((*first).first),
                        (*first).second);
        }
    }

    template <typename K, typename... ValueArgs>
    void try_emplace(K&& key, ValueArgs&&... value_args) {
        try_emplace(std::piecewise_construct,
                    std::forward_as_tuple(std::forward<K>(key)),
                    std::forward_as_tuple(std::forward<ValueArgs>(value_args)...));
    }

    size_type size() const noexcept { return size_; }

    map_type build() {
        if (!storage_.data_ && capacity_ > 0) {
            throw std::logic_error(
                "immutable_map_builder::build: reserve() was not called");
        }

        storage_.size_ = size_;
        if (size_ > 0) {
            Hash hash_fn;
            storage_.build_index(hash_fn);
        }

        map_type result(typename storage_t::adopt_tag{},
                        storage_.data_, size_, storage_.index_size_,
                        storage_.get_alloc());
        storage_.data_       = nullptr;
        storage_.size_       = 0;
        storage_.index_size_ = 0;
        capacity_ = 0;
        size_     = 0;
        return result;
    }
};

// ============================================================================
// collect
// ============================================================================

template <typename Key, typename Value,
          typename Hash  = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>,
          typename Alloc = std::allocator<std::byte>,
          typename ForwardIt>
immutable_map<Key, Value, Hash, KeyEq, Alloc>
collect(ForwardIt first, ForwardIt last) {
    const auto n = static_cast<std::size_t>(std::distance(first, last));
    immutable_map_builder<Key, Value, Hash, KeyEq, Alloc> builder;
    builder.reserve(n);
    builder.insert(first, last);
    return builder.build();
}

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

template <typename Key, typename Value,
          typename Hash  = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>,
          typename Alloc = std::allocator<std::byte>,
          typename ForwardIt>
immutable_map<Key, Value, Hash, KeyEq, Alloc>
collect_piecewise(ForwardIt first, ForwardIt last) {
    const auto n = static_cast<std::size_t>(std::distance(first, last));
    immutable_map_builder<Key, Value, Hash, KeyEq, Alloc> builder;
    builder.reserve(n);
    builder.insert_piecewise(first, last);
    return builder.build();
}

template <typename Key, typename Value,
          typename Hash  = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>,
          typename Alloc = std::allocator<std::byte>,
          typename Range>
auto collect_piecewise(const Range& range)
    -> decltype(collect_piecewise<Key, Value, Hash, KeyEq, Alloc>(
            std::begin(range), std::end(range)))
{
    return collect_piecewise<Key, Value, Hash, KeyEq, Alloc>(
        std::begin(range), std::end(range));
}

} // namespace imm
