#include "layerspath.hpp"

#include <format>

#include <lua.hpp>
#include <fstream>
#include <sstream>
#include "base/error.hpp"

#include "base/error.hpp"
#include "fileoperator/sql_adapter.hpp"


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

    void LayersPath::Save(const std::filesystem::path& path)
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

    void LayersPath::Save(const std::filesystem::path& path, std::string_view script)
    {
        // Expose layers to Lua and allow script to decide saving strategy.
        // Script may return a string (db_path) or set global 'db_path'.
        // If script returns boolean true or sets 'custom_saved' true, we assume script handled saving.

        if (script.empty())
        {
            // fallback to default
            Save(path);
            return;
        }

        lua_State* L = luaL_newstate();
        if (!L) throw RuntimeError("Lua init failed");
        luaL_openlibs(L);

        // push layers as Lua table
        lua_newtable(L);
        int idx = 1;
        for (const auto& layerData : layers_)
        {
            lua_newtable(L); // layerData
            lua_pushstring(L, layerData.layerConfig.c_str()); lua_setfield(L, -2, "config");
            // data as simple nested array of points
            lua_newtable(L);
            int poly_idx = 1;
            for (const auto& poly : layerData.layer)
            {
                lua_newtable(L); // polygon
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

        // push output path
        lua_pushstring(L, path.string().c_str());
        lua_setglobal(L, "output_path");

        int loadStatus = luaL_loadbuffer(L, script.data(), script.size(), "LayersPathScript");
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

    // Check Lua-provided flags
    bool custom_saved = false;
        bool use_db = false;
        std::string db_path;
        std::string table_name = "layers";
        std::string col_layer_config = "layer_config";
        std::string col_layer_data = "layer_data";
        std::string returned_string;

        int nret = lua_gettop(L);
        if (nret > 0)
        {
            if (lua_isboolean(L, -1))
            {
                // If script returns a boolean, treat true as custom_saved
                custom_saved = lua_toboolean(L, -1);
            }
            else if (lua_isstring(L, -1))
            {
                // capture returned string (could be formatted content or a db path)
                size_t len = 0;
                const char* s = lua_tolstring(L, -1, &len);
                returned_string.assign(s, len);
            }
        }

        // globals
        lua_getglobal(L, "use_db");
        if (lua_isboolean(L, -1)) use_db = lua_toboolean(L, -1);

        lua_getglobal(L, "db_path");
        if (lua_isstring(L, -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            db_path.assign(s, len);
        }

        lua_getglobal(L, "table_name");
        if (lua_isstring(L, -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            table_name.assign(s, len);
        }

        lua_getglobal(L, "col_layer_config");
        if (lua_isstring(L, -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            col_layer_config.assign(s, len);
        }

        lua_getglobal(L, "col_layer_data");
        if (lua_isstring(L, -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            col_layer_data.assign(s, len);
        }

        // if script signaled custom save, just return
        if (custom_saved)
        {
            Save(path);
            lua_close(L);
            return;
        }

        // Optional: rows table provided by script
        lua_getglobal(L, "rows");
        bool has_rows = lua_istable(L, -1);

        if (use_db || !db_path.empty())
        {
            if (db_path.empty()) db_path = path.string();
            SQL::SQLiteAdapter db;
            db.Connect(db_path);
            db += callback_;
            if (!db.IsConnected())
            {
                lua_close(L);
                throw RuntimeError("Failed to connect to database: " + db_path);
            }

            // create table with user-specified column names
            db | SQL::SQLCreateTable(
                table_name,
                {
                    {"id", "INTEGER PRIMARY KEY AUTOINCREMENT"},
                    {col_layer_config, "TEXT NOT NULL"},
                    {col_layer_data, "BLOB NOT NULL"}
                });

            if (has_rows)
            {
                // iterate rows table: expected array of {config=..., data=...}
                size_t len = lua_rawlen(L, -1);
                for (size_t i = 1; i <= len; ++i)
                {
                    lua_rawgeti(L, -1, static_cast<int>(i)); // push rows[i]
                    if (lua_istable(L, -1))
                    {
                        lua_getfield(L, -1, "config");
                        std::string cfg;
                        if (lua_isstring(L, -1)) { size_t l=0; const char* s = lua_tolstring(L, -1, &l); cfg.assign(s,l); }
                        lua_pop(L,1);
                        lua_getfield(L, -1, "data");
                        std::string data;
                        if (lua_isstring(L, -1)) { size_t l=0; const char* s = lua_tolstring(L, -1, &l); data.assign(s,l); }
                        lua_pop(L,1);

                        db | SQL::SQLInsert(table_name, {{col_layer_config, cfg}, {col_layer_data, data}});
                    }
                    lua_pop(L,1); // pop rows[i]
                }
            }
            else
            {
                // fallback: insert serialized layers
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
                    db | SQL::SQLInsert(table_name, {{col_layer_config, layerData.layerConfig}, {col_layer_data, ss.str()}});
                }
            }

            lua_close(L);
            return;
        }

        // use_db is false and no db_path specified: treat script as formatter and write its output
        std::string out;
        if (!returned_string.empty())
        {
            out = returned_string;
        }
        else
        {
            lua_getglobal(L, "result");
            if (lua_isstring(L, -1))
            {
                size_t len = 0;
                const char* s = lua_tolstring(L, -1, &len);
                out.assign(s, len);
            }
        }

        // write to file
        try
        {
            std::ofstream ofs(path, std::ios::binary);
            ofs << out;
        }
        catch (...) {}

        lua_close(L);
    }

    std::string LayersPath::ToString()
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

    std::string LayersPath::ToString(const std::string_view script)
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
} // namespace HsBa::Slicer