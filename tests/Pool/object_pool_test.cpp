#define BOOST_TEST_MODULE object_pool_test
#include <boost/test/included/unit_test.hpp>

#include "base/object_pool.hpp"
#include "base/memory_pool.hpp"

BOOST_AUTO_TEST_SUITE(object_pool)

BOOST_AUTO_TEST_CASE(object_pool_normal)
{
    // Basic test for default allocation (emplace)
    HsBa::Slicer::NamedObjectPool<int, 10> pool;

    // Test emplace
    auto ptr1 = pool.emplace("obj1", 42);
    BOOST_CHECK(ptr1 != nullptr);
    BOOST_CHECK(*ptr1 == 42);
    BOOST_CHECK(pool.Contains("obj1"));
    BOOST_CHECK(pool.size() == 1);

    // Test get
    auto ptr2 = pool.get("obj1");
    BOOST_CHECK(ptr2 == ptr1);
    BOOST_CHECK(*ptr2 == 42);

    // Test operator[]
    auto ptr3 = pool["obj1"];
    BOOST_CHECK(ptr3 == ptr1);

    // Test non-existent object
    auto ptr4 = pool.get("nonexistent");
    BOOST_CHECK(ptr4 == nullptr);
}

BOOST_AUTO_TEST_CASE(object_pool_raii)
{
    // RAII test: ensure objects are properly managed
    HsBa::Slicer::NamedObjectPool<std::string, 5> pool;

    {
        auto ptr1 = pool.emplace("temp", "temporary");
        BOOST_CHECK(pool.Contains("temp"));
        BOOST_CHECK(pool.size() == 1);
        BOOST_CHECK(pool.ActiveCount() == 1);
        BOOST_CHECK(pool.InactiveCount() == 0);
    } // ptr1 goes out of scope here

    // After cleanup, the object should be inactive
    BOOST_CHECK(pool.Contains("temp"));
    BOOST_CHECK(pool.size() == 1);
    BOOST_CHECK(pool.ActiveCount() == 0);
    BOOST_CHECK(pool.InactiveCount() == 1);

    // Cleanup should remove inactive objects
    size_t cleaned = pool.Cleanup();
    BOOST_CHECK(cleaned == 1);
    BOOST_CHECK(pool.size() == 0);
    BOOST_CHECK(!pool.Contains("temp"));
}

BOOST_AUTO_TEST_CASE(object_pool_default_allocation_only)
{
    // Test using only default allocation (emplace)
    HsBa::Slicer::NamedObjectPool<double, 3> pool;

    auto ptr1 = pool.emplace("val1", 3.14);
    auto ptr2 = pool.emplace("val2", 2.71);
    auto ptr3 = pool.emplace("val3", 1.41);

    BOOST_CHECK(pool.size() == 3);
    BOOST_CHECK(pool.ActiveCount() == 3);

    // Test values
    BOOST_CHECK_CLOSE(*ptr1, 3.14, 0.001);
    BOOST_CHECK_CLOSE(*ptr2, 2.71, 0.001);
    BOOST_CHECK_CLOSE(*ptr3, 1.41, 0.001);

    // Test duplicate name throws
    BOOST_CHECK_THROW(pool.emplace("val1", 1.0), HsBa::Slicer::InvalidArgumentError);
}


BOOST_AUTO_TEST_CASE(object_pool_mixed_allocation)
{
    // Test mixing default allocation and MemoryPool allocation in the same pool
    // Use int for simplicity to avoid issues with complex types
    HsBa::Slicer::NamedObjectPool<int, 5> pool;
    HsBa::Slicer::MemoryPool<int, 4096> memPool;

    // Use default allocation
    auto ptr1 = pool.emplace("default_int", 42);
    BOOST_CHECK(ptr1 != nullptr);
    BOOST_CHECK(*ptr1 == 42);

    // Use MemoryPool allocation
    auto ptr2 = pool.allocate("memory_int", memPool, 99);
    BOOST_CHECK(ptr2 != nullptr);
    BOOST_CHECK(*ptr2 == 99);

    // Both should be in the pool
    BOOST_CHECK(pool.size() == 2);
    BOOST_CHECK(pool.Contains("default_int"));
    BOOST_CHECK(pool.Contains("memory_int"));

    // Test retrieval
    auto retrieved1 = pool.get("default_int");
    auto retrieved2 = pool.get("memory_int");
    BOOST_CHECK(retrieved1 == ptr1);
    BOOST_CHECK(retrieved2 == ptr2);

    // Test active count
    BOOST_CHECK(pool.ActiveCount() == 2);
}


BOOST_AUTO_TEST_CASE(object_pool_capacity_limits)
{
    // Test pool capacity limits
    HsBa::Slicer::NamedObjectPool<int, 2> smallPool;

    auto ptr1 = smallPool.emplace("obj1", 1);
    auto ptr2 = smallPool.emplace("obj2", 2);

    BOOST_CHECK(smallPool.size() == 2);

    // Should throw when full
    BOOST_CHECK_THROW(smallPool.emplace("obj3", 3), HsBa::Slicer::RuntimeError);

    // After making one inactive, should be able to add again
    ptr1.reset(); // Make obj1 inactive
    BOOST_CHECK(smallPool.ActiveCount() == 1);
    BOOST_CHECK(smallPool.InactiveCount() == 1);

    // Cleanup inactive
    smallPool.Cleanup();
    BOOST_CHECK(smallPool.size() == 1);

    // Now can add again
    auto ptr3 = smallPool.emplace("obj3", 3);
    BOOST_CHECK(smallPool.size() == 2);
}

BOOST_AUTO_TEST_CASE(object_pool_get_names)
{
    // Test GetNames functionality
    HsBa::Slicer::NamedObjectPool<int, 10> pool;

    pool.emplace("name1", 1);

    pool.emplace("name2", 2);

    pool.emplace("name3", 3);


    auto names = pool.GetNames();
    BOOST_CHECK(names.size() == 3);
    BOOST_CHECK(std::find(names.begin(), names.end(), "name1") != names.end());
    BOOST_CHECK(std::find(names.begin(), names.end(), "name2") != names.end());
    BOOST_CHECK(std::find(names.begin(), names.end(), "name3") != names.end());
}

BOOST_AUTO_TEST_SUITE_END()