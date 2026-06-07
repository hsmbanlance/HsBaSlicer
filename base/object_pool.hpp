/**  @file object_pool.hpp
 * @brief A header file containing the definition of a named object pool.
 * @author HsBa 
 */
#pragma once
#ifndef HSBA_SLICER_OBJECT_POOL_HPP
#define HSBA_SLICER_OBJECT_POOL_HPP

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "error.hpp"

namespace HsBa::Slicer
{
/** @brief A named object pool for managing a collection of shared objects.
* @tparam T The type of objects to manage.
* @tparam MaxSize The maximum number of objects that can be stored in the pool.
*/
template <typename T, std::size_t MaxSize>
class NamedObjectPool
{
public:
    /* @brief The type of objects managed by the pool. */
    using ObjectType = T;
    /* @brief A shared pointer to an object of type T. */
    using ObjectPtr = std::shared_ptr<T>;
    /* @brief A weak pointer to an object of type T. */
    using WeakPtr = std::weak_ptr<T>;
    /* @brief The maximum number of objects that can be stored in the pool. */
    static constexpr std::size_t max_size = MaxSize;

    struct PooledObject
    {
        ObjectPtr shared_ref;
        WeakPtr weak_ref;

        explicit PooledObject(const ObjectPtr& ref) : shared_ref(ref), weak_ref(ref) {}

        bool isInactive() const { return shared_ref.use_count() == 1; }

        long useCount() const { return shared_ref.use_count(); }
    };

    NamedObjectPool() = default;

    NamedObjectPool(const NamedObjectPool&) = delete;
    NamedObjectPool& operator=(const NamedObjectPool&) = delete;
    NamedObjectPool(NamedObjectPool&&) = delete;
    NamedObjectPool& operator=(NamedObjectPool&&) = delete;

    ~NamedObjectPool()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        objects_.clear();
    }
    /** @brief Create and add an object to the pool.
     * @tparam Args The types of the arguments for the object constructor.
     * @param name The name of the object.
     * @param args The arguments for the object constructor.
     * @return A shared pointer to the created object.
     */
    template <typename... Args>
    ObjectPtr emplace(const std::string& name, Args&&... args)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (objects_.find(name) != objects_.end())
        {
            throw InvalidArgumentError("Object with name '" + name + "' already exists in pool");
        }
        // only cleanup when trying to add a new object and the pool is already at max capacity, otherwise we might end
        // up in a situation where the pool is constantly cleaning up inactive objects even when there are still active
        // ones, which would lead to performance issues and unexpected behavior
        if (objects_.size() >= MaxSize)
            CleanupInactiveObjects();

        if (objects_.size() >= MaxSize)
        {
            throw RuntimeError("Pool is full (max size: " + std::to_string(MaxSize) +
                               ") and no inactive objects available");
        }

        ObjectPtr ptr = std::make_shared<T>(std::forward<Args>(args)...);

        objects_.emplace(name, PooledObject{ptr});

        return ptr;
    }

    /** @brief Allocate and add an object to the pool.
     * @tparam Allocator The type of the allocator.
     * @tparam Args The types of the arguments for the object constructor.
     * @param name The name of the object.
     * @param alloc The allocator to use for memory allocation.
     * @param args The arguments for the object constructor.
     * @return A shared pointer to the allocated object.
     */
    template <typename Allocator, typename... Args>
    ObjectPtr allocate(const std::string& name, Allocator&& alloc, Args&&... args)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (objects_.find(name) != objects_.end())
        {
            throw InvalidArgumentError("Object with name '" + name + "' already exists in pool");
        }
        // only cleanup when trying to add a new object and the pool is already at max capacity, otherwise we might end
        // up in a situation where the pool is constantly cleaning up inactive objects even when there are still active
        // ones, which would lead to performance issues and unexpected behavior
        if (objects_.size() >= MaxSize)
            CleanupInactiveObjects();
        if (objects_.size() >= MaxSize)
        {
            throw RuntimeError("Pool is full (max size: " + std::to_string(MaxSize) +
                               ") and no inactive objects available");
        }
        ObjectPtr ptr = std::allocate_shared<T>(std::forward<Allocator>(alloc), std::forward<Args>(args)...);
        objects_.emplace(name, PooledObject{ptr});
        return ptr;
    }

    /** @brief Get an object from the pool by name.
     * @param name The name of the object to retrieve.
     * @return A shared pointer to the object if found, or nullptr if not found.
     */
    ObjectPtr get(const std::string& name)
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = objects_.find(name);
        if (it != objects_.end())
        {

            return it->second.shared_ref;
        }
        return nullptr;
    }
    /** @brief Get an object from the pool by name using the subscript operator.
     * @param name The name of the object to retrieve.
     * @return A shared pointer to the object if found, or nullptr if not found.
     */
    ObjectPtr operator[](const std::string& name) { return get(name); }

    /** @brief Check if the pool contains an object with the specified name.
     * @param name The name of the object to check for.
     * @return true if the object exists in the pool, false otherwise.
     */
    bool Contains(const std::string& name) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return objects_.find(name) != objects_.end();
    }

    /** @brief Get the number of objects in the pool.
     * @return The number of objects in the pool.
     */
    std::size_t size() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return objects_.size();
    }

    /** @brief Get the number of inactive objects in the pool.
     * @return The number of inactive objects in the pool.
     */
    std::size_t InactiveCount() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::size_t count = 0;
        for (const auto& [name, obj] : objects_)
        {
            if (obj.isInactive())
            {
                ++count;
            }
        }
        return count;
    }

    /** @brief Get the number of active objects in the pool.
     * @return The number of active objects in the pool.
     */
    std::size_t ActiveCount() const { return size() - InactiveCount(); }

    /** @brief Clean up inactive objects in the pool.
     * @return The number of inactive objects that were cleaned up.
     */
    std::size_t Cleanup()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return CleanupInactiveObjects();
    }
    /** @brief Get a list of all object names in the pool.
     * @return A vector containing the names of all objects in the pool.
     */
    std::vector<std::string> GetNames() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(objects_.size());
        for (const auto& [name, _] : objects_)
        {
            names.push_back(name);
        }
        return names;
    }

private:
    std::size_t CleanupInactiveObjects()
    {
        std::size_t cleaned = 0;
        for (auto it = objects_.begin(); it != objects_.end();)
        {
            if (it->second.isInactive())
            {
                it->second.shared_ref.reset();
                it = objects_.erase(it);
                ++cleaned;
            }
            else
            {
                ++it;
            }
        }
        return cleaned;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, PooledObject> objects_;
};
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_OBJECT_POOL_HPP