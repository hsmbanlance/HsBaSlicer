/** @file singleton.hpp
 * @brief A header file containing the definition of the Singleton class template.
 * @author HsBa
 * @date 2024-06-01
 */
#pragma once
#ifndef HSBA_SLICER_SINGLETON_HPP
#define HSBA_SLICER_SINGLETON_HPP

#include <memory>
#include <mutex>
#include <shared_mutex>

namespace HsBa::Slicer::Utils
{
/**
 * @brief A thread-safe singleton class template that ensures only one instance of the class is created and provides
 * global access to that instance.
 * @tparam T The type of the singleton class.
 */
template <typename T>
class Singleton
{
protected:
    struct Protected
    {
    };

public:
    /** @brief Gets the singleton instance.
     * @tparam Args The argument types for the singleton constructor.
     * @param args The arguments for the singleton constructor.
     * @return A shared pointer to the singleton instance.
     */
    template <typename... Args>
    requires std::constructible_from<T, Protected, Args...> static std::shared_ptr<T> GetInstance(Args&&... args)
    {
        std::call_once(instance_flag_,
                       [&]() { instance_ = std::make_shared<T>(Protected{}, std::forward<Args>(args)...); });
        return instance_;
    }
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

protected:
    Singleton() = default;
    ~Singleton() = default;
    static std::shared_ptr<T> instance_;
    static std::shared_mutex mutex_;
    static std::once_flag instance_flag_;
};

template <typename T>
std::shared_ptr<T> Singleton<T>::instance_ = nullptr;
template <typename T>
std::shared_mutex Singleton<T>::mutex_;
template <typename T>
std::once_flag Singleton<T>::instance_flag_;
}  // namespace HsBa::Slicer::Utils

#endif  // !HSBA_SLICER_SINGLETON_HPP