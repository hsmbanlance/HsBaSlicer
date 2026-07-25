#include "lua_register.h"

#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"

extern "C"
{
    HSBA_SLICER_API void HsBaAdd2DFunction(HsBaLuaRegFn func)
    {
        if (func)
            HsBa::Slicer::Add2DFunctions(func);
    }

    HSBA_SLICER_API void HsBaAdd3DFunction(HsBaLuaRegFn func)
    {
        if (func)
            HsBa::Slicer::Add3DFunctions(func);
    }

    HSBA_SLICER_API void HsBaAddFileFunction(HsBaLuaRegFn func)
    {
        if (func)
            HsBa::Slicer::AddFileFunctions(func);
    }

    HSBA_SLICER_API void HsBaAddEventCallback(const char* event_name, HsBaLuaRegFn func)
    {
        if (event_name && func)
            HsBa::Slicer::AddEventCallback(event_name, func);
    }
}
