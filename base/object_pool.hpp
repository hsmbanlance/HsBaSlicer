#pragma once
#ifndef HSBA_SLICER_OBJECT_POOL_HPP
#define HSBA_SLICER_OBJECT_POOL_HPP

#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "error.hpp"

namespace HsBa::Slicer
{
	template <typename T, std::size_t MaxSize>
	class NamedObjectPool 
	{
	public:
		using ObjectType = T;
		using ObjectPtr = std::shared_ptr<T>;
		using WeakPtr = std::weak_ptr<T>;

		static constexpr std::size_t max_size = MaxSize;


		struct PooledObject 
		{
			ObjectPtr shared_ref;
			WeakPtr weak_ref;

			explicit PooledObject(const ObjectPtr& ref) : shared_ref(ref), weak_ref(ref) {}

			bool isInactive() const 
			{
				return shared_ref.use_count() == 1;
			}

			long useCount() const 
			{
				return shared_ref.use_count();
			}
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

		template <typename... Args>
		ObjectPtr emplace(const std::string& name, Args&&... args) 
		{
			std::unique_lock<std::shared_mutex> lock(mutex_);

			if (objects_.find(name) != objects_.end()) 
			{
				throw InvalidArgumentError("Object with name '" + name + "' already exists in pool");
			}
			// only cleanup when trying to add a new object and the pool is already at max capacity, otherwise we might end up in a situation where the pool is constantly cleaning up inactive objects even when there are still active ones, which would lead to performance issues and unexpected behavior
			if (objects_.size() >= MaxSize)
				CleanupInactiveObjects();

			if (objects_.size() >= MaxSize) 
			{
				throw RuntimeError("Pool is full (max size: " + std::to_string(MaxSize) +
					") and no inactive objects available");
			}

			ObjectPtr ptr = std::make_shared<T>(std::forward<Args>(args)...);

			objects_.emplace(name, PooledObject{ ptr });

			return ptr;
		}

		template<typename Allocator, typename... Args>
		ObjectPtr allocate(const std::string& name, Allocator&& alloc, Args&&... args) 
		{
			std::unique_lock<std::shared_mutex> lock(mutex_);
			if (objects_.find(name) != objects_.end()) 
			{
				throw InvalidArgumentError("Object with name '" + name + "' already exists in pool");
			}
			// only cleanup when trying to add a new object and the pool is already at max capacity, otherwise we might end up in a situation where the pool is constantly cleaning up inactive objects even when there are still active ones, which would lead to performance issues and unexpected behavior
			if(objects_.size() >= MaxSize)
				CleanupInactiveObjects();
			if (objects_.size() >= MaxSize) 
			{
				throw RuntimeError("Pool is full (max size: " + std::to_string(MaxSize) +
					") and no inactive objects available");
			}
			ObjectPtr ptr = std::allocate_shared<T>(std::forward<Allocator>(alloc), std::forward<Args>(args)...);
			objects_.emplace(name, PooledObject{ ptr });
			return ptr;
		}


		ObjectPtr get(const std::string& name) {
			std::shared_lock<std::shared_mutex> lock(mutex_);

			auto it = objects_.find(name);
			if (it != objects_.end()) {

				return it->second.shared_ref;
			}
			return nullptr;
		}

		ObjectPtr operator[](const std::string& name) 
		{
			return get(name);
		}


		bool Contains(const std::string& name) const 
		{
			std::shared_lock<std::shared_mutex> lock(mutex_);
			return objects_.find(name) != objects_.end();
		}


		std::size_t size() const 
		{
			std::shared_lock<std::shared_mutex> lock(mutex_);
			return objects_.size();
		}


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


		std::size_t ActiveCount() const 
		{
			return size() - InactiveCount();
		}


		std::size_t Cleanup() 
		{
			std::unique_lock<std::shared_mutex> lock(mutex_);
			return CleanupInactiveObjects();
		}

		std::vector<std::string> GetNames() const 
		{
			std::shared_lock<std::shared_mutex> lock(mutex_);
			std::vector<std::string> names;
			names.reserve(objects_.size());
			for (const auto& [name, _] : objects_) {
				names.push_back(name);
			}
			return names;
		}

	private:

		std::size_t CleanupInactiveObjects() 
		{
			std::size_t cleaned = 0;
			for (auto it = objects_.begin(); it != objects_.end(); ) 
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
} // namespace HsBa::Slicer

#endif // !HSBA_SLICER_OBJECT_POOL_HPP