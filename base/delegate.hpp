# pragma once
# ifndef HSBA_SLICER_DELEGATE_HPP

# define HSBA_SLICER_DELEGATE_HPP

#include <utility>
#include <functional>
#include <vector>
#include <shared_mutex>
#include <algorithm>

#include "concepts.hpp"

namespace HsBa::Slicer::Utils
{	
	/**
	 * @brief A delegate is a type that represents references to methods with a specific parameter list and return type.
	 * @tparam R return type of the delegate
	 * @tparam ...Args parameter types of the delegate
	 */
	template<typename R, typename ...Args>
	class Delegate
	{
	public:
		using Callback = std::function<R(Args...)>;
		Delegate() = default;
		template<typename CallbackType>
		    requires std::invocable<CallbackType, Args...>
		void Add(CallbackType&& callback)
		{
			std::lock_guard lock(mutex_);
			callbacks_.emplace_back(std::forward<CallbackType>(callback));
		}
		[[deprecated("std::functional has not operator== function")]]
		void Remove(const Callback& callback)
		{
			std::lock_guard lock(mutex_);
			auto it = std::remove_if(callbacks_.begin(), callbacks_.end(),
				[&callback](const Callback& cb) { return cb.target_type() == callback.target_type(); });
			if (it != callbacks_.end())
			{
				callbacks_.erase(it, callbacks_.end());
			}
		}
		R Invoke(Args&&... args)
		{
			std::vector<std::function<R(Args...)>> callbacks;
			{
				std::lock_guard lock(mutex_);
				callbacks = callbacks_;
			}
			if constexpr (std::is_void_v<R>)
			{
				for (const auto& callback : callbacks)
				{
					callback(std::forward<Args>(args)...);
				}
				return;
			}
			else if constexpr(Addable<R>)
			{
				R result{};
				for (const auto& callback : callbacks)
				{
					result = result + callback(std::forward<Args>(args)...);
				}
				return result;
			}
			else
			{
				R result{};
				for (const auto& callback : callbacks)
				{
					result = callback(std::forward<Args>(args)...);
				}
				return result;
			}
		}
		bool empty() const
		{
			std::shared_lock lock(mutex_);
			return callbacks_.empty();
		}
		void Clear()
		{
			std::lock_guard lock(mutex_);
			callbacks_.clear();
		}
		size_t size() const
		{
			std::shared_lock lock(mutex_);
			return callbacks_.size();
		}
		Delegate(const Delegate&) = delete;
		Delegate& operator=(const Delegate&) = delete;
		Delegate(Delegate&&) = default;
		Delegate& operator=(Delegate&&) = default;
		template<typename CallbackType>
		    requires std::invocable<CallbackType, Args...>
		Delegate& operator+=(CallbackType&& callback)
		{
			Add(callback);
			return *this;
		}
		[[deprecated]]
		Delegate& operator-=(const Callback& callback)
		{
			Remove(callback);
			return *this;
		}
		R operator()(Args&&... args)
		{
			return Invoke(std::forward<Args>(args)...);
		}
		auto begin() const
		{
			std::shared_lock lock(mutex_);
			return callbacks_.begin();
		}
		auto end() const
		{
			std::shared_lock lock(mutex_);
			return callbacks_.end();
		}
	private:
		mutable std::shared_mutex mutex_;
		std::vector<Callback> callbacks_;
	};

	template<typename R, typename... Args>
	    requires (sizeof...(Args) > 0)
	class Event final
	{
	public:
		using DelegateType = Delegate<R, Args...>;
		using Callback = typename DelegateType::Callback;
		template<typename T,typename, typename...>
		friend class EventSource;
	private:
		Event() = default;
		Event(const Event&) = delete;
		Event& operator=(const Event&) = delete;
		Event(Event&&) = default;
		Event& operator=(Event&&) = default;
		DelegateType delegate_;
		R Invoke(Args... args)
		{
			return delegate_.Invoke(std::forward<Args>(args)...);
		}
	};

	template<typename Devired, typename R, typename... Args>
	class EventSource
	{
	public:
		template<typename CallbackType>
		    requires std::invocable<CallbackType, Args...>
		void Add(CallbackType&& callback)
		{
			event_.delegate_.Add(std::forward<CallbackType>(callback));
		}
		[[deprecated("std::functional has not operator== function")]]
		void Remove(const typename Event<R, Args...>::Callback& callback)
		{
			event_.delegate_.Remove(callback);
		}
		template<typename CallbackType>
		    requires std::invocable<CallbackType, Args...>
		void operator+=(CallbackType&& callback)
		{
			Add(callback);
		}
		[[deprecated]]
		void operator-=(const typename Event<R, Args...>::Callback& callback)
		{
			Remove(callback);
		}
	protected:
		EventSource() = default;
		~EventSource() = default;
		Event<R, Args...> event_;
		R RaiseEvent(Args... args)
		{
			return event_.Invoke(args...);
		}
	};
}

#endif // !HSBA_SLICER_DELEGATE_HPP
