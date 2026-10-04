/** @file any_object.hpp
 * @brief A header file containing the definition of the AnyObject class.
 * @author HsBa
 * @date 2024-06
 */
#pragma once
#ifndef HSBA_SLICER_ANY_OBJECT_HPP

#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "concepts.hpp"
#include "error.hpp"

// ---------------------------------------------------------------------------
// Mock support gating
// ---------------------------------------------------------------------------
// Auto-enable the Mockit-like stubbing facility when any common test framework
// macro is visible in the current translation unit, or when the user opts in
// explicitly via HSBA_ANY_OBJECT_ENABLE_MOCK. When the macro is not defined,
// none of the mock code below is compiled and AnyObject::Invoke has zero
// additional overhead.
#if !defined(HSBA_ANY_OBJECT_ENABLE_MOCK)
#if defined(BOOST_TEST_MODULE) || defined(BOOST_TEST_INCLUDED) || defined(BOOST_TEST_DYN_LINK) ||                      \
    defined(BOOST_TEST_ALTERNATIVE_INIT_API) || defined(GTEST_INCLUDE_GTEST_GTEST_H_) || defined(GTEST_API_) ||        \
    defined(GTEST_HAS_MOCK) || defined(CATCH_VERSION_MAJOR) || defined(CATCH_CONFIG_MAIN) ||                           \
    defined(CATCH_CONFIG_RUNNER) || defined(DOCTEST_LIBRARY_INCLUDED) || defined(DOCTEST_CONFIG_IMPLEMENT) ||          \
    defined(DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)
#define HSBA_ANY_OBJECT_ENABLE_MOCK 1
#endif
#endif

namespace HsBa::Slicer::Utils
{
class AnyObject;
/** @brief A structure containing type information for AnyObject instances.
 */
struct TypeInfo
{
    std::string_view Name;
    void (*destroy)(void*);
    void* (*copy)(const void*);
    void* (*move)(void*);

    using Field = std::pair<TypeInfo*, size_t>;
    using Method = AnyObject (*)(void*, std::span<AnyObject>);
    std::unordered_map<std::string_view, Field> fields;
    std::unordered_map<std::string_view, Method> methods;
};
/** @brief Get the type info for type T.
 * @tparam T The type for which to get info.
 * @return A pointer to the type info.
 */
template <typename T>
TypeInfo* GetTypeInfo();
/** @brief A type-erased object wrapper backed by TypeInfo for reflection and method dispatch.
 *
 * Holds an opaque value pointer together with its TypeInfo, supporting copy/move, typed
 * cast<T>(), reflective field traversal (ForeachField) and method invocation (Invoke).
 */
class AnyObject
{
public:
    AnyObject() : type_info(nullptr), data(nullptr), flag(0) {}
    AnyObject(TypeInfo* type_info, void* data) : type_info(type_info), data(data), flag(0) {}
    AnyObject(const AnyObject& other);
    AnyObject(AnyObject&& other) noexcept;
    ~AnyObject();
    AnyObject& operator=(const AnyObject& other);
    AnyObject& operator=(AnyObject&& other) noexcept;
    template <typename T, typename = std::enable_if_t<!std::is_same_v<std::remove_cvref_t<T>, AnyObject>>>
    AnyObject(T&& value);
    template <typename T>
    T& cast()
    {
        if (!type_info || type_info != GetTypeInfo<std::remove_cvref_t<T>>())
        {
            throw RuntimeError("Bad AnyObject cast: type mismatch");
        }
        return *static_cast<T*>(data);
    }
    template <typename T>
    T cast_new()
    {
        if (!type_info || type_info != GetTypeInfo<std::remove_cvref_t<T>>())
        {
            throw RuntimeError("Bad AnyObject cast: type mismatch");
        }
        return *static_cast<T*>(data);
    }
    TypeInfo* get_type_info() const { return type_info; }
    /** @brief Read-only accessor for the underlying data pointer.
     *
     * Pure getter: it does not alter ownership (the `flag` semantics are untouched) nor any
     * behavior. Provided so reflection consumers (e.g. ParamStore field traversal) can reach a
     * sub-object's address without needing to name its concrete type via cast<T>(). */
    void* get_data() const noexcept { return data; }
    AnyObject Invoke(std::string_view method_name, std::span<AnyObject> args);
    void ForeachField(const std::function<void(std::string_view, AnyObject)>& callback);

private:
    TypeInfo* type_info;
    void* data;
    uint8_t flag;
};

#ifdef HSBA_ANY_OBJECT_ENABLE_MOCK
namespace Mock
{
/** @brief Recorded information about a single mocked call.
 */
struct CallRecord
{
    std::string method_name;
    std::vector<AnyObject> args;
};

/** @brief Signature of a mocked action.
 *
 * The action receives the target object's raw pointer (`self`) and the
 * argument list of the intercepted call. It may either return an AnyObject
 * (which becomes the result of AnyObject::Invoke) or throw any exception,
 * in which case the exception propagates out of Invoke().
 */
using Action = std::function<AnyObject(void* /*self*/, std::span<AnyObject> /*args*/)>;
/// Legacy alias kept for readability at existing stub sites.
using MockFn = Action;
/// Predicate on the argument list of a mocked call.
using ArgPredicate = std::function<bool(std::span<AnyObject> /*args*/)>;

/** @brief A single argument-conditional rule: (predicate, action).
 *
 * Rules registered for the same (type, method) pair are evaluated in
 * insertion order; the first rule whose predicate returns true wins. Rules
 * take precedence over the plain stub installed via stub()/stub_return().
 */
struct Rule
{
    ArgPredicate predicate;
    Action action;
};

// -------------------------------------------------------------------------
// Action factories
// -------------------------------------------------------------------------

/// Build an action that always returns a copy of @p value.
template <typename R>
inline Action Return(R value)
{
    return [v = std::move(value)](void*, std::span<AnyObject>) mutable -> AnyObject { return AnyObject{v}; };
}

/// Build an action that always rethrows @p eptr.
inline Action Throw(std::exception_ptr eptr)
{
    return [eptr](void*, std::span<AnyObject>) -> AnyObject { std::rethrow_exception(eptr); };
}

/// Build an action that constructs @c E(args...) and throws it every call.
template <typename E, typename... Args>
inline Action ThrowOf(Args&&... args)
{
    return Throw(std::make_exception_ptr(E(std::forward<Args>(args)...)));
}

/// Wrap any user callable (lambda / function pointer) into an Action.
template <typename F>
inline Action Do(F&& f)
{
    return [fn = std::forward<F>(f)](void* self, std::span<AnyObject> args) -> AnyObject { return fn(self, args); };
}

// -------------------------------------------------------------------------
// Predicate factories
// -------------------------------------------------------------------------

/// Predicate that matches every argument list.
inline ArgPredicate AnyArgs()
{
    return [](std::span<AnyObject>) { return true; };
}

/// Predicate that matches when the argument count equals @p n.
inline ArgPredicate ArgCountIs(std::size_t n)
{
    return [n](std::span<AnyObject> args) { return args.size() == n; };
}

/// Predicate that matches when args[index] holds a V equal to @p expected.
/// Non-copyable / mismatched types evaluate to false instead of throwing.
template <typename V>
inline ArgPredicate ArgAtIs(std::size_t index, V expected)
{
    using VT = std::remove_cvref_t<V>;
    return [index, e = std::move(expected)](std::span<AnyObject> args)
    {
        if (args.size() <= index)
            return false;
        try
        {
            return args[index].cast<VT>() == e;
        }
        catch (...)
        {
            return false;
        }
    };
}

/// Predicate that matches when args[index] satisfies a user-provided predicate.
template <typename V>
inline ArgPredicate ArgAtMatches(std::size_t index, std::function<bool(const std::remove_cvref_t<V>&)> pred)
{
    using VT = std::remove_cvref_t<V>;
    return [index, p = std::move(pred)](std::span<AnyObject> args)
    {
        if (args.size() <= index)
            return false;
        try
        {
            return p(args[index].cast<VT>());
        }
        catch (...)
        {
            return false;
        }
    };
}

/// Conjunction of predicates.
template <typename... Ps>
inline ArgPredicate AllOf(Ps... ps)
{
    return [... ps = std::move(ps)](std::span<AnyObject> args) { return (ps(args) && ...); };
}

/// Disjunction of predicates.
template <typename... Ps>
inline ArgPredicate AnyOf(Ps... ps)
{
    return [... ps = std::move(ps)](std::span<AnyObject> args) { return (ps(args) || ...); };
}

/// Negation of a predicate.
inline ArgPredicate Not(ArgPredicate p)
{
    return [p = std::move(p)](std::span<AnyObject> args) { return !p(args); };
}

/** @brief Mockit-like registry that lets tests stub AnyObject::Invoke results.
 *
 * The registry is compiled in only when a common test framework macro is
 * detected (see HSBA_ANY_OBJECT_ENABLE_MOCK). Even when compiled in, mocking
 * is disabled by default: call enable() or use ScopedEnable for RAII
 * activation so that unrelated test cases observe the real behaviour.
 *
 * Stubs are keyed by (TypeInfo*, method_name). When mocking is enabled and a
 * matching stub exists, AnyObject::Invoke returns the stub's result instead of
 * dispatching to the real method. Calls that do not match any stub fall
 * through to the real implementation.
 *
 * All operations are thread-safe (guarded by an internal mutex), which allows
 * tests running in parallel to install/inspect stubs safely.
 */
class MockRegistry
{
public:
    static MockRegistry& instance()
    {
        static MockRegistry reg;
        return reg;
    }

    /// Manually turn mocking on.
    void enable()
    {
        std::lock_guard<std::mutex> lk(mu_);
        enabled_ = true;
    }
    /// Manually turn mocking off. Stubs are preserved so they can be reused.
    void disable()
    {
        std::lock_guard<std::mutex> lk(mu_);
        enabled_ = false;
    }
    void set_enabled(bool on)
    {
        std::lock_guard<std::mutex> lk(mu_);
        enabled_ = on;
    }
    bool is_enabled() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return enabled_;
    }

    /// Install a stub for a specific (type, method) pair.
    void stub(TypeInfo* type, std::string_view method_name, MockFn fn)
    {
        if (!type)
            return;
        std::lock_guard<std::mutex> lk(mu_);
        mocks_[type][std::string(method_name)] = std::move(fn);
    }

    /// Install a stub for a specific (type, method) pair.
    template <typename T>
    void stub(std::string_view method_name, MockFn fn)
    {
        stub(GetTypeInfo<std::remove_cvref_t<T>>(), method_name, std::move(fn));
    }

    /// Install a stub that always returns the given value.
    template <typename R>
    void stub_return(TypeInfo* type, std::string_view method_name, R value)
    {
        stub(type, method_name,
             [v = std::move(value)](void*, std::span<AnyObject>) mutable -> AnyObject { return AnyObject{v}; });
    }

    template <typename T, typename R>
    void stub_return(std::string_view method_name, R value)
    {
        stub_return(GetTypeInfo<std::remove_cvref_t<T>>(), method_name, std::move(value));
    }

    /// Install a stub that always rethrows @p eptr.
    void stub_throw(TypeInfo* type, std::string_view method_name, std::exception_ptr eptr)
    {
        stub(type, method_name, Throw(eptr));
    }

    template <typename T>
    void stub_throw(std::string_view method_name, std::exception_ptr eptr)
    {
        stub_throw(GetTypeInfo<std::remove_cvref_t<T>>(), method_name, eptr);
    }

    /// Install a stub that constructs @c E(args...) and throws it every call.
    template <typename T, typename E, typename... Args>
    void stub_throw(std::string_view method_name, Args&&... args)
    {
        stub_throw<T>(method_name, std::make_exception_ptr(E(std::forward<Args>(args)...)));
    }

    /** @brief Register an argument-conditional rule.
     *
     * Rules for the same (type, method) pair are evaluated in insertion
     * order and take precedence over any plain stub installed via stub() /
     * stub_return() / stub_throw(). The first rule whose predicate returns
     * true handles the call; if none matches, the plain stub (if any) is
     * used, otherwise AnyObject::Invoke falls through to the real method.
     */
    void add_rule(TypeInfo* type, std::string_view method_name, ArgPredicate pred, Action act)
    {
        if (!type)
            return;
        std::lock_guard<std::mutex> lk(mu_);
        rules_[type][std::string(method_name)].push_back(Rule{std::move(pred), std::move(act)});
    }

    template <typename T>
    void add_rule(std::string_view method_name, ArgPredicate pred, Action act)
    {
        add_rule(GetTypeInfo<std::remove_cvref_t<T>>(), method_name, std::move(pred), std::move(act));
    }

    /// Convenience: register a rule that fires for any argument list.
    template <typename T>
    void when_called(std::string_view method_name, Action act)
    {
        add_rule<T>(method_name, AnyArgs(), std::move(act));
    }

    /// Number of rules currently registered for a (type, method) pair.
    std::size_t rule_count(TypeInfo* type, std::string_view method_name) const
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = rules_.find(type);
        if (it == rules_.end())
            return 0;
        auto mit = it->second.find(std::string(method_name));
        return mit == it->second.end() ? 0 : mit->second.size();
    }

    template <typename T>
    std::size_t rule_count(std::string_view method_name) const
    {
        return rule_count(GetTypeInfo<std::remove_cvref_t<T>>(), method_name);
    }

    /// Remove every rule registered for a (type, method) pair.
    void clear_rules(TypeInfo* type, std::string_view method_name)
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = rules_.find(type);
        if (it == rules_.end())
            return;
        it->second.erase(std::string(method_name));
    }

    template <typename T>
    void clear_rules(std::string_view method_name)
    {
        clear_rules(GetTypeInfo<std::remove_cvref_t<T>>(), method_name);
    }

    /// Remove a single stub.
    void unstub(TypeInfo* type, std::string_view method_name)
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = mocks_.find(type);
        if (it == mocks_.end())
            return;
        it->second.erase(std::string(method_name));
    }

    template <typename T>
    void unstub(std::string_view method_name)
    {
        unstub(GetTypeInfo<std::remove_cvref_t<T>>(), method_name);
    }

    /// Drop every stub and rule registered for a given type.
    void clear(TypeInfo* type)
    {
        std::lock_guard<std::mutex> lk(mu_);
        mocks_.erase(type);
        rules_.erase(type);
    }

    /// Drop every stub, rule and recorded call.
    void clear()
    {
        std::lock_guard<std::mutex> lk(mu_);
        mocks_.clear();
        rules_.clear();
        calls_.clear();
    }

    /// Number of recorded calls for a (type, method) pair.
    std::size_t call_count(TypeInfo* type, std::string_view method_name) const
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = calls_.find(type);
        if (it == calls_.end())
            return 0;
        auto mit = it->second.find(std::string(method_name));
        return mit == it->second.end() ? 0 : mit->second.size();
    }

    template <typename T>
    std::size_t call_count(std::string_view method_name) const
    {
        return call_count(GetTypeInfo<std::remove_cvref_t<T>>(), method_name);
    }

    /// Recorded calls for a (type, method) pair, in call order.
    std::vector<CallRecord> calls(TypeInfo* type, std::string_view method_name) const
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = calls_.find(type);
        if (it == calls_.end())
            return {};
        auto mit = it->second.find(std::string(method_name));
        return mit == it->second.end() ? std::vector<CallRecord>{} : mit->second;
    }

    template <typename T>
    std::vector<CallRecord> calls(std::string_view method_name) const
    {
        return calls(GetTypeInfo<std::remove_cvref_t<T>>(), method_name);
    }

    /// Discard every recorded call without touching the stubs.
    void reset_calls()
    {
        std::lock_guard<std::mutex> lk(mu_);
        calls_.clear();
    }

    /** @brief Internal dispatch used by AnyObject::Invoke.
     *
     * Returns true when the call was served by the mock (a matching rule or
     * the plain stub) and writes the produced value to @p out. Any exception
     * thrown by the chosen action propagates to the caller. Returns false
     * when mocking is disabled or neither a rule nor a stub matches, in
     * which case AnyObject::Invoke falls through to the real method.
     */
    bool try_invoke(TypeInfo* type, std::string_view method_name, void* data, std::span<AnyObject> args, AnyObject& out)
    {
        std::vector<Rule> rules_snapshot;
        MockFn fallback;
        bool has_fallback = false;
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!enabled_ || !type)
                return false;
            auto rit = rules_.find(type);
            if (rit != rules_.end())
            {
                auto rmit = rit->second.find(std::string(method_name));
                if (rmit != rit->second.end())
                    rules_snapshot = rmit->second;
            }
            auto mit = mocks_.find(type);
            if (mit != mocks_.end())
            {
                auto mmit = mit->second.find(std::string(method_name));
                if (mmit != mit->second.end())
                {
                    fallback = mmit->second;
                    has_fallback = true;
                }
            }
            if (rules_snapshot.empty() && !has_fallback)
                return false;
        }

        // Evaluate rules outside the lock so user predicates/actions may
        // safely re-enter the registry. First match wins.
        const Action* chosen = nullptr;
        for (const auto& r : rules_snapshot)
        {
            if (!r.predicate || r.predicate(args))
            {
                chosen = &r.action;
                break;
            }
        }
        if (!chosen && !has_fallback)
            return false;  // no rule matched and no plain stub -> real method

        // The call will be served by the mock: record it before invoking so
        // the action body (and post-hoc inspection) can observe the record.
        {
            std::lock_guard<std::mutex> lk(mu_);
            CallRecord rec;
            rec.method_name = std::string(method_name);
            rec.args.assign(args.begin(), args.end());
            calls_[type][rec.method_name].push_back(std::move(rec));
        }

        if (chosen)
        {
            out = *chosen ? (*chosen)(data, args) : AnyObject{};
        }
        else
        {
            out = fallback ? fallback(data, args) : AnyObject{};
        }
        return true;
    }

    /// RAII helper: enable mocking on construction, restore previous state on destruction.
    class ScopedEnable
    {
    public:
        explicit ScopedEnable(MockRegistry& reg = MockRegistry::instance()) : reg_(reg), prev_(reg.is_enabled())
        {
            reg_.enable();
        }
        ~ScopedEnable() { reg_.set_enabled(prev_); }
        ScopedEnable(const ScopedEnable&) = delete;
        ScopedEnable& operator=(const ScopedEnable&) = delete;

    private:
        MockRegistry& reg_;
        bool prev_;
    };

    /// RAII helper: enable mocking and clear every stub / recorded call on destruction.
    /// Useful inside a BOOST_AUTO_TEST_CASE body to guarantee isolation.
    class ScopedStubs
    {
    public:
        explicit ScopedStubs(MockRegistry& reg = MockRegistry::instance()) : reg_(reg), prev_(reg.is_enabled())
        {
            reg_.clear();
            reg_.enable();
        }
        ~ScopedStubs()
        {
            reg_.clear();
            reg_.set_enabled(prev_);
        }
        ScopedStubs(const ScopedStubs&) = delete;
        ScopedStubs& operator=(const ScopedStubs&) = delete;
        MockRegistry& registry() noexcept { return reg_; }

    private:
        MockRegistry& reg_;
        bool prev_;
    };

private:
    MockRegistry() = default;
    mutable std::mutex mu_;
    bool enabled_{false};
    std::unordered_map<TypeInfo*, std::unordered_map<std::string, MockFn>> mocks_;
    std::unordered_map<TypeInfo*, std::unordered_map<std::string, std::vector<Rule>>> rules_;
    std::unordered_map<TypeInfo*, std::unordered_map<std::string, std::vector<CallRecord>>> calls_;
};

/// Free-function shortcuts mirroring the typical Mockit API.
inline void EnableMock()
{
    MockRegistry::instance().enable();
}
inline void DisableMock()
{
    MockRegistry::instance().disable();
}
inline void SetMockEnabled(bool on)
{
    MockRegistry::instance().set_enabled(on);
}
inline bool IsMockEnabled()
{
    return MockRegistry::instance().is_enabled();
}
inline void ClearMocks()
{
    MockRegistry::instance().clear();
}

template <typename T>
inline void StubMethod(std::string_view method_name, MockFn fn)
{
    MockRegistry::instance().stub<T>(method_name, std::move(fn));
}

template <typename T, typename R>
inline void StubReturn(std::string_view method_name, R value)
{
    MockRegistry::instance().stub_return<T>(method_name, std::move(value));
}

/// Install a stub that always rethrows @p eptr for method @p method_name of @c T.
template <typename T>
inline void StubThrow(std::string_view method_name, std::exception_ptr eptr)
{
    MockRegistry::instance().stub_throw<T>(method_name, eptr);
}

/// Install a stub that constructs @c E(args...) and throws it on every call.
template <typename T, typename E, typename... Args>
inline void StubThrow(std::string_view method_name, Args&&... args)
{
    MockRegistry::instance().template stub_throw<T, E>(method_name, std::forward<Args>(args)...);
}

/// Register an argument-conditional rule for method @p method_name of @c T.
template <typename T>
inline void WhenCalled(std::string_view method_name, ArgPredicate pred, Action act)
{
    MockRegistry::instance().add_rule<T>(method_name, std::move(pred), std::move(act));
}
}  // namespace Mock
#endif  // HSBA_ANY_OBJECT_ENABLE_MOCK

template <typename T>
struct member_fn_traits;
template <typename R, typename C, typename... Args>
struct member_fn_traits<R (C::*)(Args...)>
{
    using ReturnType = R;
    using ClassType = C;
    using ArgTypes = std::tuple<Args...>;
};
template <typename R, typename C, typename... Args>
struct member_fn_traits<R (C::*)(Args...) const>
{
    using ReturnType = R;
    using ClassType = C;
    using ArgTypes = std::tuple<Args...>;
};

template <auto ptr>
auto* type_ensure()
{
    using traits = member_fn_traits<decltype(ptr)>;
    using ClassType = typename traits::ClassType;
    using ResultType = typename traits::ReturnType;
    using ArgTypes = typename traits::ArgTypes;
    return +[](void* obj, std::span<AnyObject> args) -> AnyObject
    {
        auto self = static_cast<ClassType*>(obj);
        return [=]<std::size_t... Is>(std::index_sequence<Is...>)
        {
            if constexpr (std::is_void_v<ResultType>)
            {
                (self->*ptr)(args[Is].template cast<std::remove_cvref_t<std::tuple_element_t<Is, ArgTypes>>>()...);
                return AnyObject{};
            }
            else
            {
                return AnyObject{
                    (self->*ptr)(args[Is].template cast<std::remove_cvref_t<std::tuple_element_t<Is, ArgTypes>>>()...)};
            }
        }(std::make_index_sequence<std::tuple_size_v<ArgTypes>>{});
    };
}

/**
 * @brief Return type info for type T, this is a singleton per type T
 * @tparam T type
 * @return TypeInfo* pointer to the type info
 */
template <typename T>
TypeInfo* GetTypeInfo()
{
    static TypeInfo info;
    info.Name = typeid(T).name();
    info.destroy = [](void* data) { delete static_cast<T*>(data); };
    info.copy = [](const void* data) -> void* { return new T(*static_cast<const T*>(data)); };
    info.move = [](void* data) -> void* { return new T(std::move(*static_cast<T*>(data))); };
    return &info;
}

template <typename T, typename>
AnyObject::AnyObject(T&& value)
{
    using Decayed = std::remove_cvref_t<T>;
    type_info = GetTypeInfo<Decayed>();
    data = new Decayed(std::forward<T>(value));
    flag = 0b1;
}

template <>
inline TypeInfo* GetTypeInfo<std::string>()
{
    static TypeInfo info;
    info.Name = "std::string";
    info.destroy = [](void* data) { delete static_cast<std::string*>(data); };
    info.copy = [](const void* data) -> void* { return new std::string(*static_cast<const std::string*>(data)); };
    info.move = [](void* data) -> void* { return new std::string(std::move(*static_cast<std::string*>(data))); };
    info.methods["size"] = +[](void* obj, std::span<AnyObject>) -> AnyObject
    {
        auto str = static_cast<std::string*>(obj);
        return AnyObject{str->size()};
    };
    info.methods["c_str"] = +[](void* obj, std::span<AnyObject>) -> AnyObject
    {
        auto str = static_cast<std::string*>(obj);
        return AnyObject{str->c_str()};
    };
    info.methods["at"] = +[](void* obj, std::span<AnyObject> args) -> AnyObject
    {
        auto str = static_cast<std::string*>(obj);
        if (args.size() != 1 || !args[0].get_type_info() || args[0].get_type_info()->Name != "size_t")
        {
            throw RuntimeError("std::string::at expects one size_t argument");
        }
        size_t index = args[0].cast<size_t>();
        if (index >= str->size())
        {
            throw RuntimeError("std::string::at index out of range");
        }
        return AnyObject{(*str)[index]};
    };
    return &info;
}

template <>
inline TypeInfo* GetTypeInfo<std::string_view>()
{
    static TypeInfo info;
    info.Name = "std::string_view";
    info.destroy = [](void* data) { /* do nothing */ };
    info.copy = [](const void* data) -> void*
    { return new std::string_view(*static_cast<const std::string_view*>(data)); };
    info.move = [](void* data) -> void*
    { return new std::string_view(std::move(*static_cast<std::string_view*>(data))); };
    info.methods["size"] = +[](void* obj, std::span<AnyObject>) -> AnyObject
    {
        auto str = static_cast<std::string_view*>(obj);
        return AnyObject{str->size()};
    };
    info.methods["data"] = +[](void* obj, std::span<AnyObject>) -> AnyObject
    {
        auto str = static_cast<std::string_view*>(obj);
        return AnyObject{str->data()};
    };
    info.methods["at"] = +[](void* obj, std::span<AnyObject> args) -> AnyObject
    {
        auto str = static_cast<std::string_view*>(obj);
        if (args.size() != 1 || !args[0].get_type_info() || args[0].get_type_info()->Name != "size_t")
        {
            throw RuntimeError("std::string_view::at expects one size_t argument");
        }
        size_t index = args[0].cast<size_t>();
        if (index >= str->size())
        {
            throw RuntimeError("std::string_view::at index out of range");
        }
        return AnyObject{(*str)[index]};
    };
    return &info;
}

}  // namespace HsBa::Slicer::Utils

#endif  // HSBA_SLICER_ANY_OBJECT_HPP