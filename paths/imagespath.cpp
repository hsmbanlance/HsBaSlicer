#include "imagespath.hpp"

#include <sstream>
#include <fstream>
#include <format>

#include <lua.hpp>
#include "utils/LuaNewObject.hpp"

#include "base/error.hpp"
#include "cipher/encoder.hpp"
#include "fileoperator/zipper.hpp"
#include "fileoperator/bit7z_zipper.hpp"
#include "cipher/LuaAdapter.hpp"
#include "fileoperator/LuaAdapter.hpp"

namespace HsBa::Slicer
{
	ImagesPath::ImagesPath(std::string_view config_file, std::string_view config_str,const std::function<void(double, std::string_view)>& callback)
		: config_{ std::string{config_file},std::string{config_str} },
		images_{}, callback_{ callback }
	{
	}

	void ImagesPath::AddImage(std::string_view path, std::string_view image_str)
	{
		images_.emplace(std::string{path}, std::string{image_str});
	}
	
	void ImagesPath::Save(const std::filesystem::path& path) const
	{
		Zipper zipper;
		zipper += callback_;
		zipper.AddByteFile(config_.path, config_.configStr);
		for (const auto& [path, image] : images_)
		{
			zipper.AddByteFile(path, image);
		}
		zipper.Save(path.string());
	}

	void ImagesPath::Save(const std::filesystem::path& path, std::string_view script) const
	{
		auto L = MakeUniqueLuaState();
		if (!L) throw RuntimeError("Lua init failed");
		luaL_openlibs(L.get());
		// register helpers
		RegisterLuaZipper(L.get());
		Cipher::RegisterLuaCipher(L.get());
#ifdef HSBA_USE_BIT7Z
		RegisterLuaBit7zZipper(L.get());
#endif

		// push config global
		lua_newtable(L.get());
		lua_pushstring(L.get(), config_.path.c_str()); lua_setfield(L.get(), -2, "path");
		lua_pushstring(L.get(), config_.configStr.c_str()); lua_setfield(L.get(), -2, "configStr");
		lua_setglobal(L.get(), "config");

		// push images table
		lua_newtable(L.get());
		int idx = 1;
		for (const auto& [p, img] : images_)
		{
			lua_newtable(L.get());
			lua_pushstring(L.get(), p.c_str()); lua_setfield(L.get(), -2, "path");
			lua_pushlstring(L.get(), img.data(), img.size()); lua_setfield(L.get(), -2, "data");
			lua_rawseti(L.get(), -2, idx);
			++idx;
		}
		lua_setglobal(L.get(), "images");

		// push output path
		lua_pushstring(L.get(), path.string().c_str());
		lua_setglobal(L.get(), "output_path");

		if (script.empty()) return;

		int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "ImagesPathSaveScript");
		if (loadStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua load error: {}", err));
		}
		int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua runtime error: {}", err));
		}
		// if returned string, write to output_path
		int nret = lua_gettop(L.get());
		if (nret > 0 && lua_isstring(L.get(), -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L.get(), -1, &len);
			std::ofstream ofs(path, std::ios::binary);
			if (!ofs) throw RuntimeError("Failed to open output file: " + path.string());
			ofs.write(s, static_cast<std::streamsize>(len));
		}
	}

	void ImagesPath::Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName) const
	{
		std::string script_copy(script);
		// set funcName global and call same
		auto L = MakeUniqueLuaState();
		if (!L) throw RuntimeError("Lua init failed");
		luaL_openlibs(L.get());
		RegisterLuaZipper(L.get());
		Cipher::RegisterLuaCipher(L.get());
#ifdef HSBA_USE_BIT7Z
		RegisterLuaBit7zZipper(L.get());
#endif

		// push config
		lua_newtable(L.get());
		lua_pushstring(L.get(), config_.path.c_str()); lua_setfield(L.get(), -2, "path");
		lua_pushstring(L.get(), config_.configStr.c_str()); lua_setfield(L.get(), -2, "configStr");
		lua_setglobal(L.get(), "config");
		// push images
		lua_newtable(L.get());
		int idx = 1;
		for (const auto& [p, img] : images_)
		{
			lua_newtable(L.get());
			lua_pushstring(L.get(), p.c_str()); lua_setfield(L.get(), -2, "path");
			lua_pushlstring(L.get(), img.data(), img.size()); lua_setfield(L.get(), -2, "data");
			lua_rawseti(L.get(), -2, idx);
			++idx;
		}
		lua_setglobal(L.get(), "images");
		// push path and funcName
		lua_pushstring(L.get(), path.string().c_str());
		lua_setglobal(L.get(), "output_path");

		int loadStatus = luaL_loadbuffer(L.get(), script_copy.data(), script_copy.size(), "ImagesPathSaveScriptWithFunc");
		if (loadStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua load error: {}", err));
		}
		int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua runtime error: {}", err));
		}
		int nret = lua_gettop(L.get());
		if (nret > 0 && lua_isstring(L.get(), -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L.get(), -1, &len);
			std::ofstream ofs(path, std::ios::binary);
			if (!ofs) throw RuntimeError("Failed to open output file: " + path.string());
			ofs.write(s, static_cast<std::streamsize>(len));
		}
	}

	void ImagesPath::Save(const std::filesystem::path& path, const std::filesystem::path& script_file, std::string_view funcName) const
	{
		std::ifstream ifs(script_file, std::ios::binary);
		if (!ifs) throw RuntimeError("Failed to open Lua script file: " + script_file.string());
		std::ostringstream oss; oss << ifs.rdbuf(); std::string script = oss.str();
		Save(path, std::string_view{script}, funcName);
	}

	std::string ImagesPath::ToString(std::string_view script) const
	{
		if (script.empty()) return ToString();

		auto L = MakeUniqueLuaState();
		if (!L) throw RuntimeError("Lua init failed");
		luaL_openlibs(L.get());
		// register helpers
		Cipher::RegisterLuaCipher(L.get());

		// push config global
		lua_newtable(L.get());
		lua_pushstring(L.get(), config_.path.c_str()); lua_setfield(L.get(), -2, "path");
		lua_pushstring(L.get(), config_.configStr.c_str()); lua_setfield(L.get(), -2, "configStr");
		lua_setglobal(L.get(), "config");

		// push images table
		lua_newtable(L.get());
		int idx = 1;
		for (const auto& [p, img] : images_)
		{
			lua_newtable(L.get());
			lua_pushstring(L.get(), p.c_str()); lua_setfield(L.get(), -2, "path");
			lua_pushlstring(L.get(), img.data(), img.size()); lua_setfield(L.get(), -2, "data");
			lua_rawseti(L.get(), -2, idx);
			++idx;
		}
		lua_setglobal(L.get(), "images");

		int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "ImagesPathToStringScript");
		if (loadStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua load error: {}", err));
		}
		int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua runtime error: {}", err));
		}
		int nret = lua_gettop(L.get());
		if (nret > 0 && lua_isstring(L.get(), -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L.get(), -1, &len);
			return std::string(s, len);
		}
		return ToString();
	}

	std::string ImagesPath::ToString(const std::string_view script, const std::string_view funcName) const
	{
		std::string script_copy(script);
		if (script_copy.empty()) return ToString();

		auto L = MakeUniqueLuaState();
		if (!L) throw RuntimeError("Lua init failed");
		luaL_openlibs(L.get());
		Cipher::RegisterLuaCipher(L.get());
		// push config
		lua_newtable(L.get());
		lua_pushstring(L.get(), config_.path.c_str()); lua_setfield(L.get(), -2, "path");
		lua_pushstring(L.get(), config_.configStr.c_str()); lua_setfield(L.get(), -2, "configStr");
		lua_setglobal(L.get(), "config");
		// push images
		lua_newtable(L.get());
		int idx = 1;
		for (const auto& [p, img] : images_)
		{
			lua_newtable(L.get());
			lua_pushstring(L.get(), p.c_str()); lua_setfield(L.get(), -2, "path");
			lua_pushlstring(L.get(), img.data(), img.size()); lua_setfield(L.get(), -2, "data");
			lua_rawseti(L.get(), -2, idx);
			++idx;
		}
		lua_setglobal(L.get(), "images");
		// push funcName
		lua_pushstring(L.get(), funcName.data()); lua_setglobal(L.get(), "funcName");

		int loadStatus = luaL_loadbuffer(L.get(), script_copy.data(), script_copy.size(), "ImagesPathToStringScriptWithFunc");
		if (loadStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua load error: {}", err));
		}
		int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua runtime error: {}", err));
		}
		int nret = lua_gettop(L.get());
		if (nret > 0 && lua_isstring(L.get(), -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L.get(), -1, &len);
			return std::string(s, len);
		}
		return ToString();
	}

	std::string ImagesPath::ToString(const std::filesystem::path& script_file, const std::string_view funcName) const
	{
		std::ifstream ifs(script_file, std::ios::binary);
		if (!ifs) throw RuntimeError("Failed to open Lua script file: " + script_file.string());
		std::ostringstream oss; oss << ifs.rdbuf(); std::string script = oss.str();
		return ToString(std::string_view{script}, funcName);
	}

	std::string ImagesPath::ToString() const
	{
		callback_(0.0, "save as string, no use callback");
		std::ostringstream oss;
		oss << "#" << config_.path << "\n";
		oss << config_.configStr << "\n";
		for (const auto& [path, image] : images_)
		{
			oss << "#" << path << "\n";
			oss << Cipher::Encoder::base64_decode_to_string(image) << "\n";
		}
		return oss.str();
	}

	
} // namespace HsBa::Slicer