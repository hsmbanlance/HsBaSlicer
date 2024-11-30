#pragma once
#ifndef HSBA_SLICER_APP_CONFIG_HPP
#define HSBA_SLICER_APP_CONFIG_HPP

#include <mutex>
#include <shared_mutex>

namespace HsBa::Slicer
{
	class AppConfigSingletone
	{
		static AppConfigSingletone& GetInstance();
		static void DeleteInstance();
		std::string GetSevenZPath() const;
	private:
		static std::shared_mutex mutex_;
		static AppConfigSingletone* instance_;
		std::string sevenZ_path_;
		AppConfigSingletone();
	};
} // namespace HsBa::Slicer

#endif // !HSBA_SLICER_APP_CONFIG_HPP
