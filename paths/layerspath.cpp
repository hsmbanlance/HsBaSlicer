#include "layerspath.hpp"

#include <format>
#include <fstream>
#include <sstream>

#include <lua.hpp>

#include "base/error.hpp"
#include "fileoperator/sql_adapter.hpp"
#include "fileoperator/LuaAdapter.hpp"


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

    void LayersPath::Save(const std::filesystem::path& path, std::string_view script) const
    {
        // create lua state and register adapters
        lua_State* L = luaL_newstate();
        if (!L) throw RuntimeError("Lua init failed");
        // ensure lua state is closed on scope exit
        std::unique_ptr<lua_State, void(*)(lua_State*)> lua_guard(L, [](lua_State* p){ if(p) lua_close(p); });
        luaL_openlibs(L);
        HsBa::Slicer::RegisterLuaSQLiteAdapter(L);
    #ifdef USE_MYSQL
        HsBa::Slicer::RegisterLuaMySQLAdapter(L);
    #endif
    #ifdef USE_PGSQL
        HsBa::Slicer::RegisterLuaPostgreSQLAdapter(L);
    #endif

        // create SQLiteAdapter as a Lua userdata and expose as global 'db'
        SQL::SQLiteAdapter* db = HsBa::Slicer::NewLuaObject<SQL::SQLiteAdapter>(L, "SQLiteAdapter");
        db->Connect(path.string());
        *db += callback_;
        lua_setglobal(L, "db");


        // push layers as global similar to ToString
        lua_newtable(L);
        int idx = 1;
        for (const auto& layerData : layers_)
        {
            lua_newtable(L); // layerData
            lua_pushstring(L, layerData.layerConfig.c_str()); lua_setfield(L, -2, "config");
            lua_newtable(L);
            int poly_idx = 1;
            for (const auto& poly : layerData.layer)
            {
                lua_newtable(L);
                int pt_idx = 1;
                for (const auto& pt : poly)
                {
                    lua_newtable(L);
                    lua_pushnumber(L, pt.x); lua_setfield(L, -2, "x");
                    lua_pushnumber(L, pt.y); lua_setfield(L, -2, "y");
                    lua_rawseti(L, -2, pt_idx);
                    ++pt_idx;
                }
                lua_rawseti(L, -2, poly_idx);
                ++poly_idx;
            }
            lua_setfield(L, -2, "data");
            lua_rawseti(L, -2, idx);
            ++idx;
        }
        lua_setglobal(L, "layers");

        if (script.empty())
        {
            return;
        }

        // push path as global
        lua_pushstring(L, path.string().c_str());
        lua_setglobal(L, "output_path");

        int loadStatus = luaL_loadbuffer(L, script.data(), script.size(), "LayersPathSaveScript");
        if (loadStatus != LUA_OK)
        {
            std::string err = lua_tostring(L, -1);
            throw RuntimeError(std::format("-- Lua load error: {}", err));
        }

        int callStatus = lua_pcall(L, 0, LUA_MULTRET, 0);
        if (callStatus != LUA_OK)
        {
            std::string err = lua_tostring(L, -1);
            throw RuntimeError(std::format("-- Lua runtime error: {}", err));
        }
        // If Lua returned a string, write it to the given path
        {
            int nret = lua_gettop(L);
            if (nret > 0 && lua_isstring(L, -1))
            {
                size_t len = 0;
                const char* s = lua_tolstring(L, -1, &len);
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

    void LayersPath::Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName) const
    {
        lua_State* L = luaL_newstate();
        if (!L) throw RuntimeError("Lua init failed");
        std::unique_ptr<lua_State, void(*)(lua_State*)> lua_guard(L, [](lua_State* p){ if(p) lua_close(p); });
        luaL_openlibs(L);
        HsBa::Slicer::RegisterLuaSQLiteAdapter(L);
    #ifdef USE_MYSQL
        HsBa::Slicer::RegisterLuaMySQLAdapter(L);
    #endif
    #ifdef USE_PGSQL
        HsBa::Slicer::RegisterLuaPostgreSQLAdapter(L);
    #endif

        // create SQLiteAdapter as a Lua userdata and expose as global 'db'
        SQL::SQLiteAdapter* db = HsBa::Slicer::NewLuaObject<SQL::SQLiteAdapter>(L, "SQLiteAdapter");
        db->Connect(path.string());
        *db += callback_;
        lua_setglobal(L, "db");

        // push layers
        lua_newtable(L);
        int idx = 1;
        for (const auto& layerData : layers_)
        {
            lua_newtable(L); // layerData
            lua_pushstring(L, layerData.layerConfig.c_str()); lua_setfield(L, -2, "config");
            lua_newtable(L);
            int poly_idx = 1;
            for (const auto& poly : layerData.layer)
            {
                lua_newtable(L);
                int pt_idx = 1;
                for (const auto& pt : poly)
                {
                    lua_newtable(L);
                    lua_pushnumber(L, pt.x); lua_setfield(L, -2, "x");
                    lua_pushnumber(L, pt.y); lua_setfield(L, -2, "y");
                    lua_rawseti(L, -2, pt_idx);
                    ++pt_idx;
                }
                lua_rawseti(L, -2, poly_idx);
                ++poly_idx;
            }
            lua_setfield(L, -2, "data");
            lua_rawseti(L, -2, idx);
            ++idx;
        }
        lua_setglobal(L, "layers");

        // push path
        lua_pushstring(L, path.string().c_str());
        lua_setglobal(L, "output_path");

        // push function name
        lua_pushstring(L, funcName.data());
        lua_setglobal(L, "funcName");

        int loadStatus = luaL_loadbuffer(L, script.data(), script.size(), "LayersPathSaveScriptWithFunc");
        if (loadStatus != LUA_OK)
        {
            std::string err = lua_tostring(L, -1);
            throw RuntimeError(std::format("-- Lua load error: {}", err));
        }
        int callStatus = lua_pcall(L, 0, LUA_MULTRET, 0);
        if (callStatus != LUA_OK)
        {
            std::string err = lua_tostring(L, -1);
            throw RuntimeError(std::format("-- Lua runtime error: {}", err));
        }
        // If Lua returned a string, write it to the given path
        {
            int nret = lua_gettop(L);
            if (nret > 0 && lua_isstring(L, -1))
            {
                size_t len = 0;
                const char* s = lua_tolstring(L, -1, &len);
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

    void LayersPath::Save(const std::filesystem::path& path, const std::filesystem::path& script_file, std::string_view funcName) const
    {
        std::ifstream ifs(script_file, std::ios::binary);
        if (!ifs)
        {
            throw RuntimeError("Failed to open Lua script file: " + script_file.string());
        }
        std::ostringstream oss;
        oss << ifs.rdbuf();
        std::string script = oss.str();
        Save(path, std::string_view{script}, funcName);
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

    std::string LayersPath::ToString(const std::string_view script) const
    {
        if (script.empty()) return ToString();

        lua_State* L = luaL_newstate();
        if (!L) throw RuntimeError("Lua init failed");
        luaL_openlibs(L);

        // push layers as global like Save
        lua_newtable(L);
        int idx = 1;
        for (const auto& layerData : layers_)
        {
            lua_newtable(L); // layerData
            lua_pushstring(L, layerData.layerConfig.c_str()); lua_setfield(L, -2, "config");
            lua_newtable(L);
            int poly_idx = 1;
            for (const auto& poly : layerData.layer)
            {
                lua_newtable(L);
                int pt_idx = 1;
                for (const auto& pt : poly)
                {
                    lua_newtable(L);
                    lua_pushnumber(L, pt.x); lua_setfield(L, -2, "x");
                    lua_pushnumber(L, pt.y); lua_setfield(L, -2, "y");
                    lua_rawseti(L, -2, pt_idx);
                    ++pt_idx;
                }
                lua_rawseti(L, -2, poly_idx);
                ++poly_idx;
            }
            lua_setfield(L, -2, "data");
            lua_rawseti(L, -2, idx);
            ++idx;
        }
        lua_setglobal(L, "layers");

        int loadStatus = luaL_loadbuffer(L, script.data(), script.size(), "LayersPathToStringScript");
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

        // Prefer returned string
        std::string body;
        int nret = lua_gettop(L);
        if (nret > 0 && lua_isstring(L, -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            body.assign(s, len);
            lua_close(L);
            return body;
        }

        lua_getglobal(L, "result");
        if (lua_isstring(L, -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            body.assign(s, len);
        }
        else
        {
            body = "";
        }

        lua_close(L);
        return body;
    }

    std::string LayersPath::ToString(const std::string_view script, const std::string_view funcName) const
    {
        lua_State* L = luaL_newstate();
        if (!L) throw RuntimeError("Lua init failed");
        luaL_openlibs(L);
        // like without funcName, push layers
        lua_newtable(L);
        int idx = 1;
        for (const auto& layerData : layers_)
        {
            lua_newtable(L); // layerData
            lua_pushstring(L, layerData.layerConfig.c_str()); lua_setfield(L, -2, "config");
            lua_newtable(L);
            int poly_idx = 1;
            for (const auto& poly : layerData.layer)
            {
                lua_newtable(L);
                int pt_idx = 1;
                for (const auto& pt : poly)
                {
                    lua_newtable(L);
                    lua_pushnumber(L, pt.x); lua_setfield(L, -2, "x");
                    lua_pushnumber(L, pt.y); lua_setfield(L, -2, "y");
                    lua_rawseti(L, -2, pt_idx);
                    ++pt_idx;
                }
                lua_rawseti(L, -2, poly_idx);
                ++poly_idx;
            }
            lua_setfield(L, -2, "data");
            lua_rawseti(L, -2, idx);
            ++idx;
        }
        lua_setglobal(L, "layers");
        // push function name
        lua_pushstring(L, funcName.data());
        lua_setglobal(L, "funcName");
        int loadStatus = luaL_loadbuffer(L, script.data(), script.size(), "LayersPathToStringScriptWithFunc");
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
        // Prefer returned string
        std::string result;
        int nret = lua_gettop(L);
        if (nret > 0 && lua_isstring(L, -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            result.assign(s, len);
            lua_close(L);
            return result;
        }
        lua_getglobal(L, "result");
        if (lua_isstring(L, -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            result.assign(s, len);
        }
        else
        {
            result = "";
        }
        lua_close(L);
        return result;
    }

    std::string LayersPath::ToString(const std::filesystem::path& script_file, const std::string_view funcName) const
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
        return ToString(std::string_view{script}, funcName);
    }


} // namespace HsBa::Slicer