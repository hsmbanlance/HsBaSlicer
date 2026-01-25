#pragma once

#ifndef HSBA_SLICER_IPATH_HPP
#define HSBA_SLICER_IPATH_HPP

#include <filesystem>
#include <string>
#include <string_view>
#include <functional>

// forward-declare lua state to avoid including lua.hpp in this core header
struct lua_State;

namespace HsBa::Slicer 
{
	class IPath
	{
	public:
		virtual ~IPath() = default;
		virtual void Save(const std::filesystem::path&) const = 0;
		virtual void Save(const std::filesystem::path&, std::string_view script,
			const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
		virtual void Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName,
			const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
		virtual void Save(const std::filesystem::path& path, const std::filesystem::path& script_file, std::string_view funcName,
			const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
		virtual std::string ToString() const = 0;
		virtual std::string ToString(const std::string_view script,
			const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
		virtual std::string ToString(const std::string_view script, const std::string_view funcName,
			const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
		virtual std::string ToString(const std::filesystem::path& script_file, const std::string_view funcName,
			const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
	};

	struct OutPoints3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};
} // namespace HsBa::Slicer

#endif // !HSBA_SLICER_IPATH_HPP