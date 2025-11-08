#pragma once
#ifndef HSBA_SLICER_IMAGES_PATH_HPP
#define HSBA_SLICER_IMAGES_PATH_HPP

#include <unordered_map>
#include <functional>

#include "IPath.hpp"

namespace HsBa::Slicer
{
	class ImagesPath : public IPath
	{
	public:
		ImagesPath(std::string_view config_file, std::string_view config_str,
			const std::function<void(double,std::string_view)>& callback = [](double,std::string_view){});
		virtual ~ImagesPath() = default;
		virtual void Save(const std::filesystem::path&) override;
		virtual void Save(const std::filesystem::path&, std::string_view script) override;
		virtual std::string ToString() override;
		virtual std::string ToString(std::string_view script) override;
		void AddImage(std::string_view path, std::string_view image_str);
	private:
		struct ConfigFile {
			std::string path;
			std::string configStr;
		};
		ConfigFile config_;
		std::unordered_map<std::string,std::string> images_;
		std::function<void(double, std::string_view)> callback_;
	};
} // namepace HsBa::Slicer

#endif // !HSBA_SLICER_IMAGES_PATH_HPP