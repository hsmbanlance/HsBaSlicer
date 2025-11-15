#pragma once

#ifndef HSBA_SLICER_IPATH_HPP
#define HSBA_SLICER_IPATH_HPP

#include <filesystem>
#include <string>
#include <string_view>

namespace HsBa::Slicer 
{
	class IPath
	{
	public:
		virtual ~IPath() = default;
		virtual void Save(const std::filesystem::path&) = 0;
		virtual void Save(const std::filesystem::path&, std::string_view script) = 0;
		virtual std::string ToString() = 0;
		virtual std::string ToString(const std::string_view script) = 0;
	};

	struct OutPoints3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};
} // namespace HsBa::Slicer

#endif // !HSBA_SLICER_IPATH_HPP