#ifndef STARSIGHT_SSOVECTOR_HPP
#define STARSIGHT_SSOVECTOR_HPP

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#if defined(__cplusplus)
#if __cplusplus >= 201703L
    #define NODISCARD [[nodiscard]]
#define NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define NODISCARD
#define NO_UNIQUE_ADDRESS
#endif
#if __cplusplus >= 202002L
#define CXX20_CONSTEXPR constexpr
#else
#define CXX20_CONSTEXPR
#endif
#else
    #define CXX20_CONSTEXPR
#define NODISCARD
#define NO_UNIQUE_ADDRESS
#endif

namespace detail
{
    constexpr bool likely(bool x) {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_expect(x, 1);
#else
        return x;
#endif
    }

    constexpr bool unlikely(bool x) {
        return !likely(!x);
    }
} // namespace detail

template<std::uint64_t n>
using required_uint_t =
        typename std::conditional<
                (n <= std::numeric_limits<std::uint8_t>::max()), std::uint8_t,
                typename std::conditional<
                        (n <= std::numeric_limits<std::uint16_t>::max()), std::uint16_t,
                        typename std::conditional<
                                (n <=
                                 std::numeric_limits<std::uint32_t>::max()),
                                std::uint32_t, std::uint64_t>::type>::type>::type;

template<class T, std::size_t MinStackCapacity = 0, std::size_t MaxCapacity = std::numeric_limits<std::size_t>::max()>
class ssovector {
    static_assert(MaxCapacity >= MinStackCapacity, "Max Capacity has to be greater than minimum stack capacity");

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = typename std::make_signed<size_type>::type;
    using reference = value_type &;
    using rvalue_reference = value_type &&;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    using stack_size_type = required_uint_t<MinStackCapacity>;
    using heap_size_type = required_uint_t<MaxCapacity>;

public:
    constexpr static std::size_t stack_capacity() {
        constexpr std::size_t res =
                std::max((sizeof(heap_data_t) - sizeof(stack_size_type)) / sizeof(value_type), MinStackCapacity);
        return res;
    }

    constexpr static std::size_t is_always_small() {
        return MaxCapacity == stack_capacity();
    }

private:
    struct heap_data_t {
        heap_size_type m_size{};
        value_type *m_array{};
    };

    struct stack_data_t {
        stack_size_type m_size{};
        std::array<value_type, stack_capacity()> m_array{};
    };

    struct empty {
        template<class G>
        empty &operator=(G &&) { return *this; }

        template<class G>
        empty(G &&) {}

        template<class G>
        operator G() const {
            return {};
        }
    };

    NO_UNIQUE_ADDRESS typename std::conditional<
            stack_capacity() == MaxCapacity, empty, heap_size_type>::type m_capacity = stack_capacity();

    union data_union_t {
        heap_data_t heap;
        stack_data_t stack;

        constexpr data_union_t() : stack{} {}

        CXX20_CONSTEXPR ~data_union_t() {} // destruction is handled in normal destructor
    } m_data{};

    constexpr void _set_size(size_type size) {
        if (is_small()) {
            m_data.stack.m_size = size;
        } else {
            m_data.heap.m_size = size;
        }
    }

    constexpr void _debug_throw_if_invalid_index(size_type idx) const{
#if !NDEBUG
        if (idx >= size())
            throw std::out_of_range("invalid index: " + std::to_string(idx));
#endif
    }

public:
    NODISCARD constexpr bool is_small() const {
        if (is_always_small())
            return true;
        else
            return capacity() <= stack_capacity();
    }

    constexpr ssovector() = default;

    constexpr ssovector(size_type size) : ssovector(size, value_type{}) {}

    constexpr ssovector(size_type size, const_reference default_value) {
        if (size <= stack_capacity()) {
            for (size_type i = 0; i < size; ++i) {
                m_data.stack.m_array[i] = default_value;
            }
            m_data.stack.m_size = size;
            m_capacity = stack_capacity();
        } else {
#ifndef NDEBUG
            if (is_always_small())
                throw std::bad_alloc();
#endif
            _allocate_heap(size);
            for (size_type i = 0; i < size; ++i) {
                new (&m_data.heap.m_array[i]) value_type{default_value};
            }
            m_data.heap.m_size = size;
            m_capacity = size;
        }
    }

    template<class ForwardIterator, typename std::enable_if<
            decltype(std::next(std::declval<ForwardIterator>()), 0){} == 0 &&
            decltype(*std::declval<ForwardIterator>(), 0){} == 0,
            bool>::type = false>
    constexpr ssovector(ForwardIterator first, ForwardIterator last) {
        for (; first != last; ++first)
            push_back(*first);
    }

    template<class RandomAccessIterator, typename std::enable_if<
            decltype(std::next(std::declval<RandomAccessIterator>()), 0){} == 0 &&
            decltype(*std::declval<RandomAccessIterator>(), 0){} == 0 &&
            decltype(std::declval<RandomAccessIterator>() - std::declval<RandomAccessIterator>(), 0){} == 0,
            bool>::type = false>
    constexpr ssovector(RandomAccessIterator first, RandomAccessIterator last) {
        const size_type size = last - first;
        _set_size(size);
        if (!is_small())
            _reallocate(size);
        for (; first != last; first = std::next(first))
            push_back(*first);
    }

    constexpr ssovector(ssovector const &other) {
        *this = other;
    }

    constexpr ssovector(ssovector &&other) noexcept {
        *this = std::move(other);
    }

    constexpr ssovector &operator=(ssovector const &other) {
        if (other.is_small()) {
            this->~ssovector();
            m_data.stack.m_size = other.m_data.stack.m_size;
            for (size_type i = 0; i < other.size(); ++i)
                m_data.stack.m_array[i] = other.m_data.stack.m_array[i];
            m_capacity = other.m_data.stack.m_size;
        } else {
            if (capacity() < other.capacity())
                *this = ssovector(other.size()); // not infinite recursion since rhs is an rvalue reference
            for (size_type i = 0; i < other.size(); ++i)
                m_data.heap.m_array[i] = other.m_data.heap.m_array[i];
        }
        return *this;
    }

    constexpr ssovector &operator=(ssovector &&other) noexcept {
        if (other.is_small()) {
            this->~ssovector();
            m_data.stack.m_array = std::move(other.m_data.stack.m_array);
            m_data.stack.m_size = other.m_data.stack.m_size;
            m_capacity = other.m_data.stack.m_size;
        } else {
            this->~ssovector();
            m_data.heap.m_array = other.m_data.heap.m_array;
            m_data.heap.m_size = other.m_data.heap.m_size;
            m_capacity = other.m_capacity;
            other.m_data.heap.m_array = nullptr;
            other.m_data.heap.m_size = 0;
            other.m_capacity = stack_capacity();
        }
        return *this;
    }

    CXX20_CONSTEXPR ~ssovector() {
        for (auto &&i: *this) {
            i.~value_type();
        }
        if (!is_small()) {
            _deallocate_heap();
            m_data.heap.m_array = nullptr;
            m_data.heap.m_size = 0;
        }
    }

    NODISCARD constexpr size_type size() const {
        if (is_small()) {
            return m_data.stack.m_size;
        } else {
            return m_data.heap.m_size;
        }
    }

    NODISCARD constexpr size_type size_bytes() const
    {
        if(is_small())
        {
            return m_data.stack.m_size * sizeof(value_type);
        }
        else
        {
            return m_data.heap.m_size * sizeof(value_type);
        }
    }

    NODISCARD constexpr size_type capacity() const {
        if (is_always_small())
            return stack_capacity();
        else
            return m_capacity;
    }

    NODISCARD constexpr size_type max_size() const {
        return MaxCapacity;
    }

    NODISCARD constexpr pointer data() {
        if (is_small())
            return &m_data.stack.m_array[0];
        else
            return &m_data.heap.m_array[0];
    }

    NODISCARD constexpr const_pointer data() const {
        if (is_small())
            return &m_data.stack.m_array[0];
        else
            return &m_data.heap.m_array[0];
    }

    NODISCARD constexpr reference operator[](size_type idx) {
        _debug_throw_if_invalid_index(idx);
        return data()[idx];
    }

    NODISCARD constexpr const_reference operator[](size_type idx) const {
        _debug_throw_if_invalid_index(idx);
        return data()[idx];
    }

    template<std::size_t StackCap, std::size_t MaxCap>
    NODISCARD constexpr bool operator==(ssovector<value_type, StackCap, MaxCap> const &other) const {
        if (size() != other.size())
            return false;
        const_pointer p1 = data(), p2 = other.data();
        for (size_type i = 0; i < size(); ++i)
            if (p1[i] != p2[i])
                return false;
        return true;
    }

    template<std::size_t StackCap, std::size_t MaxCap>
    NODISCARD constexpr bool operator!=(ssovector<value_type, StackCap, MaxCap> const &other) const {
        return !(*this == other);
    }

    template<std::size_t StackCap, std::size_t MaxCap>
    NODISCARD constexpr bool operator<(ssovector<value_type, StackCap, MaxCap> const &other) const {
        for (size_type i = 0; i < std::min(size(), other.size()); ++i) {
            const auto lhs = (*this)[i], rhs = other[i];
            if (lhs < rhs)
                return true;
            if (rhs < lhs)
                return false;
        }
        return size() < other.size();
    }

    template<std::size_t StackCap, std::size_t MaxCap>
    NODISCARD constexpr bool operator>(ssovector<value_type, StackCap, MaxCap> const &other) const {
        return other < *this;
    }

    template<std::size_t StackCap, std::size_t MaxCap>
    NODISCARD constexpr bool operator<=(ssovector<value_type, StackCap, MaxCap> const &other) const {
        return !(other > *this);
    }

    template<std::size_t StackCap, std::size_t MaxCap>
    NODISCARD constexpr bool operator>=(ssovector<value_type, StackCap, MaxCap> const &other) const {
        return other <= *this;
    }

    constexpr T &push_back(const_reference value) {
        return emplace_back(value);
    }

    constexpr T &push_back(rvalue_reference value) {
        return emplace_back(std::move(value));
    }

    template<class... Args>
    constexpr T &emplace_back(Args &&...args) {
        _grow_to_allow_size_increase();
        if (is_small()) {
            m_data.stack.m_array[m_data.stack.m_size] = value_type{std::forward<Args>(args)...};
            return m_data.stack.m_array[m_data.stack.m_size++];
        } else {
            new (&m_data.heap.m_array[m_data.heap.m_size]) value_type{std::forward<Args>(args)...};
            return m_data.heap.m_array[m_data.heap.m_size++];
        }
    }

    constexpr T pop_back() {
        T ret = std::move(*rbegin());
        erase(rbegin().base());
        return ret;
    }

    NODISCARD constexpr iterator begin() {
        return data();
    }

    NODISCARD constexpr const_iterator begin() const {
        return data();
    }

    NODISCARD constexpr const_iterator cbegin() const {
        return data();
    }

    NODISCARD constexpr reverse_iterator rbegin() {
        return reverse_iterator{end()};
    }

    NODISCARD constexpr const_reverse_iterator rbegin() const {
        return const_reverse_iterator{end()};
    }

    NODISCARD constexpr const_reverse_iterator crbegin() const {
        return const_reverse_iterator{end()};
    }

    NODISCARD constexpr iterator end() {
        return data() + size();
    }

    NODISCARD constexpr const_iterator end() const {
        return data() + size();
    }

    NODISCARD constexpr const_iterator cend() const {
        return data() + size();
    }

    NODISCARD constexpr reverse_iterator rend() {
        return reverse_iterator{begin()};
    }

    NODISCARD constexpr const_reverse_iterator rend() const {
        return const_reverse_iterator{begin()};
    }

    NODISCARD constexpr const_reverse_iterator crend() const {
        return const_reverse_iterator{begin()};
    }

    NODISCARD constexpr reference front() {
        return *begin();
    }

    NODISCARD constexpr const_reference front() const {
        return *begin();
    }

    NODISCARD constexpr reference back() {
        return *rbegin();
    }

    NODISCARD constexpr const_reference back() const {
        return *rbegin();
    }

    NODISCARD constexpr bool empty() const {
        return size() == 0;
    }

    void resize(size_type new_size) {
        if (new_size > m_capacity) {
            _reallocate(new_size);
            m_data.heap.m_size = new_size;
        } else if ((new_size <= stack_capacity()) & (size() > stack_capacity())) {
            _reallocate(new_size);
            m_data.stack.m_size = new_size;
        } else {
            _set_size(new_size);
        }
    }

    constexpr iterator erase(iterator p1, iterator p2) {
        const difference_type return_value_offs = p1 - data();
        while (p2 < end())
            *p1++ = std::move(*p2++);
        const size_type old_size = size();
        const size_type new_size = old_size - (p2 - p1);
        if (old_size > stack_capacity() && new_size <= stack_capacity()) {
            _set_size(new_size);
            _reallocate(new_size);
            return m_data.stack.m_array.data() + return_value_offs;
        } else {
            _set_size(new_size);
            return data() + return_value_offs;
        }
    }

    constexpr iterator erase(iterator position) {
        return erase(position, position + 1);
    }

private:
    constexpr void _grow_to_allow_size_increase() {
        if (is_always_small())
            return;
        if (detail::unlikely(size() >= capacity()))
            _reallocate(std::min(MaxCapacity, capacity() * 2)); // if capacity() * 2 overflows you have bigger problems
    }

    constexpr void _reallocate(size_type new_capacity) {
        ssovector other;
        if (new_capacity > stack_capacity()) {
            other._allocate_heap(new_capacity);
        }
        auto sz = std::min(size(), new_capacity);
        other._set_size(sz);
        other.m_capacity = new_capacity;
        if (std::is_trivially_move_constructible<value_type>::value &&
            std::is_trivially_destructible<value_type>::value) {
            std::memcpy(other.data(), data(), sz * sizeof(value_type));
        } else {
            for (size_type i = 0; i < sz; ++i)
                other.data()[i] = std::move(data()[i]);
        }
        *this = std::move(other);
    }

    void _allocate_heap(size_type size) {
#ifndef NDEBUG
        if (is_always_small())
            throw std::bad_alloc();
        if (size > MaxCapacity)
            throw std::bad_alloc();
#endif
        m_data.heap.m_array = ::new T[size]{};
        m_capacity = size;
    }

    void _deallocate_heap() {
        ::delete[] m_data.heap.m_array;
    }
};

#undef CXX20_CONSTEXPR
#undef NODISCARD
#undef NO_UNIQUE_ADDRESS

#endif //STARSIGHT_SSOVECTOR_HPP
