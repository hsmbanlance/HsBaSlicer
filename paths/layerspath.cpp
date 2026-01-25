#include "layerspath.hpp"

#include <format>
#include <fstream>
#include <sstream>

#include <lua.hpp>

#include "base/error.hpp"
#include "fileoperator/sql_adapter.hpp"
#include "fileoperator/LuaAdapter.hpp"
#include "utils/LuaNewObject.hpp"

namespace HsBa::Slicer
{

    LayersPath::LayersPath(const std::function<void(std::string_view, std::string_view)>& callback)
        : callback_(callback)
    {
    }

    void LayersPath::push_back(const std::string& layerConfig, const PolygonsD& layer)
    {
        layers_.emplace_back(LayersData{layerConfig, layer});
    }

    void LayersPath::Save(const std::filesystem::path& path) const
    {
        SQL::SQLiteAdapter db;
        db.Connect(path.string());
        db += callback_;
        if(!db.IsConnected())
        {
            throw RuntimeError("Failed to create or open database file: " + path.string());
        }
        db | SQL::SQLCreateTable(
            "layers",
            {
                {"id", "INTEGER PRIMARY KEY AUTOINCREMENT"},
                {"layer_config", "TEXT NOT NULL"},
                {"layer_data", "BLOB NOT NULL"}
            });
        for (const auto& layerData : layers_)
        {
            std::ostringstream ss;
            ss << "{";
            for (const auto& polygon : layerData.layer)
            {
                ss << "[";
                for (const auto& point : polygon)
                {
                    ss << std::format("({},{}),", point.x, point.y);
                }
                ss.seekp(-1, ss.cur);
                ss << "],";
            }
            ss << "}";
            db | SQL::SQLInsert(
                "layers",
                {
                    {"layer_config", layerData.layerConfig},
                    {"layer_data", ss.str()}
                });
        }
    }

        void LayersPath::Save(const std::filesystem::path& path, std::string_view script,
        const std::function<void(lua_State*)>& lua_reg) const
    {
        // create lua state and register adapters
        auto L = MakeUniqueLuaState();
        if (!L) throw RuntimeError("Lua init failed");
        luaL_openlibs(L.get());
        RegisterLuaSQLiteAdapter(L.get());
    #ifdef HSBA_USE_MYSQL
        RegisterLuaMySQLAdapter(L.get());
    #endif
    #ifdef HSBA_USE_PGSQL
        RegisterLuaPostgreSQLAdapter(L.get());
    #endif
        if (lua_reg) lua_reg(L.get());

        // create SQLiteAdapter as a Lua userdata and expose as global 'db'
        SQL::SQLiteAdapter* db = NewLuaObject<SQL::SQLiteAdapter>(L.get(), "SQLiteAdapter");
        db->Connect(path.string());
        *db += callback_;
        lua_setglobal(L.get(), "db");


        // push layers as global similar to ToString
        lua_newtable(L.get());
        int idx = 1;
        for (const auto& layerData : layers_)
        {
            lua_newtable(L.get()); // layerData
            lua_pushstring(L.get(), layerData.layerConfig.c_str()); lua_setfield(L.get(), -2, "config");
            lua_newtable(L.get());
            int poly_idx = 1;
            for (const auto& poly : layerData.layer)
            {
                lua_newtable(L.get());
                int pt_idx = 1;
                for (const auto& pt : poly)
                {
                    lua_newtable(L.get());
                    lua_pushnumber(L.get(), pt.x); lua_setfield(L.get(), -2, "x");
                    lua_pushnumber(L.get(), pt.y); lua_setfield(L.get(), -2, "y");
                    lua_rawseti(L.get(), -2, pt_idx);
                    ++pt_idx;
                }
                lua_rawseti(L.get(), -2, poly_idx);
                ++poly_idx;
            }
            lua_setfield(L.get(), -2, "data");
            lua_rawseti(L.get(), -2, idx);
            ++idx;
        }
        lua_setglobal(L.get(), "layers");

        if (script.empty())
        {
            return;
        }

        // push path as global
        lua_pushstring(L.get(), path.string().c_str());
        lua_setglobal(L.get(), "output_path");

        int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "LayersPathSaveScript");
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
        // If Lua returned a string, write it to the given path
        {
            int nret = lua_gettop(L.get());
            if (nret > 0 && lua_isstring(L.get(), -1))
            {
                size_t len = 0;
                const char* s = lua_tolstring(L.get(), -1, &len);
                std::ofstream ofs(path, std::ios::binary);
                if (!ofs)
                {
                    throw RuntimeError("Failed to open output file: " + path.string());
                }
                ofs.write(s, static_cast<std::streamsize>(len));
                ofs.close();
                return;
            }
        }
        // lua_guard will close L when going out of scope
    }

        void LayersPath::Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName,
        const std::function<void(lua_State*)>& lua_reg) const
    {
        auto L = MakeUniqueLuaState();
        if (!L) throw RuntimeError("Lua init failed");
        luaL_openlibs(L.get());
        RegisterLuaSQLiteAdapter(L.get());
    #ifdef HSBA_USE_MYSQL
        RegisterLuaMySQLAdapter(L.get());
    #endif
    #ifdef HSBA_USE_PGSQL
        RegisterLuaPostgreSQLAdapter(L.get());
    #endif
        if (lua_reg) lua_reg(L.get());

        // create SQLiteAdapter as a Lua userdata and expose as global 'db'
        SQL::SQLiteAdapter* db = NewLuaObject<SQL::SQLiteAdapter>(L.get(), "SQLiteAdapter");
        db->Connect(path.string());
        *db += callback_;
        lua_setglobal(L.get(), "db");

        // push layers
        lua_newtable(L.get());
        int idx = 1;
        for (const auto& layerData : layers_)
        {
            lua_newtable(L.get()); // layerData
            lua_pushstring(L.get(), layerData.layerConfig.c_str()); lua_setfield(L.get(), -2, "config");
            lua_newtable(L.get());
            int poly_idx = 1;
            for (const auto& poly : layerData.layer)
            {
                lua_newtable(L.get());
                int pt_idx = 1;
                for (const auto& pt : poly)
                {
                    lua_newtable(L.get());
                    lua_pushnumber(L.get(), pt.x); lua_setfield(L.get(), -2, "x");
                    lua_pushnumber(L.get(), pt.y); lua_setfield(L.get(), -2, "y");
                    lua_rawseti(L.get(), -2, pt_idx);
                    ++pt_idx;
                }
                lua_rawseti(L.get(), -2, poly_idx);
                ++poly_idx;
            }
            lua_setfield(L.get(), -2, "data");
            lua_rawseti(L.get(), -2, idx);
            ++idx;
        }
        lua_setglobal(L.get(), "layers");

        // push path
        lua_pushstring(L.get(), path.string().c_str());
        lua_setglobal(L.get(), "output_path");

        // push function name
        lua_pushstring(L.get(), funcName.data());
        lua_setglobal(L.get(), "funcName");

        int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "LayersPathSaveScriptWithFunc");
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
        // If Lua returned a string, write it to the given path
        {
            int nret = lua_gettop(L.get());
            if (nret > 0 && lua_isstring(L.get(), -1))
            {
                size_t len = 0;
                const char* s = lua_tolstring(L.get(), -1, &len);
                std::ofstream ofs(path, std::ios::binary);
                if (!ofs)
                {
                    throw RuntimeError("Failed to open output file: " + path.string());
                }
                ofs.write(s, static_cast<std::streamsize>(len));
                ofs.close();
                return;
            }
        }
        // lua_guard will close L
    }

    void LayersPath::Save(const std::filesystem::path& path, const std::filesystem::path& script_file, std::string_view funcName,
        const std::function<void(lua_State*)>& lua_reg) const
    {
        std::ifstream ifs(script_file, std::ios::binary);
        if (!ifs)
        {
            throw RuntimeError("Failed to open Lua script file: " + script_file.string());
        }
        std::ostringstream oss;
        oss << ifs.rdbuf();
        std::string script = oss.str();
        Save(path, std::string_view{script}, funcName, lua_reg);
    }

    std::string LayersPath::ToString() const
    {
        std::ostringstream ss;
        ss << "{";
        for (const auto& layerData : layers_)
        {
            ss << "{config: " << layerData.layerConfig << ", data: ";
            ss << "{";
            for (const auto& polygon : layerData.layer)
            {
                ss << "[";
                for (const auto& point : polygon)
                {
                    ss << std::format("({},{}),", point.x, point.y);
                }
                ss.seekp(-1, ss.cur);
                ss << "],";
            }
            ss << "}},";
        }
        ss << "}";
        return ss.str();
    }

    std::string LayersPath::ToString(const std::string_view script,
        const std::function<void(lua_State*)>& lua_reg) const
    {
        if (script.empty()) return ToString();

        auto L = MakeUniqueLuaState();
        if (!L) throw RuntimeError("Lua init failed");
        luaL_openlibs(L.get());
        if (lua_reg) lua_reg(L.get());

        // push layers as global like Save
        lua_newtable(L.get());
        int idx = 1;
        for (const auto& layerData : layers_)
        {
            lua_newtable(L.get()); // layerData
            lua_pushstring(L.get(), layerData.layerConfig.c_str()); lua_setfield(L.get(), -2, "config");
            lua_newtable(L.get());
            int poly_idx = 1;
            for (const auto& poly : layerData.layer)
            {
                lua_newtable(L.get());
                int pt_idx = 1;
                for (const auto& pt : poly)
                {
                    lua_newtable(L.get());
                    lua_pushnumber(L.get(), pt.x); lua_setfield(L.get(), -2, "x");
                    lua_pushnumber(L.get(), pt.y); lua_setfield(L.get(), -2, "y");
                    lua_rawseti(L.get(), -2, pt_idx);
                    ++pt_idx;
                }
                lua_rawseti(L.get(), -2, poly_idx);
                ++poly_idx;
            }
            lua_setfield(L.get(), -2, "data");
            lua_rawseti(L.get(), -2, idx);
            ++idx;
        }
        lua_setglobal(L.get(), "layers");

        int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "LayersPathToStringScript");
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

        // Prefer returned string
        std::string body;
        int nret = lua_gettop(L.get());
        if (nret > 0 && lua_isstring(L.get(), -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L.get(), -1, &len);
            body.assign(s, len);
            return body;
        }

        lua_getglobal(L.get(), "result");
        if (lua_isstring(L.get(), -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L.get(), -1, &len);
            body.assign(s, len);
        }
        else
        {
            body = "";
        }

        return body;
    }

    std::string LayersPath::ToString(const std::string_view script, const std::string_view funcName,
        const std::function<void(lua_State*)>& lua_reg) const
    {
        auto L = MakeUniqueLuaState();
        if (!L) throw RuntimeError("Lua init failed");
        luaL_openlibs(L.get());
        if (lua_reg) lua_reg(L.get());
        // like without funcName, push layers
        lua_newtable(L.get());
        int idx = 1;
        for (const auto& layerData : layers_)
        {
            lua_newtable(L.get()); // layerData
            lua_pushstring(L.get(), layerData.layerConfig.c_str()); lua_setfield(L.get(), -2, "config");
            lua_newtable(L.get());
            int poly_idx = 1;
            for (const auto& poly : layerData.layer)
            {
                lua_newtable(L.get());
                int pt_idx = 1;
                for (const auto& pt : poly)
                {
                    lua_newtable(L.get());
                    lua_pushnumber(L.get(), pt.x); lua_setfield(L.get(), -2, "x");
                    lua_pushnumber(L.get(), pt.y); lua_setfield(L.get(), -2, "y");
                    lua_rawseti(L.get(), -2, pt_idx);
                    ++pt_idx;
                }
                lua_rawseti(L.get(), -2, poly_idx);
                ++poly_idx;
            }
            lua_setfield(L.get(), -2, "data");
            lua_rawseti(L.get(), -2, idx);
            ++idx;
        }
        lua_setglobal(L.get(), "layers");
        // push function name
        lua_pushstring(L.get(), funcName.data());
        lua_setglobal(L.get(), "funcName");
        int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "LayersPathToStringScriptWithFunc");
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
        // Prefer returned string
        std::string result;
        int nret = lua_gettop(L.get());
        if (nret > 0 && lua_isstring(L.get(), -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L.get(), -1, &len);
            result.assign(s, len);
            // L will be closed by UniqueLua deleter
            return result;
        }
        lua_getglobal(L.get(), "result");
        if (lua_isstring(L.get(), -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L.get(), -1, &len);
            result.assign(s, len);
        }
        else
        {
            result = "";
        }
        // L will be closed by UniqueLua deleter
        return result;
    }

    std::string LayersPath::ToString(const std::filesystem::path& script_file, const std::string_view funcName,
        const std::function<void(lua_State*)>& lua_reg) const
    {
        // load script from file
        std::ifstream ifs(script_file, std::ios::binary);
        if (!ifs)
        {
            throw RuntimeError("Failed to open Lua script file: " + script_file.string());
        }
        std::ostringstream oss;
        oss << ifs.rdbuf();
        std::string script = oss.str();
        return ToString(std::string_view{script}, funcName, lua_reg);
    }


} // namespace HsBa::Slicer