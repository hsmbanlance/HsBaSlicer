#include "base/template_helper.hpp"

// static enums name tests

enum class Enum
{
    First,
    Second,
    Third
};

static_assert(HsBa::Slicer::Utils::EnumName<Enum::First>() == "First");
static_assert(HsBa::Slicer::Utils::EnumName<Enum::Second>() == "Second");
static_assert(HsBa::Slicer::Utils::EnumName<Enum::Third>() == "Third");

static_assert(HsBa::Slicer::Utils::EnumName(Enum::First) == "First");
static_assert(HsBa::Slicer::Utils::EnumName(Enum::Second) == "Second");
static_assert(HsBa::Slicer::Utils::EnumName(Enum::Third) == "Third");

static_assert(HsBa::Slicer::Utils::EnumFromName<Enum>("First") == Enum::First);
static_assert(HsBa::Slicer::Utils::EnumFromName<Enum>("Second") == Enum::Second);
static_assert(HsBa::Slicer::Utils::EnumFromName<Enum>("Third") == Enum::Third);

class NonCopyable
{
public:
    NonCopyable(int value) : value_(value) {}
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    int GetValue() const { return value_; }

private:
    int value_;
};

void TemplateHelperTests()
{
    // just to make sure the code compiles, no runtime test
    HsBa::Slicer::Utils::NamedRawPtr<"Test", int> ptr;
    static_assert(ptr.Name() == "Test");
    static_assert(std::is_same_v<decltype(ptr.Get()), int*>);

    HsBa::Slicer::Utils::template_call<"abc">([](std::string_view, int) {}, 1);

    HsBa::Slicer::Utils::MakeNonCopyableArray<NonCopyable, 3>(1);
}

// static any visit tests

#include <base/any_visit.hpp>

void AnyVisitTests()
{
    std::any a = 1;
    HsBa::Slicer::Utils::Visit<int>([](auto&& arg, int)
                                    { static_assert(std::is_same_v<std::decay_t<decltype(arg)>, int>); }, a, 1);

    HsBa::Slicer::Utils::Visit<int>(
        [](auto&& arg, int)
        {
            static_assert(std::is_same_v<std::decay_t<decltype(arg)>, int>);
            return arg + 1;
        },
        a, 1);

    // must return same type for all types
    HsBa::Slicer::Utils::Visit<int, double>([](auto&& arg) { return static_cast<double>(arg + 1); }, a);

    std::any b = 2.0;

    HsBa::Slicer::Utils::Visit<int, double>(
        HsBa::Slicer::Utils::Overloaded{[](int item, int, double)
                                        {
                                            static_assert(std::is_same_v<std::decay_t<decltype(item)>, int>);
                                            return 0;
                                        },
                                        [](double item, int, double)
                                        {
                                            static_assert(std::is_same_v<std::decay_t<decltype(item)>, double>);
                                            return 0;
                                        }},
        b, 1, 2.0);
}

// static tuple each tests

#include <base/tuple_each.hpp>

void TupleEachTests()
{
    auto tuple = std::make_tuple(1, 2.0, "test");
    HsBa::Slicer::Utils::TupleEach(
        [](auto&& item)
        {
            using T = std::decay_t<decltype(item)>;
            static_assert(std::is_same_v<T, int> || std::is_same_v<T, double> || std::is_same_v<T, const char*>);
        },
        tuple);

    HsBa::Slicer::Utils::TupleEach(
        [](auto&& item, int)
        {
            using T = std::decay_t<decltype(item)>;
            static_assert(std::is_same_v<T, int> || std::is_same_v<T, double> || std::is_same_v<T, const char*>);
        },
        tuple, 1);

    HsBa::Slicer::Utils::TupleEach(
        [](auto&& item, int, double)
        {
            using T = std::decay_t<decltype(item)>;
            static_assert(std::is_same_v<T, int> || std::is_same_v<T, double> || std::is_same_v<T, const char*>);
            return 0;
        },
        tuple, 1, 2.0);

    HsBa::Slicer::Utils::TupleEach(
        HsBa::Slicer::Utils::Overloaded{[](int item, int, double)
                                        {
                                            static_assert(std::is_same_v<std::decay_t<decltype(item)>, int>);
                                            return 0;
                                        },
                                        [](double item, int, double)
                                        {
                                            static_assert(std::is_same_v<std::decay_t<decltype(item)>, double>);
                                            return 0;
                                        },
                                        [](const char* item, int, double)
                                        {
                                            static_assert(std::is_same_v<std::decay_t<decltype(item)>, const char*>);
                                            return 0;
                                        }},
        tuple, 1, 2.0);
}

// static coroutine tests

#if __cpp_lib_coroutine && __cpp_impl_coroutine
#include <base/coroutine.hpp>

HsBa::Slicer::Utils::Task<int> CoroutineTests()
{
    co_return 42;
}

HsBa::Slicer::Utils::Task<void> CoroutineTestsVoid()
{
    co_return;
}

HsBa::Slicer::Utils::Task<int> CoroutineTestsException()
{
    throw std::runtime_error("test exception");
    co_return 42;
}

HsBa::Slicer::Utils::CustomAllocatorTask<int, HsBa::Slicer::Utils::AsyncExecutor, std::allocator<int>>
CustomAllocatorCoroutineTests()
{
    co_return 42;
}

HsBa::Slicer::Utils::CustomAllocatorTask<int, HsBa::Slicer::Utils::AsyncExecutor, std::allocator<int>>
CustomAllocatorCoroutineTestsException()
{
    throw std::runtime_error("test exception");
    co_return 42;
}

HsBa::Slicer::Utils::Generator<int> GeneratorTests()
{
    for (int i = 0; i < 5; ++i)
    {
        co_yield i;
    }
}

#endif  // __cpp_lib_coroutine && __cpp_impl_coroutine

// test any object

#include <base/any_object.hpp>

class TestAnyObject
{
public:
    void TestFunc() {}

private:
    int value_;
};

namespace HsBa::Slicer::Utils
{
template <>
TypeInfo* GetTypeInfo<TestAnyObject>()
{
    static TypeInfo info;
    info.Name = typeid(TestAnyObject).name();
    info.destroy = [](void* data) { delete static_cast<TestAnyObject*>(data); };
    info.copy = [](const void* data) -> void* { return new TestAnyObject(*static_cast<const TestAnyObject*>(data)); };
    info.move = [](void* data) -> void* { return new TestAnyObject(std::move(*static_cast<TestAnyObject*>(data))); };
    info.methods["TestFunc"] = +[](void* obj, std::span<AnyObject>)
    {
        static_cast<TestAnyObject*>(obj)->TestFunc();
        return AnyObject();
    };
    info.fields["value_"] = {GetTypeInfo<int>(), 0};
    return &info;
}
}  // namespace HsBa::Slicer::Utils

void AnyObjectTests()
{
    HsBa::Slicer::Utils::AnyObject obj = TestAnyObject();
    obj.Invoke("TestFunc", {});
    obj.ForeachField(
        [](std::string_view name, HsBa::Slicer::Utils::AnyObject field)
        {
            static_assert(std::is_same_v<std::decay_t<decltype(name)>, std::string_view>);
            static_assert(std::is_same_v<std::decay_t<decltype(field)>, HsBa::Slicer::Utils::AnyObject>);
        });
}