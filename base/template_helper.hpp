#pragma once
#ifndef HSBA_SLICER_TEMPLATE_HELPHER_HPP
#define HSBA_SLICER_TEMPLATE_HELPHER_HPP

#include <utility>
#include <algorithm>
#include <type_traits>
#include <concepts>
#include <future>
#include <set>
#include <vector>
#include <list>
#include <unordered_set>
#include <optional>
#include <exception>
#include <ranges>

#if __cpp_lib_coroutine && __cpp_impl_coroutine
#include <coroutine>
#endif
#ifdef __cpp_lib_generator
#include <generator>
#endif // __cpp_lib_generator

#include "concepts.hpp"
#include "error.hpp"

namespace HsBa::Slicer::Utils
{
	template<typename T, size_t N>
	struct Template_Helper
	{
		inline constexpr Template_Helper(const T(&arr)[N])
		{
			std::copy(arr, arr + N, this->arr);
		}
		T arr[N];
	};


	/// <summary>
	/// template call
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="...Args">function arguments</typeparam>
	/// <typeparam name="th">template helper</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="...args">arguments</param>
	/// <returns>callback return</returns>
	template<Template_Helper th, typename Callback, typename... Args>
		requires std::invocable<Callback, decltype(th.arr), Args...>
	inline auto template_call(Callback&& callback, Args&&... args)
	{
		return std::forward<Callback>(callback)(th.arr, std::forward<Args>(args)...);
	}

	/// <summary>
	/// invoke function
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="...Args">function arguments</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="...args">callback arguments</param>
	/// <returns>callback return</returns>
	template<typename Callback, typename... Args>
		requires std::invocable<Callback, Args...>
	inline constexpr auto Invoke(Callback&& callback, Args&&... args)
	{
		return std::forward<Callback>(callback)(std::forward<Args>(args)...);
	}

	/// <summary>
	/// async invoke function
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="...Args">function arguments</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="...args">callback arguments</param>
	/// <returns>future</returns>
	template<typename Callback, typename... Args>
		requires std::invocable<Callback, Args...>
	inline auto AsyncInvoke(Callback&& callback, Args&&... args)
	{
		return std::async(std::launch::async, std::forward<Callback>(callback), std::forward<Args>(args)...);
	}

#if __cpp_lib_coroutine && __cpp_impl_coroutine
	/// <summary>
	/// coroutine executor interface
	/// </summary>
	class IExecutor
	{
	public:
		IExecutor() = default;
		virtual ~IExecutor() = default;
		virtual void execute(std::function<void()>&& func) = 0;
	};
	/// <summary>
	/// coroutine executor noop, do nothing
	/// </summary>
	class NoopExecutor : public IExecutor
	{
	public:
		inline void execute(std::function<void()>&& func) override
		{
			func();
		}
	};
	/// <summary>
	/// coroutine executor new thread
	/// </summary>
	class NewThreadExecutor : public IExecutor
	{
	public:
		inline void execute(std::function<void()>&& func) override
		{
			if (!func)
			{
				throw HsBa::Slicer::InvalidArgumentError("Null function");
			}
			std::thread thread(std::move(func));
			if (thread.joinable())
			{
				thread.detach();
			}
		}
	};
	/// <summary>
	/// coroutine executor async
	/// </summary>
	class AsyncExecutor : public IExecutor
	{
	public:
		inline void execute(std::function<void()>&& func) override
		{
			auto future = std::async(std::launch::async, func);
		}
	};
	template<typename T, typename Executor = AsyncExecutor>
		requires ((std::movable<T>&& std::default_initializable<T>) || std::is_void_v<T>)
	&& std::derived_from<Executor, IExecutor>
		class Task;
	template<typename Executor>
	class Task<void, Executor>;
	/// <summary>
	/// coroutine taskawaiter
	/// </summary>
	/// <typeparam name="T">coroutine return type</typeparam>
	/// <typeparam name="Executor">coroutine executor</typeparam>
	template<typename T, typename Executor>
		requires (std::movable<T> || std::is_void_v<T>) && std::derived_from<Executor, IExecutor>
	class TaskAwaiter
	{
		friend class Task<T, Executor>;
	public:
		inline explicit TaskAwaiter(IExecutor* executor, Task<T, Executor>&& task) noexcept
			:executor_(executor), task_(std::move(task)) {}
		TaskAwaiter(TaskAwaiter&& o) { *this = std::move(o); }
		inline TaskAwaiter& operator=(TaskAwaiter&& o)
		{
			task_(std::exchange(task_), {});
		}
		inline constexpr bool await_ready() const noexcept { return task_.handle_.done(); }
		inline void await_suspend(std::coroutine_handle<> handle) noexcept
		{
			if (!await_ready())
			{
				task_.finally([handle, this]() {
					executor_->execute([handle] {handle.resume(); }); });
			}
		}
		inline T await_resume() noexcept
		{
			if constexpr (!std::is_void_v<T>)
			{
				return task_.get_result();
			}
			else
			{
				task_.get_result();
			}
		}
	private:
		Task<T, Executor> task_;
		IExecutor* executor_;
	};
	template<typename T, typename Executor = AsyncExecutor, typename Allocator = std::allocator<T>>
		requires std::default_initializable<T>&& std::movable<T>&& std::derived_from<Executor, IExecutor>&& TAllocator<T, Allocator>
	class CustomAllocatorTask;
	/// <summary>
	/// coroutine task awaiter with custom allocator
	/// </summary>
	/// <typeparam name="T">coroutine return type</typeparam>
	/// <typeparam name="Executor">coroutine executor</typeparam>
	/// <typeparam name="Allocator">coroutine allocator</typeparam>
	template<typename T, typename Executor, typename Allocator>
		requires std::movable<T>&& std::derived_from<Executor, IExecutor>&& TAllocator<T, Allocator>
	class CustomAllocatorTaskAwaiter
	{
		friend class CustomAllocatorTask<T, Executor, Allocator>;
	public:
		inline explicit CustomAllocatorTaskAwaiter(IExecutor* executor, CustomAllocatorTask<T, Executor, Allocator>&& task) noexcept
			:executor_(executor), task_(std::move(task)) {}
		CustomAllocatorTaskAwaiter(CustomAllocatorTaskAwaiter&& o) { *this = std::move(o); }
		inline CustomAllocatorTaskAwaiter& operator=(CustomAllocatorTaskAwaiter&& o)
		{
			task_(std::exchange(task_), {});
		}
		inline constexpr bool await_ready() const noexcept { return task_.handle_.done(); }
		inline void await_suspend(std::coroutine_handle<> handle) noexcept
		{
			if (!await_ready())
			{
				task_.finally([handle, this]() {
					executor_->execute([handle] {handle.resume(); }); });
			}
		}
		inline T await_resume() noexcept
		{
			if constexpr (!std::is_void_v<T>)
			{
				return task_.get_result();
			}
			else
			{
				task_.get_result();
			}
		}
	private:
		CustomAllocatorTask<T, Executor, Allocator> task_;
		IExecutor* executor_;
	};
	/// <summary>
	/// coroutine dispatchawaiter
	/// </summary>
	class DispatchAwaiter
	{
	public:
		inline explicit DispatchAwaiter(IExecutor* executor) noexcept
			: executor_(executor) {}

		inline bool await_ready() const { return false; }
		inline void await_suspend(std::coroutine_handle<> handle) const
		{
			executor_->execute([handle]() {
				handle.resume();
				});
		}
		inline void await_resume() {}
	private:
		IExecutor* executor_;
	};

	/// <summary>
	/// coroutine task
	/// </summary>
	/// <typeparam name="T">task return type</typeparam>
	/// <typeparam name="Executor">coroutine task executor</typeparam>
	template<typename T, typename Executor>
		requires ((std::movable<T>&& std::default_initializable<T>) || std::is_void_v<T>)
	&& std::derived_from<Executor, IExecutor>
		class Task
	{
		friend class TaskAwaiter<T, Executor>;
	public:
		/// <summary>
		/// coroutines task result
		/// </summary>
		class Result
		{
		public:
			explicit Result() = default;
			inline explicit Result(T&& value) :value_(std::move(value)) {}
			inline explicit Result(std::exception_ptr&& ex) :exception_(std::move(ex)) {}
			inline T get_or_throw()
			{
				if (exception_)
				{
					std::rethrow_exception(exception_);
				}
				return value_;
			}
		private:
			T value_{};
			std::exception_ptr exception_;
		};
		struct promise_type
		{
			inline DispatchAwaiter initial_suspend() { return DispatchAwaiter{ &executor_ }; }
			inline std::suspend_always final_suspend() noexcept { return {}; }
			inline Task<T, Executor> get_return_object()
			{
				return Task{ std::coroutine_handle<promise_type>::from_promise(*this) };
			}
			inline void unhandled_exception()
			{
				std::lock_guard lock(mutex_);
				result_ = Result(std::current_exception());
				cv_.notify_all();
				notify_callbacks();
			}
			inline void return_value(T value) // must return_value
			{
				std::lock_guard lock(mutex_);
				result_ = Result(std::move(value));
				cv_.notify_all();
				notify_callbacks();
			}
			template<typename ...Args>
			inline void return_value(Args... args)
			{
				std::lock_guard lock(mutex_);
				result_ = Result(std::move(T{ args... }));
				cv_.notify_all();
				notify_callbacks();
			}
			template<typename R, typename Exe>
				requires ((std::movable<R>&& std::default_initializable<R>) || std::is_void_v<R>)
			&& std::derived_from<Exe, IExecutor>
				inline TaskAwaiter<R, Exe> await_transform(Task<R, Exe>&& task)
			{
				return TaskAwaiter<R, Exe>(&executor_, std::move(task));
			}
			template<typename R, typename Exe, typename Allo>
				requires std::default_initializable<R>&& std::movable<R>&& std::derived_from<Exe, IExecutor>&& TAllocator<R, Allo>
			inline CustomAllocatorTaskAwaiter<R, Exe, Allo> await_transform(CustomAllocatorTask<R, Exe, Allo>&& task)
			{
				return CustomAllocatorTaskAwaiter<R, Exe, Allo>(&executor_, std::move(task));
			}
			inline T get_result()
			{
				std::unique_lock lock(mutex_);
				if (!result_.has_value())
				{
					cv_.wait(lock);
				}
				return result_->get_or_throw();
			}
			inline void on_completed(std::function<void(Result)>&& func)
			{
				std::unique_lock lock(mutex_);
				if (result_.has_value())
				{
					auto value = result_.value();
					lock.unlock();
					func(value);
				}
				else
				{
					callbacks_.push_back(func);
				}
			}
		private:
			Executor executor_;
			std::optional<Result> result_;
			std::mutex mutex_;
			std::condition_variable cv_;
			std::list<std::function<void(Result)>> callbacks_;
			inline void notify_callbacks()
			{
				auto value = result_.value();
				for (auto& callback : callbacks_)
				{
					callback(value);
				}
				callbacks_.clear();
			}
		};

		using Handle = std::coroutine_handle<promise_type>;

		inline T get_result()
		{
			return handle_.promise().get_result();
		}

		inline Task& then(std::function<void(T)>&& func)
		{
			handle_.promise().on_completed([func](auto result) {
				try {
					func(result.get_or_throw());
				}
				catch (std::exception& e) {
				}
				});
			return *this;
		}

		inline Task& catching(std::function<void(std::exception&)>&& func) {
			handle_.promise().on_completed([func](auto result) {
				try {
					result.get_or_throw();
				}
				catch (std::exception& e) {
					func(e);
				}
				});
			return *this;
		}

		inline Task& finally(std::function<void()>&& func)
		{
			handle_.promise().on_completed([func](auto result) { func(); });
			return *this;
		}

		inline explicit Task(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}

		inline Task(Task&& task) noexcept : handle_(std::exchange(task.handle_, {})) {}

		Task(Task&) = delete;

		Task& operator=(Task&) = delete;

		inline ~Task()
		{
			if (handle_)
				handle_.destroy();
		}

	private:
		Handle handle_;
	};

	/// <summary>
	/// coroutine task with return void
	/// </summary>
	/// <typeparam name="Executor">coroutine executor</typeparam>
	template<typename Executor>
	class Task<void, Executor>
	{
		friend class TaskAwaiter<void, Executor>;
	public:
		/// <summary>
		/// task result
		/// </summary>
		class Result
		{
		public:
			explicit Result() = default;
			inline explicit Result(std::exception_ptr&& ex) :exception_(std::move(ex)) {}
			inline void get_or_throw()
			{
				if (exception_)
				{
					std::rethrow_exception(exception_);
				}
				return;
			}
		private:
			std::exception_ptr exception_;
		};
		struct promise_type
		{
			inline DispatchAwaiter initial_suspend() { return DispatchAwaiter{ &executor_ }; }
			inline std::suspend_always final_suspend() noexcept { return {}; }
			inline Task<void, Executor> get_return_object()
			{
				return Task{ std::coroutine_handle<promise_type>::from_promise(*this) };
			}
			inline void unhandled_exception()
			{
				std::lock_guard lock(mutex_);
				result_ = Result(std::current_exception());
				cv_.notify_all();
				notify_callbacks();
			}
			inline void return_void() // must return_void
			{
				std::lock_guard lock(mutex_);
				result_ = Result();
				cv_.notify_all();
				notify_callbacks();
			}
			template<typename R, typename Exe>
				requires ((std::movable<R>&& std::default_initializable<R>) || std::is_void_v<R>)
			&& std::derived_from<Exe, IExecutor>
				inline TaskAwaiter<R, Exe> await_transform(Task<R, Exe>&& task)
			{
				return TaskAwaiter<R, Exe>(&executor_, std::move(task));
			}
			template<typename R, typename Exe, typename Allo>
				requires std::default_initializable<R>&& std::movable<R>&& std::derived_from<Exe, IExecutor>&& TAllocator<R, Allo>
			inline CustomAllocatorTaskAwaiter<R, Exe, Allo> await_transform(CustomAllocatorTask<R, Exe, Allo>&& task)
			{
				return CustomAllocatorTaskAwaiter<R, Exe, Allo>(&executor_, std::move(task));
			}
			inline void get_result()
			{
				std::unique_lock lock(mutex_);
				if (!result_.has_value())
				{
					cv_.wait(lock);
				}
				result_->get_or_throw();
			}
			inline void on_completed(std::function<void(Result)>&& func)
			{
				std::unique_lock lock(mutex_);
				if (result_.has_value())
				{
					auto value = result_.value();
					lock.unlock();
					func(value);
				}
				else
				{
					callbacks_.push_back(func);
				}
			}
		private:
			Executor executor_;
			std::optional<Result> result_;
			std::mutex mutex_;
			std::condition_variable cv_;
			std::list<std::function<void(Result)>> callbacks_;
			inline void notify_callbacks()
			{
				auto value = result_.value();
				for (auto& callback : callbacks_)
				{
					callback(value);
				}
				callbacks_.clear();
			}
		};

		using Handle = std::coroutine_handle<promise_type>;

		inline void get_result()
		{
			handle_.promise().get_result();
		}

		inline Task& then(std::function<void()>&& func)
		{
			handle_.promise().on_completed([func](auto result) {
				try {
					result.get_or_throw();
					func();
				}
				catch (std::exception&) {
				}
				});
			return *this;
		}

		inline Task& catching(std::function<void(std::exception&)>&& func) {
			handle_.promise().on_completed([func](auto result) {
				try {
					result.get_or_throw();
				}
				catch (std::exception& e) {
					func(e);
				}
				});
			return *this;
		}

		inline Task& finally(std::function<void()>&& func)
		{
			handle_.promise().on_completed([func](auto result) { func(); });
			return *this;
		}

		inline explicit Task(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}

		inline Task(Task&& task) noexcept : handle_(std::exchange(task.handle_, {})) {}

		Task(Task&) = delete;

		Task& operator=(Task&) = delete;

		inline ~Task()
		{
			if (handle_)
				handle_.destroy();
		}

	private:
		Handle handle_;
	};
	/// <summary>
	/// coroutines task with custom allocator
	/// </summary>
	/// <typeparam name="T">task result type</typeparam>
	/// <typeparam name="Executor">executor type</typeparam>
	/// <typeparam name="Allocator">custom allocator type</typeparam>
	template<typename T, typename Executor, typename Allocator>
		requires std::default_initializable<T>&& std::movable<T>&& std::derived_from<Executor, IExecutor>&& TAllocator<T, Allocator>
	class CustomAllocatorTask
	{
		friend class CustomAllocatorTaskAwaiter<T, Executor, Allocator>;
	public:
		/// <summary>
		/// coroutines task result
		/// </summary>
		class Result
		{
		public:
			explicit Result() = default;
			inline explicit Result(T&& value) :value_(std::move(value)) {}
			inline explicit Result(std::exception_ptr&& ex) :exception_(std::move(ex)) {}
			inline T get_or_throw()
			{
				if (exception_)
				{
					std::rethrow_exception(exception_);
				}
				return value_;
			}
		private:
			T value_{};
			std::exception_ptr exception_;
		};
		struct promise_type
		{
			inline DispatchAwaiter initial_suspend() { return DispatchAwaiter{ &executor_ }; }
			inline std::suspend_always final_suspend() noexcept { return {}; }
			inline CustomAllocatorTask<T, Executor> get_return_object()
			{
				return CustomAllocatorTask{ std::coroutine_handle<promise_type>::from_promise(*this) };
			}
			inline void unhandled_exception()
			{
				std::lock_guard lock(mutex_);
				result_ = Result(std::current_exception());
				cv_.notify_all();
				notify_callbacks();
			}
			inline void return_value(T value) // must return_value
			{
				std::lock_guard lock(mutex_);
				result_ = Result(std::move(value));
				cv_.notify_all();
				notify_callbacks();
			}
			template<typename ...Args>
			inline void return_value(Args... args)
			{
				std::lock_guard lock(mutex_);
				result_ = Result(std::move(T{ args... }));
				cv_.notify_all();
				notify_callbacks();
			}
			template<typename R, typename Exe>
				requires ((std::movable<R>&& std::default_initializable<R>) || std::is_void_v<R>)
			&& std::derived_from<Exe, IExecutor>
			inline TaskAwaiter<R, Exe> await_transform(Task<R, Exe>&& task)
			{
				return TaskAwaiter<R, Exe>(&executor_, std::move(task));
			}
			template<typename R, typename Exe, typename Allo>
				requires std::default_initializable<R>&& std::movable<R>&& std::derived_from<Exe, IExecutor>&& TAllocator<R, Allo>
			inline CustomAllocatorTaskAwaiter<R, Exe, Allo> await_transform(CustomAllocatorTask<R, Exe, Allo>&& task)
			{
				return CustomAllocatorTaskAwaiter<R, Exe, Allo>(&executor_, std::move(task));
			}
			inline T get_result()
			{
				std::unique_lock lock(mutex_);
				if (!result_.has_value())
				{
					cv_.wait(lock);
				}
				return result_->get_or_throw();
			}
			inline void on_completed(std::function<void(Result)>&& func)
			{
				std::unique_lock lock(mutex_);
				if (result_.has_value())
				{
					auto value = result_.value();
					lock.unlock();
					func(value);
				}
				else
				{
					callbacks_.push_back(func);
				}
			}

			inline static Allocator alloc;
			void* operator new(size_t size)
			{
				return alloc.allocate(size);
			}

			void operator delete(void* p, size_t size) noexcept
			{
				alloc.deallocate(static_cast<T*>(p), size);
			}
		private:
			Executor executor_;
			std::optional<Result> result_;
			std::mutex mutex_;
			std::condition_variable cv_;
			std::list<std::function<void(Result)>> callbacks_;
			inline void notify_callbacks()
			{
				auto value = result_.value();
				for (auto& callback : callbacks_)
				{
					callback(value);
				}
				callbacks_.clear();
			}
		};

		using Handle = std::coroutine_handle<promise_type>;

		inline T get_result()
		{
			return handle_.promise().get_result();
		}

		inline CustomAllocatorTask& then(std::function<void(T)>&& func)
		{
			handle_.promise().on_completed([func](auto result) {
				try {
					func(result.get_or_throw());
				}
				catch (std::exception& e) {
				}
				});
			return *this;
		}

		inline CustomAllocatorTask& catching(std::function<void(std::exception&)>&& func) {
			handle_.promise().on_completed([func](auto result) {
				try {
					result.get_or_throw();
				}
				catch (std::exception& e) {
					func(e);
				}
				});
			return *this;
		}

		inline CustomAllocatorTask& finally(std::function<void()>&& func)
		{
			handle_.promise().on_completed([func](auto result) { func(); });
			return *this;
		}

		inline void static SetAllocator(const Allocator& allocator)
		{
			promise_type::alloc = allocator;
		}

		inline explicit CustomAllocatorTask(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}

		inline CustomAllocatorTask(CustomAllocatorTask&& task) noexcept : handle_(std::exchange(task.handle_, {})) {}

		CustomAllocatorTask(CustomAllocatorTask&) = delete;

		CustomAllocatorTask& operator=(CustomAllocatorTask&) = delete;

		inline ~CustomAllocatorTask()
		{
			if (handle_)
				handle_.destroy();
		}

	private:
		Handle handle_;
	};
#endif // __cpp_lib_coroutine && __cpp_impl_coroutine

#if __cpp_lib_coroutine && __cpp_impl_coroutine

	// personal couroutine Generator
	// see it in https://zh.cppreference.com/w/cpp/coroutine/coroutine_handle

	/// <summary>
	/// personal coroutine Generator
	/// </summary>
	/// <typeparam name="T">yield type</typeparam>
	template<std::movable T>
	class Generator
	{
	public:
		struct promise_type
		{
			using return_type = Generator<T>;
			inline return_type get_return_object()
			{
				return Generator{ Handle::from_promise(*this) };
			}
			inline static std::suspend_always initial_suspend() noexcept
			{
				return {};
			}
			inline static std::suspend_always final_suspend() noexcept
			{
				return {};
			}
			inline std::suspend_always yield_value(T value) noexcept
			{
				current_value = std::move(value);
				return {};
			}
			template<typename ...Args>
			inline std::suspend_always yield_value(Args... args) noexcept
			{
				current_value = std::move(T{ args... });
				return {};
			}
			inline void return_void() {} // return_void for co_return
			void await_transform() = delete;
			template<typename R, typename Exe>
				requires ((std::movable<R>&& std::default_initializable<R>) || std::is_void_v<R>)
			&& std::derived_from<Exe, IExecutor>
				inline TaskAwaiter<R, Exe> await_transform(Task<R, Exe>&& task)
			{
				Exe exe{};
				return TaskAwaiter<R, Exe>(&exe, std::move(task));
			}
			template<typename R, typename Exe, typename Allo>
				requires std::default_initializable<R>&& std::movable<R>&& std::derived_from<Exe, IExecutor>&& TAllocator<R, Allo>
			inline CustomAllocatorTaskAwaiter<R, Exe, Allo> await_transform(CustomAllocatorTask<R, Exe, Allo>&& task)
			{
				Exe exe{};
				return CustomAllocatorTaskAwaiter<R, Exe, Allo>(&exe, std::move(task));
			}
			[[noreturn]]
			inline static void unhandled_exception()
			{
				onCancel();
				throw std::runtime_error("unhandled exception in coroutine");
			}

			std::optional<T> current_value;
		};

		using Handle = std::coroutine_handle<promise_type>;

		inline explicit Generator(const Handle coroutine) :
			coroutine_{ coroutine }
		{}

		Generator() = default;
		inline ~Generator()
		{
			if (coroutine_)
				coroutine_.destroy();
		}

		Generator(const Generator&) = delete;
		Generator& operator=(const Generator&) = delete;

		inline Generator(Generator&& other) noexcept :
			coroutine_{ other.coroutine_ }
		{
			other.coroutine_ = {};
		}
		inline Generator& operator=(Generator&& other) noexcept
		{
			if (this != &other)
			{
				if (coroutine_)
					coroutine_.destroy();
				coroutine_ = other.coroutine_;
				other.coroutine_ = {};
			}
			return *this;
		}
		/// <summary>
		/// generator iterator
		/// </summary>
		class iterator
		{
		public:
			iterator& operator++()
			{
				if (coroutine_) coroutine_.resume();
				return *this;
			}
			inline const T& operator*() const
			{
				return *coroutine_.promise().current_value;
			}
			inline bool operator==(std::default_sentinel_t) const
			{
				return !coroutine_ || coroutine_.done();
			}

			inline explicit iterator(const Handle coroutine) :
				coroutine_{ coroutine }
			{}

		private:
			Handle coroutine_;
		};

		inline iterator begin()
		{
			if (coroutine_)
				coroutine_.resume();
			return iterator{ coroutine_ };
		}

		inline const std::default_sentinel_t end() noexcept { return {}; } // end() is a default sentinel

		inline static void SetOnCancel(std::function<void()> onCancel)
		{
			Generator::onCancel = onCancel;
		}

	private:

		Handle coroutine_;
		inline static std::function<void()> onCancel = []() {};

	};

	/// <summary>
	/// personal coroutine Generator with custom allocator, allocator is static member in promise_type
	/// promise_type new and delete use allocator, so allocator must be static
	/// </summary>
	/// <typeparam name="Allocator">allocator type, default is std::allocator</typeparam>
	/// <typeparam name="T">yield type</typeparam>
	template<std::movable T, typename Allocator = std::allocator<T>>
		requires TAllocator<T, Allocator>
	class CustomAllocatorGenerator
	{
	public:
		struct promise_type
		{
			using return_type = CustomAllocatorGenerator<T, Allocator>;
			inline return_type get_return_object()
			{
				return CustomAllocatorGenerator{ Handle::from_promise(*this) };
			}
			inline static std::suspend_always initial_suspend() noexcept
			{
				return {};
			}
			inline static std::suspend_always final_suspend() noexcept
			{
				return {};
			}
			inline std::suspend_always yield_value(T value) noexcept
			{
				current_value = std::move(value);
				return {};
			}
			template<typename ...Args>
			inline std::suspend_always yield_value(Args... args) noexcept
			{
				current_value = std::move(T{ args... });
				return {};
			}
			inline void return_void() {} // return_void for co_return
			void await_transform() = delete;
			template<typename R, typename Exe>
			inline TaskAwaiter<R, Exe> await_transform(Task<R, Exe>&& task)
			{
				Exe exe{};
				return TaskAwaiter<R, Exe>(&exe, std::move(task));
			}
			template<typename R, typename Exe, typename Allo>
			inline CustomAllocatorTaskAwaiter<R, Exe, Allo> await_transform(CustomAllocatorTask<R, Exe, Allo>&& task)
			{
				Exe exe{};
				return CustomAllocatorTaskAwaiter<R, Exe, Allo>(&exe, std::move(task));
			}
			[[noreturn]]
			inline static void unhandled_exception()
			{
				onCancel();
				throw std::runtime_error("unhandled exception in coroutine");
			}

			std::optional<T> current_value;
			inline static Allocator alloc;

			// new and delete for promise_type, force promise_type to use allocator
			// no get_return_object_on_allocation_failure function, because new operator isn't noexcept

			void* operator new(size_t size)
			{
				return alloc.allocate(size);
			}

			void operator delete(void* p, size_t size) noexcept
			{
				alloc.deallocate(static_cast<T*>(p), size);
			}
		};

		using Handle = std::coroutine_handle<promise_type>;

		inline explicit CustomAllocatorGenerator(const Handle coroutine) :
			coroutine_{ coroutine }
		{}

		CustomAllocatorGenerator() = default;
		inline ~CustomAllocatorGenerator()
		{
			if (coroutine_)
				coroutine_.destroy();
		}

		CustomAllocatorGenerator(const CustomAllocatorGenerator&) = delete;
		CustomAllocatorGenerator& operator=(const CustomAllocatorGenerator&) = delete;

		inline CustomAllocatorGenerator(CustomAllocatorGenerator&& other) noexcept :
			coroutine_{ other.coroutine_ }
		{
			other.coroutine_ = {};
		}
		inline CustomAllocatorGenerator& operator=(CustomAllocatorGenerator&& other) noexcept
		{
			if (this != &other)
			{
				if (coroutine_)
					coroutine_.destroy();
				coroutine_ = other.coroutine_;
				other.coroutine_ = {};
			}
			return *this;
		}
		inline void static SetAllocator(const Allocator& allocator)
		{
			promise_type::alloc = allocator;
		}
		/// <summary>
		/// generator iterator
		/// </summary>
		class iterator
		{
		public:
			iterator& operator++()
			{
				if (coroutine_) coroutine_.resume();
				return *this;
			}
			inline const T& operator*() const
			{
				return *coroutine_.promise().current_value;
			}
			inline bool operator==(std::default_sentinel_t) const
			{
				return !coroutine_ || coroutine_.done();
			}

			inline explicit iterator(const Handle coroutine) :
				coroutine_{ coroutine }
			{}

		private:
			Handle coroutine_;
		};

		inline iterator begin()
		{
			if (coroutine_)
				coroutine_.resume();
			return iterator{ coroutine_ };
		}

		inline const std::default_sentinel_t end() noexcept { return {}; } // end() is a default sentinel

		inline static void SetOnCancel(std::function<void()> onCancel)
		{
			CustomAllocatorGenerator::onCancel = onCancel;
		}

	private:

		Handle coroutine_;
		inline static std::function<void()> onCancel = []() {};

	};

	/// <summary>
	/// generator invoke list
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="Arg">list container value type</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="arg">list</param>
	/// <returns>returns Generator</returns>
	template<typename Callback, typename Arg>
		requires std::invocable<Callback, Arg>
	inline auto GeneratorInvoke(Callback&& callback, const std::list<Arg>& arg)
		-> Generator<std::invoke_result_t<Callback, Arg>>
	{
		for (const auto& i : arg)
		{
			co_yield std::forward<Callback>(callback)(i);
		}
	}
	/// <summary>
	/// generator invoke vector
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="Arg">vector container value type, bool is not supported</typeparam>
	/// <param name="arg">vector</param>
	/// <returns>returns Generator</returns>
	template<typename Callback, typename Arg>
		requires std::invocable<Callback, Arg>&& std::negation_v<std::is_same<Arg, bool>>
	inline auto GeneratorInvoke(Callback&& callback, const std::vector<Arg>& arg)
		-> Generator<std::invoke_result_t<Callback, Arg>>
	{
		for (const auto& i : arg)
		{
			co_yield std::forward<Callback>(callback)(i);
		}
	}
	/// <summary>
	/// generator invoke set
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="Arg">set container value type</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="arg">set</param>
	/// <returns>returns Generator</returns>
	template<typename Callback, typename Arg>
		requires std::invocable<Callback, Arg>
	inline auto GeneratorInvoke(Callback&& callback, const std::set<Arg>& arg)
		-> Generator<std::invoke_result_t<Callback, Arg>>
	{
		for (const auto& i : arg)
		{
			co_yield std::forward<Callback>(callback)(i);
		}
	}
	/// <summary>
	/// generator invoke unordered_set
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="Arg">unordered_set container value type</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="arg">unordered_set</param>
	/// <returns>returns Generator</returns>
	template<typename Callback, typename Arg>
		requires std::invocable<Callback, Arg>
	inline auto GeneratorInvoke(Callback&& callback, const std::unordered_set<Arg>& arg)
		-> Generator<std::invoke_result_t<Callback, Arg>>
	{
		for (const auto& i : arg)
		{
			co_yield std::forward<Callback>(callback)(i);
		}
	}
	/// <summary>
	/// generator invoke list with other argument
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="T">other argument type</typeparam>
	/// <typeparam name="Arg">list container value type</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="t">other argument</param>
	/// <param name="arg">list</param>
	/// <returns>returns Generator</returns>
	template<typename Callback, typename T, typename Arg>
		requires std::invocable<Callback, T, Arg>
	inline auto GeneratorInvoke(Callback&& callback, T&& t, const std::list<Arg>& arg)
		-> Generator<decltype(std::declval<Callback>()(std::declval<T>(), std::declval<Arg>()))>
	{
		for (const auto& i : arg)
		{
			co_yield std::forward<Callback>(callback)(std::forward<T>(t), i);
		}
	}
	/// <summary>
	/// generator invoke vector with other argument
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="T">other argument type</typeparam>
	/// <typeparam name="Arg">vector container value type, not bool</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="t">other argument</param>
	/// <param name="arg">vector</param>
	/// <returns>returns Generator</returns>
	template<typename Callback, typename T, typename Arg>
		requires std::invocable<Callback, T, Arg>&& std::negation_v<std::is_same<Arg, bool>>
	inline auto GeneratorInvoke(Callback&& callback, T&& t, const std::vector<Arg>& arg)
		-> Generator<decltype(std::declval<Callback>()(std::declval<T>(), std::declval<Arg>()))>
	{
		for (const auto& i : arg)
		{
			co_yield std::forward<Callback>(callback)(std::forward<T>(t), i);
		}
	}
	/// <summary>
	/// generator invoke set with other argument
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="T">other argument type</typeparam>
	/// <typeparam name="Arg">set container value type</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="t">other argument</param>
	/// <param name="arg">set</param>
	/// <returns>returns Generator</returns>
	template<typename Callback, typename T, typename Arg>
		requires std::invocable<Callback, T, Arg>
	inline auto GeneratorInvoke(Callback&& callback, T&& t, const std::set<Arg>& arg)
		-> Generator<decltype(std::declval<Callback>()(std::declval<T>(), std::declval<Arg>()))>
	{
		for (const auto& i : arg)
		{
			co_yield std::forward<Callback>(callback)(std::forward<T>(t), i);
		}
	}
	/// <summary>
	/// generator invoke unordered_set with other argument
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="T">other argument type</typeparam>
	/// <typeparam name="Arg">unordered_set container value type</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="t">other argument</param>
	/// <param name="arg">unordered_set</param>
	/// <returns>returns Generator</returns>
	template<typename Callback, typename T, typename Arg>
		requires std::invocable<Callback, T, Arg>
	inline auto GeneratorInvoke(Callback&& callback, T&& t, const std::unordered_set<Arg>& arg)
		-> Generator<decltype(std::declval<Callback>()(std::declval<T>(), std::declval<Arg>()))>
	{
		for (const auto& i : arg)
		{
			co_yield std::forward<Callback>(callback)(std::forward<T>(t), i);
		}
	}
#else
	// not coroutine, use std::list as returns for avioding std::vector<bool>
	template<typename Callback, typename Arg>
		requires std::invocable<Callback, Arg>
	inline auto GeneratorInvoke(Callback&& callback, const std::list<Arg>& arg)
		-> std::list<std::invoke_result_t<Callback, Arg>>
	{
		using R = std::invoke_result_t<Callback, Arg>;
		std::list<R> ret;
		for (const auto& i : arg)
		{
			ret.emplace_back(std::forward<Callback>(callback)(i));
		}
		return ret;
	}
	template<typename Callback, typename Arg>
		requires std::invocable<Callback, Arg>&& std::negation_v<std::is_same<Arg, bool>>
	inline auto GeneratorInvoke(Callback&& callback, const std::vector<Arg>& arg)
		-> std::list<std::invoke_result_t<Callback, Arg>>
	{
		using R = std::invoke_result_t<Callback, Arg>;
		std::list<R> ret;
		for (const auto& i : arg)
		{
			ret.emplace_back(std::forward<Callback>(callback)(i));
		}
		return ret;
	}
	template<typename Callback, typename Arg>
		requires std::invocable<Callback, Arg>
	inline auto GeneratorInvoke(Callback&& callback, const std::set<Arg>& arg)
		-> std::list<std::invoke_result_t<Callback, Arg>>
	{
		using R = std::invoke_result_t<Callback, Arg>;
		std::list<R> ret;
		for (const auto& i : arg)
		{
			ret.emplace_back(std::forward<Callback>(callback)(i));
		}
		return ret;
	}
	template<typename Callback, typename Arg>
		requires std::invocable<Callback, Arg>
	inline auto GeneratorInvoke(Callback&& callback, const std::unordered_set<Arg>& arg)
		-> std::list<std::invoke_result_t<Callback, Arg>>
	{
		using R = std::invoke_result_t<Callback, Arg>;
		std::list<R> ret;
		for (const auto& i : arg)
		{
			ret.emplace_back(std::forward<Callback>(callback)(i));
		}
		return ret;
	}
	template<typename Callback, typename T, typename Arg>
		requires std::invocable<Callback, T, Arg>
	inline auto GeneratorInvoke(Callback&& callback, T&& t, const std::list<Arg>& arg)
		-> std::list<decltype(std::declval<Callback>()(std::declval<T>(), std::declval<Arg>()))>
	{
		using R = decltype(std::declval<Callback>()(std::declval<T>(), std::declval<Arg>()));
		std::list<R> ret;
		for (const auto& i : arg)
		{
			ret.emplace_back(std::forward<Callback>(callback)(std::forward<T>(t), i));
		}
		return ret;
	}
	template<typename Callback, typename T, typename Arg>
		requires std::invocable<Callback, T, Arg>&& std::negation_v<std::is_same<Arg, bool>>
	inline auto GeneratorInvoke(Callback&& callback, T&& t, const std::vector<Arg>& arg)
		-> std::list<decltype(std::declval<Callback>()(std::declval<T>(), std::declval<Arg>()))>
	{
		using R = decltype(std::declval<Callback>()(std::forward<T>(t), std::declval<Arg>()));
		std::list<R> ret;
		for (const auto& i : arg)
		{
			ret.emplace_back(std::forward<Callback>(callback)(std::forward<T>(t), i));
		}
		return ret;
	}
	template<typename Callback, typename T, typename Arg>
		requires std::invocable<Callback, T, Arg>
	inline auto GeneratorInvoke(Callback&& callback, T&& t, const std::set<Arg>& arg)
		-> std::list<decltype(std::declval<Callback>()(std::declval<T>(), std::declval<Arg>()))>
	{
		using R = decltype(std::declval<Callback>()(std::forward<T>(t), std::declval<Arg>()));
		std::list<R> ret;
		for (const auto& i : arg)
		{
			ret.emplace_back(std::forward<Callback>(callback)(std::forward<T>(t), i));
		}
		return ret;
	}
	template<typename Callback, typename T, typename Arg>
		requires std::invocable<Callback, T, Arg>
	inline auto GeneratorInvoke(Callback&& callback, T&& t, const std::unordered_set<Arg>& arg)
		-> std::list<decltype(std::declval<Callback>()(std::declval<T>(), std::declval<Arg>()))>
	{
		using R = decltype(std::declval<Callback>()(std::forward<T>(t), std::declval<Arg>()));
		std::list<R> ret;
		for (const auto& i : arg)
		{
			ret.emplace_back(std::forward<Callback>(callback)(std::forward<T>(t), i));
		}
		return ret;
	}
#endif // __cpp_lib_coroutine && __cpp_impl_coroutine


}// namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_TEMPLATE_HELPHER_HPP
