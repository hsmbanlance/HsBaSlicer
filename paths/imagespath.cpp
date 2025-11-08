#include "imagespath.hpp"

#include <sstream>

#include <lua.hpp>
#include "base/error.hpp"

#include "cipher/encoder.hpp"
#include "fileoperator/zipper.hpp"
#include "fileoperator/bit7z_zipper.hpp"

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
	
	void ImagesPath::Save(const std::filesystem::path& path)
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

	void ImagesPath::Save(const std::filesystem::path& path, std::string_view script)
	{
		// Determine compression mode from Lua script
		std::string compress_mode;
		lua_State* L = luaL_newstate();
		if (!L) throw RuntimeError("Lua init failed");
		luaL_openlibs(L);

		// expose config to script
		lua_pushstring(L, config_.configStr.c_str());
		lua_setglobal(L, "config");
		lua_pushstring(L, config_.path.c_str());
		lua_setglobal(L, "config_path");

		int loadStatus = luaL_loadbuffer(L, script.data(), script.size(), "ImagesPathScript");
		if (loadStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError(std::format("-- Lua load error: {}", err));
		}

		int callStatus = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError(std::format("-- Lua runtime error: {}", err));
		}

		// Priority: return value > global 'compress'
		int nret = lua_gettop(L);
		if (nret > 0 && lua_isstring(L, -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L, -1, &len);
			compress_mode.assign(s, len);
		}
		else
		{
			lua_getglobal(L, "compress");
			if (lua_isstring(L, -1))
			{
				size_t len = 0;
				const char* s = lua_tolstring(L, -1, &len);
				compress_mode.assign(s, len);
			}
		}

		lua_close(L);

		// choose zipper
		if (compress_mode == "Zip" || compress_mode.empty())
		{
			Zipper zipper;
			zipper += callback_;
			zipper.AddByteFile(config_.path, config_.configStr);
			for (const auto& [p, image] : images_)
			{
				zipper.AddByteFile(p, image);
			}
			zipper.Save(path.string());
			return;
		}

#ifdef USE_BIT7Z
		// use Bit7zZipper for non-Zip formats
		Bit7zZipper bz(std::string(HSBA_7Z_DLL), ZipperFormat::SevenZip, std::string{}
		);
		// attach callback via EventSource interface: add operator? reuse += as Zipper has
		bz += callback_;
		bz.AddByteFile(config_.path, config_.configStr);
		for (const auto& [p, image] : images_)
		{
			bz.AddByteFile(p, image);
		}
		bz.Save(path.string());
#else
		// fallback to Zipper if Bit7z not available
		Zipper zipper;
		zipper += callback_;
		zipper.AddByteFile(config_.path, config_.configStr);
		for (const auto& [p, image] : images_)
		{
			zipper.AddByteFile(p, image);
		}
		zipper.Save(path.string());
#endif
	}
	std::string ImagesPath::ToString()
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

	std::string ImagesPath::ToString(std::string_view script)
	{
		callback_(0.0, "save as string, no use callback");
		// default encoding: base64 (keep existing behavior: decode base64 to raw bytes)
		std::string encoding = "raw"; // raw means decode base64 and output bytes as before

		if (!script.empty())
		{
			lua_State* L = luaL_newstate();
			if (!L) throw RuntimeError("Lua init failed");
			luaL_openlibs(L);
			// expose config
			lua_pushstring(L, config_.configStr.c_str());
			lua_setglobal(L, "config");
			lua_pushstring(L, config_.path.c_str());
			lua_setglobal(L, "config_path");

			int loadStatus = luaL_loadbuffer(L, script.data(), script.size(), "ImagesPathToStringScript");
			if (loadStatus != LUA_OK)
			{
				std::string err = lua_tostring(L, -1);
				lua_close(L);
				throw RuntimeError(std::format("-- Lua load error: {}", err));
			}

			int callStatus = lua_pcall(L, 0, LUA_MULTRET, 0);
			if (callStatus != LUA_OK)
			{
				std::string err = lua_tostring(L, -1);
				lua_close(L);
				throw RuntimeError(std::format("-- Lua runtime error: {}", err));
			}

			int nret = lua_gettop(L);
			if (nret > 0 && lua_isstring(L, -1))
			{
				size_t len = 0;
				const char* s = lua_tolstring(L, -1, &len);
				encoding.assign(s, len);
			}
			else
			{
				lua_getglobal(L, "encoding");
				if (lua_isstring(L, -1))
				{
					size_t len = 0;
					const char* s = lua_tolstring(L, -1, &len);
					encoding.assign(s, len);
				}
			}

			lua_close(L);
		}

		std::ostringstream oss;
		oss << "#" << config_.path << "\n";
		oss << config_.configStr << "\n";
		for (const auto& [path, image] : images_)
		{
			oss << "#" << path << "\n";
			if (encoding == "base64")
			{
				oss << Cipher::Encoder::base64_encode(image) << "\n"; // original stored base64
			}
			else if (encoding == "hex")
			{
				oss << Cipher::Encoder::hex_encode(image) << "\n";
			}
			else // raw
			{
				oss << std::string(image.begin(), image.end()) << "\n";
			}
		}
		return oss.str();
	}
} // namespace HsBa::Slicer