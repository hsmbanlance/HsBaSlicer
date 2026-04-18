#include "LuaAnyObject.hpp"

namespace HsBa::Slicer
{
    namespace
    {
        int lua_any_object_new(lua_State* L)
        {
            NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L);
            return 1;
        }

        int lua_any_object_invoke(lua_State* L)
        {
            auto *obj = (Utils::AnyObject*)lua_topointer(L, 1);
            std::string method_name = luaL_checkstring(L, 2);
            // table of arguments starts from index 3

            if(!obj)
            {
                lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
                lua_error(L);
                return 0;
            }

            // Collect arguments
            std::vector<Utils::AnyObject> args;
            int top = lua_gettop(L);
            for (int i = 3; i <= top; ++i) {
                if (lua_isinteger(L, i)) {
					auto integer_value = lua_tointeger(L, i);
                    args.emplace_back(integer_value);
                } 
                else if (lua_isnumber(L, i)) {
					auto number_value = lua_tonumber(L, i);
                    args.emplace_back(number_value);
                }
                else if (lua_isstring(L, i)) {
                    args.emplace_back(std::string(lua_tostring(L, i))); 
                } else if (lua_isboolean(L, i)) {
                    args.emplace_back((bool)lua_toboolean(L, i));
                } else {
					// For other types, push as AnyObject
					auto* subobj = (Utils::AnyObject*)lua_topointer(L, i);
					args.emplace_back(subobj ? *subobj : Utils::AnyObject());
                }
            }

            try {
                Utils::AnyObject result = obj->Invoke(method_name, args);
                // Push result back to Lua
                // For simplicity, assume result is a string or number
                auto* type_info = result.get_type_info();
                if (type_info) {
                    // push pointer
                    NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, std::move(result));
                } else {
                    lua_pushnil(L);
                }
                return 1;
            } catch (const std::exception& e) {
                lua_pushstring(L, e.what());
                lua_error(L);
                return 0;
            }
        }

        int lua_any_object_gc(lua_State* L)
        {
            LuaGC<Utils::AnyObject, AnyObjectTypeName>(L);
            return 0;
        }

        int lua_any_object_foreach_field(lua_State* L)
        {
            auto *obj = (Utils::AnyObject*)lua_topointer(L, 1);
            if (!obj) {
                lua_pushstring(L, "Invalid AnyObject object");
                lua_error(L);
                return 0;
            }
            if (!lua_isfunction(L, 2)) {
                lua_pushstring(L, "Second argument must be a function");
                lua_error(L);
                return 0;
            }

            // Push the function to the top
            lua_pushvalue(L, 2);

            obj->ForeachField([L](std::string_view name, Utils::AnyObject value) {
                // Call the Lua function with name and value
                lua_pushvalue(L, -1); // push function
                lua_pushlstring(L, name.data(), name.size()); // push name
                // Push value
                NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, std::move(value)); // push value as AnyObject
                lua_call(L, 2, 0); // call with 2 args, 0 results
            });

            lua_pop(L, 1); // pop the function
            return 0;
        }
    } // namespace
    
    void RegisterAnyObject(lua_State* L, const std::vector<LuaAnyObjectNewCastBase*>& added_types)
    {
        // Register the metatable for AnyObject
        luaL_newmetatable(L, static_cast<const char*>(AnyObjectTypeName));
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        
        // Set __gc
        lua_pushcfunction(L, lua_any_object_gc);
        lua_setfield(L, -2, "__gc");

        lua_pushcfunction(L, lua_any_object_invoke);
        lua_setfield(L, -2, "invoke");
        lua_pushcfunction(L, lua_any_object_foreach_field);
        lua_setfield(L, -2, "foreach_field");

        // Add cast methods to __index for instance methods
        for (auto* type : added_types) {
            auto cast_pair = type->GetCastFuncPair();
            lua_pushcfunction(L, cast_pair.func);
            lua_setfield(L, -2, cast_pair.name.data());
        }
        
        lua_pop(L, 1); // pop metatable
        
        // Create global AnyObject table for class methods
        lua_newtable(L);
        lua_pushcfunction(L, lua_any_object_new);
        lua_setfield(L, -2, "new");
        lua_pushcfunction(L, lua_any_object_invoke);
        lua_setfield(L, -2, "invoke");
        lua_pushcfunction(L, lua_any_object_foreach_field);
        lua_setfield(L, -2, "foreach_field");
        
        // Add newXX and castXX methods to AnyObject table
        for (auto* type : added_types) {
            auto new_pair = type->GetNewFuncPair();
            lua_pushcfunction(L, new_pair.func);
            lua_setfield(L, -2, new_pair.name.data());
            
            auto cast_pair = type->GetCastFuncPair();
            lua_pushcfunction(L, cast_pair.func);
            lua_setfield(L, -2, cast_pair.name.data());
        }
        
        lua_setglobal(L, "AnyObject");
    }

} // namespace HsBa::Slicer