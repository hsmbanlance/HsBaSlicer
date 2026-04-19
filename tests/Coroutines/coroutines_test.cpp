#define BOOST_TEST_MODULE coroutines_test
#include <boost/test/included/unit_test.hpp>

#include <base/template_helper.hpp>
#include <base/coroutine.hpp>

BOOST_AUTO_TEST_SUITE(generator_tasks)

BOOST_AUTO_TEST_CASE(test_generator)
{
	BOOST_TEST_MESSAGE("Run generator test");
	auto gen = []()->HsBa::Slicer::Utils::Generator<int> {
		co_yield 1; co_yield 2; co_return; co_yield 4; }();
		int i = 1;
		for (const auto& v : gen)
		{
			BOOST_CHECK_MESSAGE(i == v,"corroutine yield wrong value");
			++i;
		}
		BOOST_REQUIRE_MESSAGE(i == 3,"coroutine yield wrong number of values");
}

BOOST_AUTO_TEST_CASE(test_generator_allocator)
{
	BOOST_TEST_MESSAGE("Run generator with std::allocator test");
	auto gen = []()->HsBa::Slicer::Utils::CustomAllocatorGenerator<int> {
		co_yield 1; co_yield 2; co_return; co_yield 4; }();
		int i = 1;
		for (const auto& v : gen)
		{
			BOOST_CHECK_MESSAGE(i == v, "corroutine yield wrong value");
			++i;
		}
		BOOST_REQUIRE_MESSAGE(i == 3, "coroutine yield wrong number of values");
}

BOOST_AUTO_TEST_CASE(test_generator_invoke)
{
	BOOST_TEST_MESSAGE("Run generator invoke test");
	std::list<int> l{ 0, 1, 2, 3, 4, 5 };
	auto gen = HsBa::Slicer::Utils::GeneratorInvoke<int,std::list>([](int i) {return i * i; }, l);
	int i = 0;
	for (const auto v : gen)
	{
		BOOST_CHECK_MESSAGE(v == i * i,"generator invoke wrong value");
		++i;
	}
	BOOST_REQUIRE_MESSAGE(i == 6,"generator invoke wrong number of values");
}

BOOST_AUTO_TEST_CASE(test_generator_invoke_throw)
{
	BOOST_TEST_MESSAGE("Run generator invoke throw test");
	HsBa::Slicer::Utils::Generator<int>::SetOnCancel([]() {
		BOOST_TEST_MESSAGE("Generator canceled"); }
	);
	std::list<int> l{ 0, 1, 2, 3, 4, 5 };
	auto gen = HsBa::Slicer::Utils::GeneratorInvoke<int, std::list>(
	[](int i) {
		if (i >= 3)
		{
			throw std::length_error("Simulated exception");
		}
		return i * i;
		}, l);
	int i = 0;
	try
	{
		for (const auto v : gen)
		{
			BOOST_REQUIRE(v == i * i);
			++i;
		}
	}
	catch (const std::exception&) {}
	BOOST_REQUIRE_MESSAGE(i == 3,"coroutine stoped too early or too late");
}

BOOST_AUTO_TEST_CASE(test_tasks_void)
{
	auto task = []()->HsBa::Slicer::Utils::Task<void> {
		HsBa::Slicer::Utils::Task<int> exampleTask = []() ->HsBa::Slicer::Utils::Task<int> {
			BOOST_TEST_MESSAGE("co_await int");
			co_return 42;
			}();
		auto aw = co_await std::move(exampleTask);
		BOOST_CHECK_MESSAGE(aw == 42,"coroutine await wrong value");
		BOOST_TEST_MESSAGE("this is a task which return void.");
		co_return; }();
		BOOST_REQUIRE(true);
}

BOOST_AUTO_TEST_CASE(test_tasks_int)
{
	BOOST_TEST_MESSAGE("Run tasks int test");
	auto task = []()->HsBa::Slicer::Utils::Task<int> {
		HsBa::Slicer::Utils::Task<void> exampleTask = []() ->HsBa::Slicer::Utils::Task<void> {
			//
			BOOST_TEST_MESSAGE("co_await void");
			co_return;
			}();
		co_await std::move(exampleTask);
		BOOST_TEST_MESSAGE("this is a task which return int.");
		co_return 42; }();
		BOOST_REQUIRE_MESSAGE(task.get_result() == 42,"coroutine return wrong value");
}

BOOST_AUTO_TEST_CASE(test_tasks_int_allocator)
{
	BOOST_TEST_MESSAGE("Run tasks int with std::allocator test");
	auto task = []()->HsBa::Slicer::Utils::CustomAllocatorTask<int> {
		HsBa::Slicer::Utils::Task<void> exampleTask = []() ->HsBa::Slicer::Utils::Task<void> {
			//
			BOOST_TEST_MESSAGE("co_await void");
			co_return;
			}();
		co_await std::move(exampleTask);
		BOOST_TEST_MESSAGE("this is a task which return int.");
		co_return 42; }();
		BOOST_REQUIRE_MESSAGE(task.get_result() == 42, "coroutine return wrong value");
}

	BOOST_AUTO_TEST_CASE(test_tasks_int_noop)
	{
		BOOST_TEST_MESSAGE("Run tasks int with NoopExecutor");
		auto task = []()->HsBa::Slicer::Utils::Task<int, HsBa::Slicer::Utils::NoopExecutor> {
			HsBa::Slicer::Utils::Task<void, HsBa::Slicer::Utils::NoopExecutor> exampleTask = []() ->HsBa::Slicer::Utils::Task<void, HsBa::Slicer::Utils::NoopExecutor> {
				BOOST_TEST_MESSAGE("co_await void");
				co_return;
				}();
			co_await std::move(exampleTask);
			BOOST_TEST_MESSAGE("this is a task which return int.");
			co_return 99; }();
		BOOST_REQUIRE_MESSAGE(task.get_result() == 99, "coroutine return wrong value");
	}

	BOOST_AUTO_TEST_CASE(test_tasks_void_noop)
	{
		BOOST_TEST_MESSAGE("Run tasks void with NoopExecutor");
		auto task = []()->HsBa::Slicer::Utils::Task<void, HsBa::Slicer::Utils::NoopExecutor> {
			HsBa::Slicer::Utils::Task<int, HsBa::Slicer::Utils::NoopExecutor> exampleTask = []() ->HsBa::Slicer::Utils::Task<int, HsBa::Slicer::Utils::NoopExecutor> {
				BOOST_TEST_MESSAGE("co_await int");
				co_return 8;
				}();
			auto aw = co_await std::move(exampleTask);
			BOOST_CHECK_MESSAGE(aw == 8, "coroutine await wrong value");
			BOOST_TEST_MESSAGE("this is a task which return void.");
			co_return; }();
		BOOST_REQUIRE(true);
	}

	BOOST_AUTO_TEST_CASE(test_tasks_throw_noop_int)
	{
		BOOST_TEST_MESSAGE("Run tasks throw int with NoopExecutor");
		auto task = []()->HsBa::Slicer::Utils::Task<int, HsBa::Slicer::Utils::NoopExecutor> {
			throw std::runtime_error("simulated-noop");
			co_return 3; }();
		bool thrown = false;
		try
		{
			task.get_result();
		}
		catch (const std::exception&)
		{
			thrown = true;
		}
		BOOST_REQUIRE_MESSAGE(thrown, "exception from coroutine not propagated");
	}

	BOOST_AUTO_TEST_CASE(test_tasks_throw_noop_void)
	{
		BOOST_TEST_MESSAGE("Run tasks throw void with NoopExecutor");
		auto task = []()->HsBa::Slicer::Utils::Task<void, HsBa::Slicer::Utils::NoopExecutor> {
			throw std::logic_error("simulated-void-noop");
			co_return; }();
		bool thrown = false;
		try
		{
			task.get_result();
		}
		catch (const std::exception&)
		{
			thrown = true;
		}
		BOOST_REQUIRE_MESSAGE(thrown, "exception from void coroutine not propagated");
	}

	BOOST_AUTO_TEST_CASE(test_tasks_catching_noop)
	{
		BOOST_TEST_MESSAGE("Run tasks catching with NoopExecutor");
		std::atomic<bool> callback_called{ false };
		auto task = []()->HsBa::Slicer::Utils::Task<int, HsBa::Slicer::Utils::NoopExecutor> {
			throw std::runtime_error("simulated-catch-noop");
			co_return 5; }();
		task.catching([&](std::exception&){ callback_called = true; });
		bool thrown = false;
		try { task.get_result(); }
		catch (const std::exception&) { thrown = true; }
		BOOST_REQUIRE_MESSAGE(thrown, "exception not propagated");
		BOOST_REQUIRE_MESSAGE(callback_called.load(), "catching callback not invoked");
	}

BOOST_AUTO_TEST_SUITE_END()