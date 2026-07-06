/** @file delegate.hpp
 * @brief A collection of utilities for working with delegates.
 * This file provides a delegate class that can hold and invoke multiple callbacks with a specific signature.
 * @author HsBa
 * @date 2024-06
 */
#pragma once
#ifndef HSBA_SLICER_DELEGATE_HPP

#define HSBA_SLICER_DELEGATE_HPP

#include <algorithm>
#include <functional>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "concepts.hpp"

namespace HsBa::Slicer::Utils
{
/**
 * @brief A delegate is a type that represents references to methods with a specific parameter list and return type.
 * @tparam R return type of the delegate
 * @tparam ...Args parameter types of the delegate
 */
template <typename R, typename... Args>
class Delegate
{
public:
    using Callback = std::function<R(Args...)>;
    Delegate() = default;
    /** @brief Adds a callback to the delegate.
     * @tparam CallbackType The type of the callback.
     * @param callback The callback to add.
     */
    template <typename CallbackType>
    requires std::invocable<CallbackType, Args...> void Add(CallbackType&& callback)
    {
        std::lock_guard lock(mutex_);
        callbacks_.emplace_back(std::forward<CallbackType>(callback));
    }
    /** @brief Removes a callback from the delegate.
     * @param callback The callback to remove.
     */
    [[deprecated("std::functional has not operator== function")]]
    void Remove(const Callback& callback)
    {
        std::lock_guard lock(mutex_);
        auto it = std::remove_if(callbacks_.begin(), callbacks_.end(), [&callback](const Callback& cb)
                                 { return cb.target_type() == callback.target_type(); });
        if (it != callbacks_.end())
        {
            callbacks_.erase(it, callbacks_.end());
        }
    }
    /** @brief Invokes all callbacks in the delegate.
     * @param args The arguments to pass to each callback.
     * @return The result of the invocation.
     */
    R Invoke(Args&&... args)
    {
        std::vector<std::function<R(Args...)>> callbacks;
        {
            std::lock_guard lock(mutex_);
            callbacks = callbacks_;
        }
        if constexpr (std::is_void_v<R>)
        {
            for (const auto& callback : callbacks)
            {
                callback(std::forward<Args>(args)...);
            }
            return;
        }
        else if constexpr (Addable<R>)
        {
            R result{};
            for (const auto& callback : callbacks)
            {
                result = result + callback(std::forward<Args>(args)...);
            }
            return result;
        }
        else
        {
            R result{};
            for (const auto& callback : callbacks)
            {
                result = callback(std::forward<Args>(args)...);
            }
            return result;
        }
    }
    /** @brief Checks if the delegate has any callbacks.
     * @return true if the delegate has no callbacks, false otherwise.
     */
    bool empty() const
    {
        std::shared_lock lock(mutex_);
        return callbacks_.empty();
    }
    /** @brief Clears all callbacks from the delegate. */
    void Clear()
    {
        std::lock_guard lock(mutex_);
        callbacks_.clear();
    }
    /** @brief Returns the number of callbacks in the delegate.
     * @return The number of callbacks in the delegate.
     */
    size_t size() const
    {
        std::shared_lock lock(mutex_);
        return callbacks_.size();
    }
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    Delegate(Delegate&&) = default;
    Delegate& operator=(Delegate&&) = default;
    /** @brief Adds a callback to the delegate using the += operator.
     * @tparam CallbackType The type of the callback.
     * @param callback The callback to add.
     * @return A reference to the delegate.
     */
    template <typename CallbackType>
    requires std::invocable<CallbackType, Args...> Delegate& operator+=(CallbackType&& callback)
    {
        Add(callback);
        return *this;
    }
    /** @brief Removes a callback from the delegate using the -= operator.
     * @param callback The callback to remove.
     * @return A reference to the delegate.
     */
    [[deprecated]]
    Delegate& operator-=(const Callback& callback)
    {
        Remove(callback);
        return *this;
    }
    /** @brief Invokes the delegate with the given arguments.
     * @param args The arguments to pass to each callback.
     * @return The result of the invocation.
     */
    R operator()(Args&&... args) { return Invoke(std::forward<Args>(args)...); }
    /** @brief Returns an iterator to the beginning of the callbacks in the delegate.
     * @return An iterator to the beginning of the callbacks in the delegate.
     */
    auto begin() const
    {
        std::shared_lock lock(mutex_);
        return callbacks_.begin();
    }
    /** @brief Returns an iterator to the end of the callbacks in the delegate.
     * @return An iterator to the end of the callbacks in the delegate.
     */
    auto end() const
    {
        std::shared_lock lock(mutex_);
        return callbacks_.end();
    }

private:
    mutable std::shared_mutex mutex_;
    std::vector<Callback> callbacks_;
};

/** @brief A class representing an event that can be raised and handled by multiple listeners.
 * @tparam R The return type of the event.
 * @tparam Args The argument types of the event.
 */
template <typename R, typename... Args>
requires(sizeof...(Args) > 0) class Event final
{
public:
    using DelegateType = Delegate<R, Args...>;
    using Callback = typename DelegateType::Callback;
    template <typename T, typename, typename...>
    friend class EventSource;

private:
    Event() = default;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&&) = default;
    Event& operator=(Event&&) = default;
    DelegateType delegate_;
    R Invoke(Args... args) { return delegate_.Invoke(std::forward<Args>(args)...); }
};

/** @brief A class representing a source of events that can be raised and handled by multiple listeners.
 * @tparam Derived The derived class type.
 * @tparam R The return type of the event.
 * @tparam Args The argument types of the event.
 */
template <typename Derived, typename R, typename... Args>
class EventSource
{
public:
    /** @brief Adds a callback to the event source.
     * @tparam CallbackType The type of the callback.
     * @param callback The callback to add.
     */
    template <typename CallbackType>
    requires std::invocable<CallbackType, Args...> void Add(CallbackType&& callback)
    {
        event_.delegate_.Add(std::forward<CallbackType>(callback));
    }
    /** @brief Removes a callback from the event source.
     * @param callback The callback to remove.
     */
    [[deprecated("std::functional has not operator== function")]]
    void Remove(const typename Event<R, Args...>::Callback& callback)
    {
        event_.delegate_.Remove(callback);
    }
    /** @brief Adds a callback to the event source using the += operator.
     * @tparam CallbackType The type of the callback.
     * @param callback The callback to add.
     * @return A reference to the event source.
     */
    template <typename CallbackType>
    requires std::invocable<CallbackType, Args...> void operator+=(CallbackType&& callback)
    {
        Add(callback);
    }
    /** @brief Removes a callback from the event source using the -= operator.
     * @param callback The callback to remove.
     * @return A reference to the event source.
     */
    [[deprecated]]
    void operator-=(const typename Event<R, Args...>::Callback& callback)
    {
        Remove(callback);
    }

protected:
    EventSource() = default;
    ~EventSource() = default;
    Event<R, Args...> event_;
    R RaiseEvent(Args... args) { return event_.Invoke(args...); }
};
}  // namespace HsBa::Slicer::Utils

#endif  // !HSBA_SLICER_DELEGATE_HPP
