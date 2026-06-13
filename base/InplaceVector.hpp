/** @file InplaceVector.hpp
 * @brief A simple implementation of an inplace vector, which can be used as a fixed-capacity vector that stores
 * elements directly within its own memory footprint, without dynamic allocation. This is useful for small collections
 * of objects where the overhead of dynamic memory allocation is undesirable. The implementation provides basic
 * functionalities such as construction, destruction, copy/move semantics, element access, and iteration. It also
 * includes bounds checking for element access and ensures proper cleanup of resources when the vector is destroyed or
 * cleared. The InplaceVector is designed to be a drop-in replacement for std::vector in scenarios where a fixed
 * capacity is sufficient and performance is critical, especially in embedded systems or performance-sensitive
 * applications. It is implemented in a way that allows it to be used in constexpr contexts, making it suitable for
 * compile-time computations as well. The implementation also includes a check for standard library support for
 * std::inplace_vector, and if available, it uses the standard implementation instead of the custom one. This allows for
 * better performance and compatibility when the standard library provides an optimized version of inplace_vector.
 * @author HsBa
 */

#ifndef HSBA_SLICER_INPLACE_VECTOR_HPP
#define HSBA_SLICER_INPLACE_VECTOR_HPP
#pragma once

#include <cstddef>
#include <iterator>
#include <new>
#include <type_traits>
#include <utility>

#if __has_include(<inplace_vector>)
#include <inplace_vector>
#endif

// Try to detect standard inplace_vector support.
#if defined(__cpp_lib_inplace_vector) && defined(__cpp_lib_constexpr_inplace_vector)
#include <memory>
namespace HsBa::Slicer::Utils
{
template <typename T, std::size_t N>
using InplaceVector = std::inplace_vector<T, N>;
}
#else
namespace HsBa::Slicer::Utils
{

template <typename T, std::size_t N>
class InplaceVector
{
public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = pointer;
    using const_iterator = const_pointer;

    constexpr InplaceVector() noexcept : size_(0) {}

    ~InplaceVector() noexcept { clear(); }

    constexpr InplaceVector(const InplaceVector& other)
    {
        size_ = 0;
        for (size_type i = 0; i < other.size_; ++i)
        {
            emplace_back(other[i]);
        }
    }

    constexpr InplaceVector(InplaceVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        size_ = 0;
        for (size_type i = 0; i < other.size_; ++i)
        {
            emplace_back(std::move(other[i]));
        }
        other.clear();
    }

    constexpr InplaceVector& operator=(const InplaceVector& other)
    {
        if (this != &other)
        {
            clear();
            for (size_type i = 0; i < other.size_; ++i)
                emplace_back(other[i]);
        }
        return *this;
    }

    constexpr InplaceVector& operator=(InplaceVector&& other) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (this != &other)
        {
            clear();
            for (size_type i = 0; i < other.size_; ++i)
                emplace_back(std::move(other[i]));
            other.clear();
        }
        return *this;
    }

    constexpr size_type size() const noexcept { return size_; }
    constexpr size_type capacity() const noexcept { return N; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr reference operator[](size_type idx) noexcept { return *ptr_at(idx); }
    constexpr const_reference operator[](size_type idx) const noexcept { return *ptr_at(idx); }

    constexpr reference at(size_type idx)
    {
        if (idx >= size_)
            throw std::out_of_range("InplaceVector::at");
        return (*this)[idx];
    }

    constexpr void clear() noexcept
    {
        for (size_type i = size_; i > 0; --i)
        {
            ptr_at(i - 1)->~T();
        }
        size_ = 0;
    }

    template <typename... Args>
    constexpr reference emplace_back(Args&&... args)
    {
        if (size_ >= N)
            throw std::length_error("InplaceVector capacity exceeded");
        void* place = static_cast<void*>(&storage_[size_]);
        T* obj = new (place) T(std::forward<Args>(args)...);
        ++size_;
        return *obj;
    }

    constexpr void push_back(const T& v) { emplace_back(v); }
    constexpr void push_back(T&& v) { emplace_back(std::move(v)); }

    constexpr void pop_back() noexcept
    {
        if (size_ == 0)
            return;
        ptr_at(size_ - 1)->~T();
        --size_;
    }

    constexpr iterator begin() noexcept { return data(); }
    constexpr iterator end() noexcept { return data() + size_; }
    constexpr const_iterator begin() const noexcept { return data(); }
    constexpr const_iterator end() const noexcept { return data() + size_; }

    constexpr pointer data() noexcept { return reinterpret_cast<pointer>(storage_); }
    constexpr const_pointer data() const noexcept { return reinterpret_cast<const_pointer>(storage_); }

private:
    static_assert(N > 0, "InplaceVector requires positive capacity");
    using storage_t = std::aligned_storage_t<sizeof(T), alignof(T)>;
    storage_t storage_[N];
    size_type size_;

    constexpr pointer ptr_at(size_type idx) noexcept { return reinterpret_cast<pointer>(&storage_[idx]); }
    constexpr const_pointer ptr_at(size_type idx) const noexcept
    {
        return reinterpret_cast<const_pointer>(&storage_[idx]);
    }
};

}  // namespace HsBa::Slicer::Utils
#endif  // __cpp_lib_inplace_vector

#endif  // !HSBA_SLICER_INPLACE_VECTOR_HPP
