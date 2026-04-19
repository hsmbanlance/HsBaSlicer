#define BOOST_TEST_MODULE memory_pool_test
#include <boost/test/included/unit_test.hpp>

#include <vector>
#include <list>
#include <string>
#include <memory>

#include "base/memory_pool.hpp"
#include "base/coroutine.hpp"

// Global type alias for pool
using TestPool = HsBa::Slicer::StaticMemoryPool<int, 1024 * sizeof(int)>;

// Global coroutine functions for testing
HsBa::Slicer::Utils::CustomAllocatorTask<int, HsBa::Slicer::Utils::AsyncExecutor, TestPool> simple_task() {
	co_return 42;
}

HsBa::Slicer::Utils::CustomAllocatorGenerator<int, TestPool> simple_generator() {
	co_yield 1;
	co_yield 2;
	co_yield 3;
}

BOOST_AUTO_TEST_SUITE(memory_pool)

BOOST_AUTO_TEST_CASE(memory_pool_in_std_contains)
{
	HsBa::Slicer::MemoryPool<int, 1024 * sizeof(int)> pool;
	std::vector<int, HsBa::Slicer::MemoryPool<int, 1024 * sizeof(int)>> vec(pool);
	std::list<int, HsBa::Slicer::MemoryPool<int, 1024 * sizeof(int)>> list(pool);
	vec.emplace_back(1);
	list.emplace_back(1);
	BOOST_CHECK_EQUAL(vec[0], 1);
	BOOST_CHECK_EQUAL(list.front(), 1);
}

BOOST_AUTO_TEST_CASE(memory_pool_in_std_smart_ptr)
{
	HsBa::Slicer::MemoryPool<std::string, 1024 * sizeof(std::string)> pool;
	std::vector<std::string, HsBa::Slicer::MemoryPool<std::string, 1024 * sizeof(std::string)>> vec(pool);
	vec.emplace_back("Hello");
	vec.emplace_back("World");
	std::shared_ptr<std::string> sp1 = std::allocate_shared<std::string>(pool, "Shared Hello");
	std::shared_ptr<std::string> sp2 = std::allocate_shared<std::string>(pool, "Shared World");
	BOOST_CHECK_EQUAL(*sp1, "Shared Hello");
	BOOST_CHECK_EQUAL(*sp2, "Shared World");
}

BOOST_AUTO_TEST_CASE(static_memory_pool_std_contain)
{
	std::vector<int, HsBa::Slicer::StaticMemoryPool<int, 1024 * sizeof(int)>> vec;
	std::list<int, HsBa::Slicer::StaticMemoryPool<int, 1024 * sizeof(int)>> list;
	vec.emplace_back(1);
	list.emplace_back(1);
	BOOST_CHECK_EQUAL(vec[0], 1);
	BOOST_CHECK_EQUAL(list.front(), 1);
}

BOOST_AUTO_TEST_CASE(static_memory_pool_std_smart_ptr)
{
	std::vector<std::string, HsBa::Slicer::StaticMemoryPool<std::string, 1024 * sizeof(std::string)>> vec;
	vec.emplace_back("Hello");
	vec.emplace_back("World");
	std::shared_ptr<std::string> sp1 = std::allocate_shared<std::string>(HsBa::Slicer::StaticMemoryPool<std::string, 1024 * sizeof(std::string)>(), "Shared Hello");
	std::shared_ptr<std::string> sp2 = std::allocate_shared<std::string>(HsBa::Slicer::StaticMemoryPool<std::string, 1024 * sizeof(std::string)>(), "Shared World");
	BOOST_CHECK_EQUAL(*sp1, "Shared Hello");
	BOOST_CHECK_EQUAL(*sp2, "Shared World");
}

BOOST_AUTO_TEST_CASE(static_memory_pool_coroutine)
{
	TestPool pool;

	// Test CustomAllocatorTask
	HsBa::Slicer::Utils::CustomAllocatorTask<int, HsBa::Slicer::Utils::AsyncExecutor, TestPool>::SetAllocator(pool);
	auto task = simple_task();
	int result = task.get_result();
	BOOST_CHECK_EQUAL(result, 42);

	// Test CustomAllocatorGenerator
	HsBa::Slicer::Utils::CustomAllocatorGenerator<int, TestPool>::SetAllocator(pool);
	auto gen = simple_generator();
	std::vector<int> values;
	for (auto v : gen) {
		values.push_back(v);
	}
	BOOST_CHECK_EQUAL(values.size(), 3);
	BOOST_CHECK_EQUAL(values[0], 1);
	BOOST_CHECK_EQUAL(values[1], 2);
	BOOST_CHECK_EQUAL(values[2], 3);
}

BOOST_AUTO_TEST_SUITE_END()