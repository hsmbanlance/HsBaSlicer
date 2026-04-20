#define BOOST_TEST_MODULE thread_pool_test
#include <boost/test/included/unit_test.hpp>

#include <atomic>

#include "base/thread_pool.hpp"
#include "base/coroutine.hpp"

using namespace HsBa::Slicer;
using namespace HsBa::Slicer::Utils;

BOOST_AUTO_TEST_SUITE(thread_pool)

BOOST_AUTO_TEST_CASE(thread_pool_normal)
{
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    auto future1 = pool.submit([&counter]() {
        counter.fetch_add(1, std::memory_order_relaxed);
        return 10;
    });
    auto future2 = pool.submit([&counter]() {
        counter.fetch_add(2, std::memory_order_relaxed);
        return 20;
    });

    BOOST_CHECK_EQUAL(future1.get(), 10);
    BOOST_CHECK_EQUAL(future2.get(), 20);

    pool.WaitAll();
    BOOST_CHECK_EQUAL(counter.load(std::memory_order_relaxed), 3);
}

#ifdef HSBA_ENABLE_THREAD_POOL_COROUTINE
BOOST_AUTO_TEST_CASE(thread_pool_coroutine)
{
    auto task = []() -> Task<int, ThreadPoolExecutor> {
        co_await []() -> Task<void, ThreadPoolExecutor> {
            co_return;
        }();
        co_return 123;
    }();

    BOOST_CHECK_EQUAL(task.get_result(), 123);
}

BOOST_AUTO_TEST_CASE(thread_pool_mixed)
{
    ThreadPool pool(2);
    ThreadPoolExecutor::SetDefaultPool(&pool);
    std::atomic<int> counter{0};

    auto future = pool.submit([&counter]() {
        counter.fetch_add(5, std::memory_order_relaxed);
    });

    auto task = []() -> Task<int, ThreadPoolExecutor> {
        co_await []() -> Task<void, ThreadPoolExecutor> {
            co_return;
        }();
        co_return 42;
    }();

    BOOST_CHECK_EQUAL(task.get_result(), 42);
    future.get();
    pool.WaitAll();
    BOOST_CHECK_EQUAL(counter.load(std::memory_order_relaxed), 5);

    ThreadPoolExecutor::SetDefaultPool(nullptr);
}
#endif

BOOST_AUTO_TEST_SUITE_END()