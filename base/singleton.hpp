#pragma once
#ifndef HSBA_SLICER_SINGLETON_HPP
#define HSBA_SLICER_SINGLETON_HPP

#include <shared_mutex>
#include <memory>

namespace HsBa::Slicer::Utils
{
	template <typename T>
	class Singleton
	{
	public:
		static T& GetInstance()
		{
			if (!instance_) {
				std::shared_lock<std::shared_mutex> read_lock(mutex_);
				if (!instance_) {
					read_lock.unlock();
					std::unique_lock<std::shared_mutex> write_lock(mutex_);
					if (!instance_) {
						instance_ = std::shared_ptr<T>(new T());
					}
				}
			}
			return *instance_;
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
	};

	template <typename T>
	std::shared_ptr<T> Singleton<T>::instance_ = nullptr;
	template <typename T>
	std::shared_mutex Singleton<T>::mutex_;
} // namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_SINGLETON_HPP