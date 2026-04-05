#pragma once
#ifndef HSBA_SLICER_MEMORY_POOL_HPP
#define HSBA_SLICER_MEMORY_POOL_HPP

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <new>
#include <mutex>

namespace HsBa::Slicer
{
	template <size_t PoolSize>
	struct MemoryPoolState
	{
		alignas(std::max_align_t) char pool[PoolSize];
		bool used[PoolSize] = { false };
		size_t used_count = 0;
	};
	template <typename T, size_t PoolSize>
	class MemoryPool
	{
	public:
		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_move_assignment = std::false_type;
		using propagate_on_container_swap = std::false_type;
		using is_always_equal = std::false_type;

		template <typename U>
		struct rebind
		{
			using other = MemoryPool<U, PoolSize>;
		};

		MemoryPool() : state_(std::make_shared<MemoryPoolState<PoolSize>>()) {}
		~MemoryPool() = default;
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
		MemoryPool(const MemoryPool<U, PoolSize>& other) : state_(other.get_state()) {}

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

		size_type max_size() const noexcept
		{
			return PoolSize / sizeof(T);
		}
		template <typename U, typename... Args>
		void construct(U* p, Args&&... args)
		{
			::new(static_cast<void*>(p)) U(std::forward<Args>(args)...);
		}

		template <typename U>
		void destroy(U* p)
		{
			p->~U();
		}
		bool operator==(const MemoryPool& other) const noexcept
		{
			return state_ == other.state_;
		}

		bool operator!=(const MemoryPool& other) const noexcept
		{
			return !(*this == other);
		}

		const std::shared_ptr<MemoryPoolState<PoolSize>>& get_state() const noexcept
		{
			return state_;
		}
	private:
		std::shared_ptr<MemoryPoolState<PoolSize>> state_;
		std::mutex mutex_;
	};

	template <typename T, size_t PoolSize>
	class StaticMemoryPool
	{
	public:
		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_copy_assignment = std::true_type;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_swap = std::true_type;
		using is_always_equal = std::true_type;

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
		StaticMemoryPool(const StaticMemoryPool<U, PoolSize>&) {}

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

		size_type max_size() const noexcept
		{
			return PoolSize / sizeof(T);
		}

		template <typename U, typename... Args>
		void construct(U* p, Args&&... args)
		{
			::new(static_cast<void*>(p)) U(std::forward<Args>(args)...);
		}

		template <typename U>
		void destroy(U* p)
		{
			p->~U();
		}

		bool operator==(const StaticMemoryPool&) const noexcept
		{
			return true; // All instances are considered equal
		}

		bool operator!=(const StaticMemoryPool&) const noexcept
		{
			return false; // All instances are considered equal
		}
		size_t UsedCount() const
		{
			std::lock_guard<std::mutex> lock(mutex_);
			return used_count;
		}
	private:
		alignas(std::max_align_t) inline static char buffer[PoolSize];
		inline static bool used[PoolSize] = { false };
		inline static size_t used_count = 0;
		inline static std::mutex mutex_;
	};
}

#endif // !HSBA_SLICER_MEMORY_POOL_HPP