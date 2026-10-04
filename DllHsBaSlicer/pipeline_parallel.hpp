/** @file pipeline_parallel.hpp
 * @brief Header-only layer-parallel execution helper for the slicing and fill pipeline hot paths.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_PIPELINE_PARALLEL_HPP
#define HSBA_SLICER_PIPELINE_PARALLEL_HPP

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <future>
#include <thread>
#include <vector>

#include "base/thread_pool.hpp"

namespace HsBa::Slicer::Pipeline
{

/**
 * @brief Run per-layer work in parallel across a thread pool (desktop release hot path).
 *
 * Slicing and fill stages iterate layers that are mutually independent, so they
 * parallelize well. This helper distributes layer indices over a ThreadPool and
 * keeps the progress callback strictly on the calling thread: work() may run
 * concurrently but MUST only write to output slots indexed by its own layer,
 * while on_progress(done_layers) is invoked serially after each completed block.
 *
 * Falls back to a plain serial loop when only one thread (or one layer) is
 * available, preserving behavior on constrained platforms.
 *
 * Exceptions thrown inside work() are rethrown to the caller via the futures.
 *
 * @param total_layers Number of layers to process.
 * @param work         Thread-safe per-layer functor invoked as work(layer_index).
 * @param on_progress  Optional functor invoked as on_progress(completed_layers)
 *                     on the calling thread after each parallel block finishes.
 */
inline void ParallelForLayers(int total_layers, const std::function<void(int)>& work,
                              const std::function<void(int)>& on_progress = nullptr)
{
    if (total_layers <= 0)
    {
        return;
    }

    // Runtime override for the worker count. HSBA_PIPELINE_THREADS=1 forces the
    // serial path, enabling an apples-to-apples serial-vs-parallel timing and
    // result comparison on the same binary (used for pipeline perf validation).
    // Unset or invalid values fall back to hardware concurrency.
    size_t requested = 0;
    if (const char* env = std::getenv("HSBA_PIPELINE_THREADS"))
    {
        long v = std::strtol(env, nullptr, 10);
        if (v > 0)
        {
            requested = static_cast<size_t>(v);
        }
    }

    const unsigned hw = std::thread::hardware_concurrency();
    size_t nthreads = (hw == 0) ? 2u : hw;
    if (requested > 0)
    {
        nthreads = std::min(requested, static_cast<size_t>(total_layers));
    }
    else
    {
        nthreads = std::min(nthreads, static_cast<size_t>(total_layers));
    }

    // Serial path: single core or single layer keeps behavior identical.
    if (nthreads <= 1 || total_layers <= 1)
    {
        for (int i = 0; i < total_layers; ++i)
        {
            work(i);
            if (on_progress)
            {
                on_progress(i + 1);
            }
        }
        return;
    }

    ThreadPool pool(nthreads);

    // Process in blocks so the progress callback fires a handful of times on the
    // calling thread rather than once per layer from worker threads.
    int blocks = static_cast<int>(std::min<std::size_t>(nthreads * 4, static_cast<std::size_t>(total_layers)));
    if (blocks < 1)
    {
        blocks = 1;
    }
    const int block_size = (total_layers + blocks - 1) / blocks;

    for (int start = 0; start < total_layers; start += block_size)
    {
        const int end = std::min(total_layers, start + block_size);
        std::vector<std::future<void>> futures;
        futures.reserve(end - start);
        for (int i = start; i < end; ++i)
        {
            futures.push_back(pool.submit([&work, i] { work(i); }));
        }
        for (auto& f : futures)
        {
            f.get();  // block on this block; rethrows any worker exception
        }
        if (on_progress)
        {
            on_progress(end);
        }
    }
}

}  // namespace HsBa::Slicer::Pipeline

#endif  // !HSBA_SLICER_PIPELINE_PARALLEL_HPP
