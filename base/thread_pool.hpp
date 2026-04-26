#pragma once
#ifndef HSBA_SLICER_THREAD_POOL_HPP
#define HSBA_SLICER_THREAD_POOL_HPP

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

#include "error.hpp"
#include "coroutine.hpp"

namespace HsBa::Slicer
{
    class ThreadPool
    {
    public:
        explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) 
            : stop_(false), active_tasks_(0)
        {
            if(num_threads == 0)
                throw InvalidArgumentError("Number of threads must be greater than 0");
            workers_.reserve(num_threads);
            for (size_t i = 0; i < num_threads; ++i) 
            {
                workers_.emplace_back([this] { WorkerLoop(); });
            }
        }
        ~ThreadPool() 
        {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                stop_ = true;
            }
            condition_.notify_all();
            for (auto& worker : workers_) 
            {
                if (worker.joinable()) 
                {
                    worker.join();
                }
            }
        }
        template<typename F, typename... Args>
        auto submit(F&& f, Args&&... args) 
            -> std::future<std::invoke_result_t<F, Args...>> 
        {
            using ReturnType = std::invoke_result_t<F, Args...>;
        
            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );
        
            std::future<ReturnType> result = task->get_future();
        
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (stop_) 
                {
                    throw RuntimeError("Cannot submit task to stopped ThreadPool");
                }
            
                tasks_.emplace([task]() { (*task)(); });
                ++active_tasks_;
            }
        
            condition_.notify_one();
            return result;
        }

        size_t PendingTasks() const 
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            return tasks_.size();
        }


        size_t ActiveTasks() const 
        {
            return active_tasks_.load();
        }

        void WaitAll() 
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            finished_condition_.wait(lock, [this] {
                return tasks_.empty() && active_tasks_ == 0;
            });
        }
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;
    private:
        void WorkerLoop()
        {
            while (true) 
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) 
                    {
                        return;
                    }
                    if (!tasks_.empty())
                    {
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    else
                    {
                        continue;  // Spurious wakeup, go back to waiting
                    }
                }
                try 
                {
                    task();
                }
                catch (const std::exception& e) 
                {
                    // Handle exception in task but still decrement active_tasks
                }
                catch (...) 
                {
                    // Handle unknown exception but still decrement active_tasks
                }
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    --active_tasks_;
                }
                finished_condition_.notify_all();
            }
        }
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        mutable std::mutex queue_mutex_;
        std::condition_variable condition_;
        std::condition_variable finished_condition_;
        std::atomic<bool> stop_{false};
        std::atomic<size_t> active_tasks_{0};
    };

#if __cpp_lib_coroutine && __cpp_impl_coroutine
#ifdef HSBA_ENABLE_THREAD_POOL_COROUTINE
namespace Utils
{
    class ThreadPoolExecutor : public IExecutor
    {
    public:
        ThreadPoolExecutor() noexcept : pool_(default_pool_) {}
        explicit ThreadPoolExecutor(ThreadPool& pool) noexcept : pool_(&pool) {}

        inline void execute(std::function<void()>&& func) override
        {
            auto* pool = get_pool();
            pool->submit([f = std::move(func)]() mutable { f(); });
        }

        inline static void SetDefaultPool(ThreadPool* pool) noexcept
        {
            default_pool_ = pool;
        }

    private:
        static inline ThreadPool* default_pool_ = nullptr;
        ThreadPool* pool_{nullptr};

        inline ThreadPool* get_pool() const noexcept
        {
            if (pool_)
            {
                return pool_;
            }
            if (default_pool_)
            {
                return default_pool_;
            }
            static ThreadPool default_pool_instance(std::max<size_t>(1u, std::thread::hardware_concurrency()));
            return &default_pool_instance;
        }
    };
}
#endif // HSBA_ENABLE_THREAD_POOL_COROUTINE
#endif // __cpp_lib_coroutine && __cpp_impl_coroutine

} // namespace HsBa::Slicer

#endif // !HSBA_SLICER_THREAD_POOL_HPP