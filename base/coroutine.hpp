#pragma once
#ifndef HSBA_SLICER_COROUTINE_HPP
#define HSBA_SLICER_COROUTINE_HPP

#if __cpp_lib_coroutine && __cpp_impl_coroutine
#include <coroutine>
#endif
#ifdef __cpp_lib_generator
#include <generator>
#endif // __cpp_lib_generator

#include "template_helper.hpp"
#if __cpp_lib_coroutine && __cpp_impl_coroutine
#include <memory>
#include <functional>
#include <thread>
#include <future>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <list>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <utility>
#endif

namespace HsBa::Slicer::Utils
{
    #if __cpp_lib_coroutine && __cpp_impl_coroutine
	/**
	 * @brief coroutine executor interface
	 */
	class IExecutor
	{
	public:
		IExecutor() = default;
		virtual ~IExecutor() = default;
		virtual void execute(std::function<void()>&& func) = 0;
	};
	/**
	 * @brief coroutine executor noop, do nothing
	 */
	class NoopExecutor : public IExecutor
	{
	public:
		inline void execute(std::function<void()>&& func) override
		{
			func();
		}
	};

	/**
	 * @brief coroutine executor async
	 */
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
	/**
	 * @brief 	coroutine taskawaiter
	 * @tparam T coroutine return type
	 * @tparam Executor coroutine executor
	 */
template<typename T, typename Executor>
		requires (std::movable<T> || std::is_void_v<T>) && std::derived_from<Executor, IExecutor>
class TaskAwaiter
	{
		friend class Task<T, Executor>;
	public:
        inline explicit TaskAwaiter(Executor executor, Task<T, Executor>&& task) noexcept
			:executor_(std::move(executor)), task_(std::move(task)) {}
		TaskAwaiter(TaskAwaiter&& o) noexcept : executor_(std::move(o.executor_)), task_(std::move(o.task_)) {}
		inline TaskAwaiter& operator=(TaskAwaiter&& o) noexcept
		{
			executor_ = std::move(o.executor_);
			task_ = std::move(o.task_);
			return *this;
		}
		inline constexpr bool await_ready() const noexcept { return task_.handle_ && task_.handle_.done(); }
        inline void await_suspend(std::coroutine_handle<> handle) noexcept
		{
			if (!await_ready())
			{
				auto exec = executor_;
				task_.finally([handle, exec]() mutable { exec.execute([handle] { handle.resume(); }); });
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
        Executor executor_;
	};
	template<typename T, typename Executor = AsyncExecutor, typename Allocator = std::allocator<T>>
		requires std::default_initializable<T>&& std::movable<T>&& std::derived_from<Executor, IExecutor>&& TAllocator<T, Allocator>&& NoStateAllocator<Allocator>
	class CustomAllocatorTask;
	
	/**
	 * @brief coroutine task awaiter with custom allocator
	 * @tparam T coroutine return type
	 * @tparam Executor coroutine executor
	 * @tparam Allocator coroutine allocator
	 */
template<typename T, typename Executor, typename Allocator>
		requires std::movable<T>&& std::derived_from<Executor, IExecutor>&& TAllocator<T, Allocator> && NoStateAllocator<Allocator> && NoStateAllocator<Allocator>
class CustomAllocatorTaskAwaiter
	{
		friend class CustomAllocatorTask<T, Executor, Allocator>;
	public:
        inline explicit CustomAllocatorTaskAwaiter(Executor executor, CustomAllocatorTask<T, Executor, Allocator>&& task) noexcept
			:executor_(std::move(executor)), task_(std::move(task)) {}
		CustomAllocatorTaskAwaiter(CustomAllocatorTaskAwaiter&& o) noexcept : executor_(std::move(o.executor_)), task_(std::move(o.task_)) {}
		inline CustomAllocatorTaskAwaiter& operator=(CustomAllocatorTaskAwaiter&& o) noexcept
		{
			executor_ = std::move(o.executor_);
			task_ = std::move(o.task_);
			return *this;
		}
		inline constexpr bool await_ready() const noexcept { return task_.handle_ && task_.handle_.done(); }
        inline void await_suspend(std::coroutine_handle<> handle) noexcept
		{
			if (!await_ready())
			{
				auto exec = executor_;
				task_.finally([handle, exec]() mutable { exec.execute([handle] { handle.resume(); }); });
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
		Executor executor_;
	};
	
	/**
	 * @brief coroutine dispatchawaiter
	 */
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
	 
	/**
	 * @brief coroutine task
	 * @tparam T task return type
	 * @tparam Executor coroutine task executor
	 */
	template<typename T, typename Executor>
		requires ((std::movable<T>&& std::default_initializable<T>) || std::is_void_v<T>)
	&& std::derived_from<Executor, IExecutor>
		class Task
	{
		friend class TaskAwaiter<T, Executor>;
	public:
		/**
		 * @brief coroutines task result
		 */
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
                return TaskAwaiter<R, Exe>(std::move(executor_), std::move(task));
			}
			template<typename R, typename Exe, typename Allo>
				requires std::default_initializable<R>&& std::movable<R>&& std::derived_from<Exe, IExecutor>&& TAllocator<R, Allo>
			inline CustomAllocatorTaskAwaiter<R, Exe, Allo> await_transform(CustomAllocatorTask<R, Exe, Allo>&& task)
			{
                return CustomAllocatorTaskAwaiter<R, Exe, Allo>(std::move(executor_), std::move(task));
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
		inline Task& operator=(Task&& task) noexcept
		{
			handle_ = std::exchange(task.handle_, {});
			return *this;
		}

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

	/**
	 * @brief coroutine task with return void
	 * @tparam Executor coroutine executor
	 */
	template<typename Executor>
	class Task<void, Executor>
	{
		friend class TaskAwaiter<void, Executor>;
	public:
		/**
		 * @brief task result
		 */
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
                return TaskAwaiter<R, Exe>(executor_, std::move(task));
			}
			template<typename R, typename Exe, typename Allo>
				requires std::default_initializable<R>&& std::movable<R>&& std::derived_from<Exe, IExecutor>&& TAllocator<R, Allo>
			inline CustomAllocatorTaskAwaiter<R, Exe, Allo> await_transform(CustomAllocatorTask<R, Exe, Allo>&& task)
			{
                return CustomAllocatorTaskAwaiter<R, Exe, Allo>(executor_, std::move(task));
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
		inline Task& operator=(Task&& task) noexcept
		{
			handle_ = std::exchange(task.handle_, {});
			return *this;
		}

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
	
	/**
	 * @brief coroutines task with custom allocator
	 * @tparam T task result type
	 * @tparam Executor executor type
	 * @tparam Allocator custom allocator type
	 */
	template<typename T, typename Executor, typename Allocator>
		requires std::default_initializable<T>&& std::movable<T>&& std::derived_from<Executor, IExecutor>&& TAllocator<T, Allocator>&& NoStateAllocator<Allocator>
	class CustomAllocatorTask
	{
		friend class CustomAllocatorTaskAwaiter<T, Executor, Allocator>;
	public:
		using allocator_type = Allocator;
		/**
		 * @brief coroutines task result
		 */
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
			inline CustomAllocatorTask<T, Executor, Allocator> get_return_object()
			{
				return CustomAllocatorTask<T, Executor, Allocator>{ std::coroutine_handle<promise_type>::from_promise(*this) };
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
                return TaskAwaiter<R, Exe>(executor_, std::move(task));
			}
			template<typename R, typename Exe, typename Allo>
				requires std::default_initializable<R>&& std::movable<R>&& std::derived_from<Exe, IExecutor>&& TAllocator<R, Allo>
			inline CustomAllocatorTaskAwaiter<R, Exe, Allo> await_transform(CustomAllocatorTask<R, Exe, Allo>&& task)
			{
                return CustomAllocatorTaskAwaiter<R, Exe, Allo>(executor_, std::move(task));
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

            // Use custom allocator for promise allocation
			void* operator new(size_t size)
			{
				using AllocTraits = std::allocator_traits<Allocator>;
				using PromiseAlloc = typename AllocTraits::template rebind_alloc<promise_type>;
				PromiseAlloc promise_alloc(alloc);
				size_t count = (size + sizeof(promise_type) - 1) / sizeof(promise_type);
				return promise_alloc.allocate(count);
			}

			void operator delete(void* p) noexcept
			{
				using AllocTraits = std::allocator_traits<Allocator>;
				using PromiseAlloc = typename AllocTraits::template rebind_alloc<promise_type>;
				PromiseAlloc promise_alloc(alloc);
				promise_alloc.deallocate(static_cast<promise_type*>(p), 1);
			}

			void operator delete(void* p, size_t size) noexcept
			{
				using AllocTraits = std::allocator_traits<Allocator>;
				using PromiseAlloc = typename AllocTraits::template rebind_alloc<promise_type>;
				PromiseAlloc promise_alloc(alloc);
				size_t count = (size + sizeof(promise_type) - 1) / sizeof(promise_type);
				promise_alloc.deallocate(static_cast<promise_type*>(p), count);
			}
			inline static Allocator alloc{};
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
		inline CustomAllocatorTask& operator=(CustomAllocatorTask&& task) noexcept
		{
			handle_ = std::exchange(task.handle_, {});
			return *this;
		}

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
	
	/**
	 * @brief personal coroutine Generator
	 * @tparam T yield type
	 */
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
				return TaskAwaiter<R, Exe>(std::move(exe), std::move(task));
			}
            template<typename R, typename Exe, typename Allo>
				requires std::default_initializable<R>&& std::movable<R>&& std::derived_from<Exe, IExecutor>&& TAllocator<R, Allo>
			inline CustomAllocatorTaskAwaiter<R, Exe, Allo> await_transform(CustomAllocatorTask<R, Exe, Allo>&& task)
			{
				Exe exe{};
				return CustomAllocatorTaskAwaiter<R, Exe, Allo>(std::move(exe), std::move(task));
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
		/**
		 * @brief generator iterator
		 */
		class iterator
		{
		public:
			iterator& operator++()
			{
				if (coroutine_ && !coroutine_.done()) coroutine_.resume();
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
	
	/**
	 * @brief personal coroutine Generator with custom allocator, allocator is static member in promise_type
	 * promise_type new and delete use allocator, so allocator must be static
	 * @tparam Allocator allocator type, default is std::allocator
	 * @tparam T yield type
	 */
	template<std::movable T, typename Allocator = std::allocator<T>>
		requires TAllocator<T, Allocator>&& NoStateAllocator<Allocator>
	class CustomAllocatorGenerator
	{
	public:
		using allocator_type = Allocator;
		struct promise_type
		{
			using return_type = CustomAllocatorGenerator<T, Allocator>;
			inline return_type get_return_object()
			{
				return CustomAllocatorGenerator<T,Allocator>{ Handle::from_promise(*this) };
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
				return TaskAwaiter<R, Exe>(std::move(exe), std::move(task));
			}
			template<typename R, typename Exe, typename Allo>
			inline CustomAllocatorTaskAwaiter<R, Exe, Allo> await_transform(CustomAllocatorTask<R, Exe, Allo>&& task)
			{
				Exe exe{};
				return CustomAllocatorTaskAwaiter<R, Exe, Allo>(std::move(exe), std::move(task));
			}
            [[noreturn]]
			inline static void unhandled_exception()
			{
				onCancel();
				throw std::runtime_error("unhandled exception in coroutine");
			}

            std::optional<T> current_value;
            inline static Allocator alloc{};
            // Use custom allocator for promise allocation
			void* operator new(size_t size)
			{
				using AllocTraits = std::allocator_traits<Allocator>;
				using PromiseAlloc = typename AllocTraits::template rebind_alloc<promise_type>;
				PromiseAlloc promise_alloc(alloc);
				size_t count = (size + sizeof(promise_type) - 1) / sizeof(promise_type);
				return promise_alloc.allocate(count);
			}

			void operator delete(void* p) noexcept
			{
				using AllocTraits = std::allocator_traits<Allocator>;
				using PromiseAlloc = typename AllocTraits::template rebind_alloc<promise_type>;
				PromiseAlloc promise_alloc(alloc);
				promise_alloc.deallocate(static_cast<promise_type*>(p), 1);
			}

			void operator delete(void* p, size_t size) noexcept
			{
				using AllocTraits = std::allocator_traits<Allocator>;
				using PromiseAlloc = typename AllocTraits::template rebind_alloc<promise_type>;
				PromiseAlloc promise_alloc(alloc);
				size_t count = (size + sizeof(promise_type) - 1) / sizeof(promise_type);
				promise_alloc.deallocate(static_cast<promise_type*>(p), count);
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
		/**
		 * @brief generator iterator
		 */
		class iterator
		{
		public:
			iterator& operator++()
			{
				if (coroutine_ && !coroutine_.done()) coroutine_.resume();
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

	template<typename Arg, template<typename> typename Container,typename Callback>
		requires std::invocable<Callback, Arg> && (std::ranges::range<Container<Arg>> && !std::same_as<Container<Arg>,std::vector<bool>>)
	inline auto GeneratorInvoke(Callback&& callback, const Container<Arg>& arg)
		-> Generator<std::invoke_result_t<Callback, Arg>>
	{
		for (const auto& i : arg)
		{
			co_yield std::forward<Callback>(callback)(i);
		}
	}
		
#else
	// not coroutine, use std::list as returns for avioding std::vector<bool>

	template<typename Arg, template<typename> typename Container,typename Callback>
		requires std::invocable<Callback, Arg> && (std::ranges::range<Container<Arg>> && !std::same_as<Container<Arg>, std::vector<bool>>)
	inline auto GeneratorInvoke(Callback&& callback, const Container<Arg>& arg)
		-> Container<std::invoke_result_t<Callback, Arg>>
	{
      using R = std::invoke_result_t<Callback, Arg>;
		Container<R> ret;
		for (const auto& i : arg)
		{
			ret.emplace_back(std::forward<Callback>(callback)(i));
		}
		return ret;
	}
#endif // __cpp_lib_coroutine && __cpp_impl_coroutine

} // namespace HsBa::Slicer::Utils

#endif // HSBA_SLICER_COROUTINE_HPP