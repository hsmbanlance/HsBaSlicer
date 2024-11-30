#include "app_config.hpp"

namespace HsBa::Slicer
{
	std::shared_mutex AppConfigSingletone::mutex_{};
	AppConfigSingletone* AppConfigSingletone::instance_ = nullptr;

	AppConfigSingletone& AppConfigSingletone::GetInstance()
	{
		if (instance_)
		{
			return *instance_;
		}
		std::unique_lock lock{ mutex_ };
		if (!instance_)
		{
			instance_ = new AppConfigSingletone();
		}
		return *instance_;
	}

	void AppConfigSingletone::DeleteInstance()
	{
		std::unique_lock lock{ mutex_ };
		if (instance_)
		{
			delete instance_;
			instance_ = nullptr;
		}
	}

	std::string AppConfigSingletone::GetSevenZPath() const
	{
		std::shared_lock lock{ mutex_ };
		return sevenZ_path_;
	}
}