/** @file memory_pool.hpp
 * @brief A header file containing the definition of a memory pool allocator.
 * This file defines a custom memory pool allocator that can be used with standard library containers to manage memory more efficiently. The allocator maintains a fixed-size pool of memory and provides allocation and deallocation functions that operate within this pool. It also includes a static memory pool variant for use cases where a shared memory pool is desired across multiple instances of the allocator.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_MEMORY_POOL_HPP
#define HSBA_SLICER_MEMORY_POOL_HPP

#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>

namespace HsBa::Slicer
{
/** @brief A struct to hold the state of the memory pool. 
 * This struct contains a character array that serves as the memory pool, a boolean array to track which parts of the pool are currently in use, and a count of how many bytes are currently allocated. The struct is aligned to the maximum alignment requirement of any type to ensure that it can be used to allocate memory for any type without violating alignment requirements.
 * @tparam PoolSize The size of the memory pool in bytes.
*/
template <size_t PoolSize>
struct MemoryPoolState
{
    alignas(std::max_align_t) char pool[PoolSize];
    bool used[PoolSize] = {false};
    size_t used_count = 0;
};
/** @brief A class to implement a memory pool allocator.
 * This class provides a custom memory pool allocator that can be used with standard library containers to manage memory more efficiently. The allocator maintains a fixed-size pool of memory and provides allocation and deallocation functions that operate within this pool. It also includes a static memory pool variant for use cases where a shared memory pool is desired across multiple instances of the allocator.
 * @tparam T The type of elements for which to allocate memory.
 * @tparam PoolSize The size of the memory pool in bytes.
 */
template <typename T, size_t PoolSize>
class MemoryPool
{
public:
    /** @brief The type of elements for which to allocate memory. */
    using value_type = T;
    /** @brief A pointer to an element of type T. */
    using pointer = T*;
    /** @brief A constant pointer to an element of type T. */
    using const_pointer = const T*;
    /** @brief A reference to an element of type T. */
    using reference = T&;
    /** @brief A constant reference to an element of type T. */
    using const_reference = const T&;
    /** @brief The size type used for allocation. */
    using size_type = size_t;
    /** @brief The difference type used for pointer arithmetic. */
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;
    using is_always_equal = std::false_type;
    /** @brief A struct to rebind the allocator for a different type. 
     * This struct allows the allocator to be used with different types by providing a way to create a new allocator instance that is compatible with the new type. The rebind struct contains a single member, `other`, which is an alias for a MemoryPool instance that allocates memory for the new type U while maintaining the same pool size.
     * @tparam U The new type for which to create a compatible allocator.
    */
    template <typename U>
    struct rebind
    {
        using other = MemoryPool<U, PoolSize>;
    };

    MemoryPool() : state_(std::make_shared<MemoryPoolState<PoolSize>>()) {}
    ~MemoryPool() = default;
    /** @brief Construct a new memory pool instance.
     * @param o The memory pool instance to copy from.
     */
    MemoryPool(const MemoryPool& o) : state_(o.state_) {}
    MemoryPool& operator=(const MemoryPool& o)
    {
        if (this != &o)
        {
            state_ = o.state_;
        }
        return *this;
    }
    MemoryPool(MemoryPool&& o) : state_(std::move(o.state_)) {}
    MemoryPool& operator=(MemoryPool&& o)
    {
        if (this != &o)
        {
            state_ = std::move(o.state_);
        }
        return *this;
    }
    /** @brief Allocate memory for a specified number of elements.
     * This function attempts to allocate memory for n elements of type T from the memory pool. It checks if there is enough free space in the pool to accommodate the requested allocation and if so, it marks the corresponding bytes in the pool as used and returns a pointer to the allocated memory. If there is not enough free space, it throws a std::bad_alloc exception.
     * @param n The number of elements to allocate.
     * @param hint A pointer that can be used as a hint for where to allocate the memory (not used in this implementation).
     * @return A pointer to the allocated memory.
     * @throws std::bad_alloc If there is not enough free space in the pool to accommodate the requested allocation.
     */
    pointer allocate(size_type n, const void* hint = nullptr)
    {
        if (n == 0)
            return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);
        if (n * sizeof(T) > PoolSize || state_->used_count + n * sizeof(T) > PoolSize)
            throw std::bad_alloc();

        size_t alignment = alignof(T);
        for (size_t i = 0; i <= PoolSize - n * sizeof(T); i += alignment)
        {
            bool can_allocate = true;
            for (size_t j = 0; j < n * sizeof(T); ++j)
            {
                if (state_->used[i + j])
                {
                    can_allocate = false;
                    break;
                }
            }
            if (can_allocate)
            {
                for (size_t j = 0; j < n * sizeof(T); ++j)
                {
                    state_->used[i + j] = true;
                }
                state_->used_count += n * sizeof(T);
                return reinterpret_cast<pointer>(state_->pool + i);
            }
        }
        throw std::bad_alloc();
    }

    template <typename U>
    MemoryPool(const MemoryPool<U, PoolSize>& other) : state_(other.get_state())
    {
    }
    /** @brief Deallocate memory for a specified number of elements.
     * This function deallocates memory for n elements of type T from the memory pool. It checks if the pointer p is valid and if the deallocation is within the bounds of the pool, and if so, it marks the corresponding bytes in the pool as free.
     * @param p A pointer to the memory to deallocate.
     * @param n The number of elements to deallocate.
     * @throws std::invalid_argument If the pointer is out of bounds or invalid.
     */
    void deallocate(pointer p, size_type n)
    {
        if (!p || !state_)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        char* char_ptr = reinterpret_cast<char*>(p);
        size_t offset = char_ptr - state_->pool;
        if (offset >= PoolSize || offset + n * sizeof(T) > PoolSize)
        {
            throw std::invalid_argument("Pointer out of pool bounds");
        }
        for (size_t i = 0; i < n * sizeof(T); ++i)
        {
            state_->used[offset + i] = false;
        }
        state_->used_count -= n * sizeof(T);
    }
    /** @brief Get the maximum number of elements that can be allocated.
     * @return The maximum number of elements that can be allocated.
     */
    size_type max_size() const noexcept { return PoolSize / sizeof(T); }
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args)
    {
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }
    /** @brief Destroy an element of type U.
     * This function destroys an element of type U at the specified pointer p.
     * @param p A pointer to the element to destroy.
     */
    template <typename U>
    void destroy(U* p)
    {
        p->~U();
    }
    bool operator==(const MemoryPool& other) const noexcept { return state_ == other.state_; }

    bool operator!=(const MemoryPool& other) const noexcept { return !(*this == other); }

    const std::shared_ptr<MemoryPoolState<PoolSize>>& get_state() const noexcept { return state_; }

private:
    std::shared_ptr<MemoryPoolState<PoolSize>> state_;
    std::mutex mutex_;
};
/** @brief A static memory pool implementation.
 * This class provides a memory pool with a fixed size that can be used to allocate and deallocate memory for objects of type T.
 * @tparam T The type of objects for which to allocate memory.
 * @tparam PoolSize The size of the memory pool in bytes.
 */
template <typename T, size_t PoolSize>
class StaticMemoryPool
{
public:
    /** @brief The type of elements for which to allocate memory. */
    using value_type = T;
    /** @brief A pointer to an element of type T. */
    using pointer = T*;
    /** @brief A constant pointer to an element of type T. */
    using const_pointer = const T*;
    /** @brief A reference to an element of type T. */
    using reference = T&;
    /** @brief A constant reference to an element of type T. */
    using const_reference = const T&;
    /** @brief The size type used for allocation. */
    using size_type = size_t;
    /** @brief The difference type used for allocation. */
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::true_type;
    /** @brief Rebind the allocator type for a different element type.
     * This struct is used to rebind the allocator type for a different element type U.
     * @tparam U The new element type.
     */
    template <typename U>
    struct rebind
    {
        using other = StaticMemoryPool<U, PoolSize>;
    };

    StaticMemoryPool() = default;

    StaticMemoryPool(const StaticMemoryPool& o) = default;
    StaticMemoryPool& operator=(const StaticMemoryPool& o) = default;
    StaticMemoryPool(StaticMemoryPool&& o) = default;
    StaticMemoryPool& operator=(StaticMemoryPool&& o) = default;

    template <typename U>
    StaticMemoryPool(const StaticMemoryPool<U, PoolSize>&)
    {
    }
    /** @brief Allocate memory for a specified number of elements.
     * This function allocates memory for n elements of type T from the memory pool. It checks if the allocation is within the bounds of the pool, and if so, it marks the corresponding bytes in the pool as allocated.
     * @param n The number of elements to allocate.
     * @param hint A hint for the allocation.
     * @return A pointer to the allocated memory.
     * @throws std::bad_alloc If the allocation fails.
     */
    pointer allocate(size_type n, const void* hint = nullptr)
    {
        if (n == 0)
            return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);
        if (n * sizeof(T) > PoolSize || used_count + n * sizeof(T) > PoolSize)
            throw std::bad_alloc();

        for (size_t i = 0; i <= PoolSize - n * sizeof(T); ++i)
        {
            bool can_allocate = true;
            for (size_t j = 0; j < n * sizeof(T); ++j)
            {
                if (used[i + j])
                {
                    can_allocate = false;
                    break;
                }
            }
            if (can_allocate)
            {
                for (size_t j = 0; j < n * sizeof(T); ++j)
                {
                    used[i + j] = true;
                }
                used_count += n * sizeof(T);
                return reinterpret_cast<pointer>(buffer + i);
            }
        }
        throw std::bad_alloc();
    }
    /** @brief Deallocate memory for a specified number of elements.
     * This function deallocates memory for n elements of type T from the memory pool. It checks if the pointer p is valid and if the deallocation is within the bounds of the pool, and if so, it marks the corresponding bytes in the pool as free.
     * @param p A pointer to the memory to deallocate.
     * @param n The number of elements to deallocate.
     * @throws std::invalid_argument If the pointer is out of bounds or invalid.
     */
    void deallocate(pointer p, size_type n)
    {
        if (!p)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        char* char_ptr = reinterpret_cast<char*>(p);
        size_t offset = char_ptr - buffer;
        if (offset >= PoolSize || offset + n * sizeof(T) > PoolSize)
        {
            throw std::invalid_argument("Pointer out of pool bounds");
        }
        for (size_t i = 0; i < n * sizeof(T); ++i)
        {
            used[offset + i] = false;
        }
        used_count -= n * sizeof(T);
    }

    size_type max_size() const noexcept { return PoolSize / sizeof(T); }

    /** @brief Construct an element of type U.
     * This function constructs an element of type U at the specified pointer p.
     * @param p A pointer to the location where the element should be constructed.
     * @param args The arguments for the constructor.
     */
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args)
    {
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    /** @brief Destroy an element of type U.
     * This function destroys an element of type U at the specified pointer p.
     * @param p A pointer to the element to destroy.
     */
    template <typename U>
    void destroy(U* p)
    {
        p->~U();
    }

    bool operator==(const StaticMemoryPool&) const noexcept
    {
        return true;  // All instances are considered equal
    }

    bool operator!=(const StaticMemoryPool&) const noexcept
    {
        return false;  // All instances are considered equal
    }
    /* @brief Get the number of bytes currently allocated in the memory pool.
     * @return The number of bytes currently allocated in the memory pool.
     */
    size_t UsedCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return used_count;
    }

private:
    alignas(std::max_align_t) inline static char buffer[PoolSize];
    inline static bool used[PoolSize] = {false};
    inline static size_t used_count = 0;
    inline static std::mutex mutex_;
};
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_MEMORY_POOL_HPP