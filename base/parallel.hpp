/** @file parallel.hpp
 * @brief Header-only layer-parallel execution helpers shared across the pipeline stack.
 *
 * Lives in the base layer so that lower libraries (support, paths, LibHsBaSlicer) and
 * the Dll pipeline orchestrators all use the same process-wide ThreadPool. Kept in
 * namespace HsBa::Slicer::Pipeline for source compatibility with DllHsBaSlicer.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_BASE_PARALLEL_HPP
#define HSBA_SLICER_BASE_PARALLEL_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <future>
#include <thread>
#include <utility>
#include <vector>

#include "base/coroutine.hpp"
#include "base/thread_pool.hpp"

namespace HsBa::Slicer::Pipeline
{

/**
 * @brief Resolve the worker count for the process-wide pipeline thread pool.
 *
 * HSBA_PIPELINE_THREADS overrides hardware concurrency; <=0 or unset falls back
 * to std::thread::hardware_concurrency(). Read once on first use; callers must
 * set the env before the pipeline runs to change parallelism.
 */
inline size_t ResolvePipelineThreads()
{
    size_t requested = 0;
    if (const char* env = std::getenv("HSBA_PIPELINE_THREADS"))
    {
        long v = std::strtol(env, nullptr, 10);
        if (v > 0)
        {
            requested = static_cast<size_t>(v);
        }
    }
    if (requested > 0)
    {
        return requested;
    }
    const unsigned hw = std::thread::hardware_concurrency();
    return (hw == 0) ? 2u : static_cast<size_t>(hw);
}

/**
 * @brief Process-wide ThreadPool reused by every ParallelFor / ParallelMap call.
 *
 * Constructed lazily on first use and lives until process exit. Callers must not
 * submit nested parallel work from within a worker callback (that would occupy
 * pool threads waiting on sibling threads). Pipelines keep all parallel calls at
 * the top level, so this is safe.
 */
inline ThreadPool& PipelinePool()
{
    static ThreadPool pool(ResolvePipelineThreads());
    return pool;
}

/// @brief Cached worker count (matches PipelinePool()'s construction size).
inline size_t PipelineThreadCount()
{
    static const size_t n = ResolvePipelineThreads();
    return n;
}

namespace detail
{

/**
 * @brief Execute work(0..total-1) with atomic cursor scheduling; when on_progress
 *        is provided it is invoked strictly on the calling thread.
 *
 * Serial path (nthreads<=1) preserves exact order. Parallel path submits
 * `nthreads` worker lambdas that each fetch_add a shared cursor until it
 * exceeds total; the calling thread polls a done-counter (fires every 32
 * completed layers or when total is reached) while waiting for the futures.
 * Exceptions from workers are rethrown via futures.get().
 */
template <typename Work>
inline void RunAtomicIndexed(int total, const Work& work, const std::function<void(int)>& on_progress)
{
    if (total <= 0)
    {
        return;
    }

    const size_t hw_threads = PipelineThreadCount();
    const size_t nthreads = std::min(hw_threads, static_cast<size_t>(total));

    if (nthreads <= 1)
    {
        for (int i = 0; i < total; ++i)
        {
            work(i);
            if (on_progress)
            {
                on_progress(i + 1);
            }
        }
        return;
    }

    std::atomic<int> cursor{0};
    std::atomic<int> done{0};

    auto& pool = PipelinePool();
    std::vector<std::future<void>> futures;
    futures.reserve(nthreads);
    for (size_t t = 0; t < nthreads; ++t)
    {
        futures.push_back(pool.submit(
            [&cursor, &done, total, &work]()
            {
                while (true)
                {
                    const int i = cursor.fetch_add(1, std::memory_order_relaxed);
                    if (i >= total)
                    {
                        break;
                    }
                    work(i);
                    done.fetch_add(1, std::memory_order_relaxed);
                }
            }));
    }

    if (on_progress)
    {
        int last_reported = 0;
        while (true)
        {
            const int d = done.load(std::memory_order_relaxed);
            if (d - last_reported >= 32 || (d == total && d != last_reported))
            {
                on_progress(d);
                last_reported = d;
            }
            bool all_ready = true;
            for (auto& f : futures)
            {
                if (f.wait_for(std::chrono::milliseconds(2)) != std::future_status::ready)
                {
                    all_ready = false;
                    break;
                }
            }
            if (all_ready)
            {
                break;
            }
        }
        const int d_final = done.load(std::memory_order_relaxed);
        if (d_final != last_reported)
        {
            on_progress(d_final);
        }
    }

    for (auto& f : futures)
    {
        f.get();  // rethrow any worker exception on the caller thread
    }
}

}  // namespace detail

/**
 * @brief Run per-layer work in parallel across the shared pipeline ThreadPool.
 *
 * Slicing/fill/support layers are mutually independent, so they parallelize
 * well. work() may run concurrently but MUST only write to output slots
 * indexed by its own layer. on_progress(done_layers) fires only on the
 * calling thread. HSBA_PIPELINE_THREADS=1 forces a serial loop, preserving
 * original ordering semantics for regression comparison on the same binary.
 *
 * @param total_layers Number of layers to process.
 * @param work         Thread-safe per-layer functor invoked as work(layer_index).
 * @param on_progress  Optional functor invoked on the calling thread with the
 *                     count of completed layers as they progress.
 */
inline void ParallelForLayers(int total_layers, const std::function<void(int)>& work,
                              const std::function<void(int)>& on_progress = nullptr)
{
    if (total_layers <= 0 || !work)
    {
        return;
    }
    detail::RunAtomicIndexed(total_layers, [&work](int i) { work(i); }, on_progress);
}

/**
 * @brief Produce one R per layer in parallel; results are written into a
 *        pre-sized vector by index so the returned order matches input order.
 *
 * R must be MoveAssignable. Serial path (HSBA_PIPELINE_THREADS=1 or total<=1)
 * iterates 0..total-1 for bit-exact parity with the original for-loop.
 */
template <typename R>
inline std::vector<R> ParallelMapLayers(int total_layers, const std::function<R(int)>& map)
{
    std::vector<R> out(static_cast<size_t>(std::max(total_layers, 0)));
    if (total_layers <= 0 || !map)
    {
        return out;
    }
    detail::RunAtomicIndexed(total_layers, [&out, &map](int i) { out[static_cast<size_t>(i)] = map(i); }, nullptr);
    return out;
}

/**
 * @brief Coroutine-mode wrapper: returns a Utils::Task<void> that dispatches the
 *        layer-parallel loop on the shared PipelinePool.
 *
 * Pipelines written as `Utils::Task<T>` (default AsyncExecutor) can `co_await`
 * this helper at stage boundaries. Semantics match ParallelForLayers exactly:
 * workers write only into their indexed output slots, on_progress fires only
 * on the coroutine's dispatch thread after all workers complete. HSBA_PIPELINE_THREADS=1
 * still runs the loop serially on the coroutine's dispatched thread.
 *
 * Because AsyncExecutor::execute blocks on the std::async future's destructor,
 * calling this helper's returned Task synchronously via .get_result() from a
 * non-coroutine context is equivalent to invoking ParallelForLayers directly.
 */
inline Utils::Task<void> ParallelForLayersAsync(int total_layers, std::function<void(int)> work,
                                                std::function<void(int)> on_progress = nullptr)
{
    if (total_layers > 0 && work)
    {
        detail::RunAtomicIndexed(total_layers, [&work](int i) { work(i); }, on_progress);
    }
    co_return;
}

/// @brief Coroutine-mode wrapper for ParallelMapLayers; co_await yields the
///        ordered result vector once every layer's map() has completed.
template <typename R>
inline Utils::Task<std::vector<R>> ParallelMapLayersAsync(int total_layers, std::function<R(int)> map)
{
    std::vector<R> out(static_cast<size_t>(std::max(total_layers, 0)));
    if (total_layers > 0 && map)
    {
        detail::RunAtomicIndexed(total_layers, [&out, &map](int i) { out[static_cast<size_t>(i)] = map(i); }, nullptr);
    }
    co_return out;
}

}  // namespace HsBa::Slicer::Pipeline

#endif  // !HSBA_SLICER_BASE_PARALLEL_HPP
