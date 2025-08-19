#pragma once
#ifndef HSBA_SLICER_SINGLETON_HPP
#define HSBA_SLICER_SINGLETON_HPP

#include <shared_mutex>
#include <mutex>
#include <memory>

namespace HsBa::Slicer::Utils
{
	template <typename T>
	class Singleton
	{
	public:
		static std::shared_ptr<T> GetInstance()
		{
			std::call_once(instance_flag_, []() {
				instance_ = std::shared_ptr<T>(new T());
				});
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
} // namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_SINGLETON_HPP