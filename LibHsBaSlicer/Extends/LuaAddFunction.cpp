#include "LuaAddFunction.hpp"

#include <map>

namespace HsBa::Slicer
{
    std::vector<LuaRegFunc> g_2dFunctions;
    std::vector<LuaRegFunc> g_3dFunctions;
    std::vector<LuaRegFunc> g_fileFunctions;

    void Add2DFunctions(LuaRegFunc func)
    {
        g_2dFunctions.push_back(std::move(func));
    }

    void Add3DFunctions(LuaRegFunc func)
    {
        g_3dFunctions.push_back(std::move(func));
    }

    void AddFileFunctions(LuaRegFunc func)
    {
        g_fileFunctions.push_back(std::move(func));
    }

    std::vector<LuaRegFunc>& Get2DFunctions()
    {
        return g_2dFunctions;
    }

    std::vector<LuaRegFunc>& Get3DFunctions()
    {
        return g_3dFunctions;
    }

    std::vector<LuaRegFunc>& GetFileFunctions()
    {
        return g_fileFunctions;
    }

    // ===== Event callbacks =====

    static std::map<std::string, std::vector<LuaRegFunc>> g_eventCallbacks;

    void AddEventCallback(const std::string& event_name, LuaRegFunc func)
    {
        g_eventCallbacks[event_name].push_back(std::move(func));
    }

    std::vector<LuaRegFunc>& GetEventCallbacks(const std::string& event_name)
    {
        return g_eventCallbacks[event_name];
    }
}