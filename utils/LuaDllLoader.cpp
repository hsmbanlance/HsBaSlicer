#ifndef HSBA_NO_DLL_LOADER

#include "LuaDllLoader.hpp"

namespace HsBa::Slicer
{
    namespace
    {
        constexpr Utils::TemplateString DllLoaderTypeName = "DllLoader";
        int lua_dll_loader_new(lua_State* L)
        {
            const char* dll_path = luaL_checkstring(L, 1);
            NewLuaObject<DllLoader, DllLoaderTypeName>(L, dll_path);
            return 1;
        }

        int lua_dll_loader_gc(lua_State* L)
        {
            LuaGC<DllLoader, DllLoaderTypeName>(L);
            return 0;
        }

        int lua_dll_unload(lua_State* L)
        {
            auto* dll = (DllLoader*)lua_topointer(L, 1);
            if (!dll)
            {
                lua_pushstring(L, std::format("Invalid DllLoader object").c_str());
                lua_error(L);
                return 0;
            }
            dll->Unload();
            return 0;
        }

        int lua_dll_reload(lua_State* L)
        {
            auto* dll = (DllLoader*)lua_topointer(L, 1);
            const char* dll_path = luaL_checkstring(L, 2);
            if (!dll)
            {
                lua_pushstring(L, std::format("Invalid DllLoader object").c_str());
                lua_error(L);
                return 0;
            }
            dll->Reload(dll_path);
            return 0;
        }
    } // namespace

    void RegisterLuaDllLoader(lua_State* L,std::vector<std::unique_ptr<DllGetFunctionAbstract>>&& get_function_registers)
    {
        // Create a metatable for DllLoader
        luaL_newmetatable(L, static_cast<const char*>(DllLoaderTypeName));
        // Set __index to itself
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        // Set __gc
        lua_pushcfunction(L, lua_dll_loader_gc);
        lua_setfield(L, -2, "__gc");
        // Register methods
        // Register get_function and call_function
        for (const auto& reg : get_function_registers)
        {
            lua_pushcfunction(L, reg->GetLuaDllGetFuction());
            lua_setfield(L, -2, std::format("get_{}", reg->Name()).c_str());
            lua_pushcfunction(L, reg->GetLuaDllCallFuction());
            lua_setfield(L, -2, std::format("call_{}", reg->Name()).c_str());
        }
        // Register unload method
        lua_pushcfunction(L, lua_dll_unload);
        lua_setfield(L, -2, "unload");
        // Register reload method
        lua_pushcfunction(L, lua_dll_reload);
        lua_setfield(L, -2, "reload");
        // Pop the metatable
        lua_pop(L, 1);
        // Create Create global DllLoader table for class methods
        lua_newtable(L);
        // Register the constructor
        lua_pushcfunction(L, lua_dll_loader_new);
        lua_setfield(L, -2, "new");
        // Register methods to global table as well
        for(const auto& reg : get_function_registers)
        {
            lua_pushcfunction(L, reg->GetLuaDllGetFuction());
            lua_setfield(L, -2, std::format("get_{}", reg->Name()).c_str());
            lua_pushcfunction(L, reg->GetLuaDllCallFuction());
            lua_setfield(L, -2, std::format("call_{}", reg->Name()).c_str());
        }

        lua_pushcfunction(L,lua_dll_unload);
        lua_setfield(L, -2, "unload");
        lua_pushcfunction(L, lua_dll_reload);
        lua_setfield(L, -2, "reload");

        // Set the global DllLoader table
        lua_setglobal(L, static_cast<const char*>(DllLoaderTypeName));
        
    }
} // namespace HsBa::Slicer



#endif // !HSBA_NO_DLL_LOADER
