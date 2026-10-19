#define BOOST_TEST_MODULE app_config_test
#include <boost/test/included/unit_test.hpp>

#include "utils/app_config.hpp"

#include <string>
#include <thread>
#include <vector>

// AppConfigSingletone is a lazily-constructed, mutex-guarded singleton. These cases
// cover GetInstance (both the fast "already constructed" path and the locked
// double-checked-creation path), DeleteInstance, and GetSevenZPath.
BOOST_AUTO_TEST_SUITE(app_config)

// Two GetInstance calls must hand back the very same object (singleton identity).
BOOST_AUTO_TEST_CASE(get_instance_is_stable)
{
    HsBa::Slicer::AppConfigSingletone& a = HsBa::Slicer::AppConfigSingletone::GetInstance();
    HsBa::Slicer::AppConfigSingletone& b = HsBa::Slicer::AppConfigSingletone::GetInstance();
    BOOST_CHECK_EQUAL(&a, &b);
}

// The default-constructed config exposes an empty 7-Zip path.
BOOST_AUTO_TEST_CASE(seven_z_path_defaults_empty)
{
    HsBa::Slicer::AppConfigSingletone& cfg = HsBa::Slicer::AppConfigSingletone::GetInstance();
    BOOST_CHECK_EQUAL(cfg.GetSevenZPath(), std::string{});
}

// DeleteInstance must destroy the cached object and reset the pointer so the next
// GetInstance lazily rebuilds a usable instance. We cannot assert on a changed
// address here: after `delete`, the allocator commonly reuses the same heap block for
// the subsequent `new`, so the recreated object very often has an identical address.
// The meaningful guarantees are "still constructible/usable after deletion" and
// "deleting an already-deleted singleton is a safe no-op".
BOOST_AUTO_TEST_CASE(delete_then_recreate)
{
    HsBa::Slicer::AppConfigSingletone::DeleteInstance();
    HsBa::Slicer::AppConfigSingletone& cfg = HsBa::Slicer::AppConfigSingletone::GetInstance();
    BOOST_CHECK_EQUAL(cfg.GetSevenZPath(), std::string{});
    // Deleting again (and once more with nothing live) must not crash.
    HsBa::Slicer::AppConfigSingletone::DeleteInstance();
    BOOST_CHECK_NO_THROW(HsBa::Slicer::AppConfigSingletone::DeleteInstance());
}

// Hammer GetInstance from many threads: the double-checked locking must yield exactly
// one shared instance (covers the std::unique_lock creation branch under contention).
BOOST_AUTO_TEST_CASE(concurrent_get_instance_single_instance)
{
    constexpr int kThreads = 8;
    std::vector<HsBa::Slicer::AppConfigSingletone*> results(kThreads, nullptr);
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i)
    {
        workers.emplace_back([&results, i] { results[i] = &HsBa::Slicer::AppConfigSingletone::GetInstance(); });
    }
    for (auto& t : workers)
        t.join();

    for (int i = 1; i < kThreads; ++i)
        BOOST_CHECK_EQUAL(results[0], results[i]);

    HsBa::Slicer::AppConfigSingletone::DeleteInstance();
}

BOOST_AUTO_TEST_SUITE_END()
