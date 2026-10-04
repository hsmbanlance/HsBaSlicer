/** @file param_convert.cpp
 * @brief Implements the FieldToAny / AnyToField two-way type convergence; see param_convert.hpp.
 */
#include "param_convert.hpp"

#include <array>
#include <cstdlib>
#include <cstring>

#include "pipelinetypes/pipeline_types.h"
#include "sql_adapter.hpp"

namespace HsBa::Slicer
{
namespace
{
// TypeInfo* set of all supported C enum types; these fields are always stored as int64_t and read
// back as int. Under the C ABI these enums are int-compatible with value range [0,4], so
// reinterpreting them as int is well defined.
const std::array<Utils::TypeInfo*, 14>& EnumTypeInfos()
{
    static const std::array<Utils::TypeInfo*, 14> infos = {
        Utils::GetTypeInfo<HsBaFillMode_t>(),       Utils::GetTypeInfo<HsBaSupportPattern_t>(),
        Utils::GetTypeInfo<HsBaGCodeFirmware_t>(),  Utils::GetTypeInfo<HsBaSlaSupportPattern_t>(),
        Utils::GetTypeInfo<HsBaSlaImageType_t>(),   Utils::GetTypeInfo<HsBaSlmMaterial_t>(),
        Utils::GetTypeInfo<HsBaSlmLight_t>(),       Utils::GetTypeInfo<HsBaMetalProtectGas_t>(),
        Utils::GetTypeInfo<HsBaLomCutMode_t>(),     Utils::GetTypeInfo<HsBaTdpBinderMode_t>(),
        Utils::GetTypeInfo<HsBaWaamMaterial_t>(),   Utils::GetTypeInfo<HsBaWaamWeldProcess_t>(),
        Utils::GetTypeInfo<HsBaWaamProtection_t>(), Utils::GetTypeInfo<HsBaWaamRobotType_t>(),
    };
    return infos;
}

// Throw a uniform "unsupported field type" error naming the field and the conversion direction.
[[noreturn]] void ThrowUnsupported(std::string_view field_name, const char* dir)
{
    throw SQL::SQLAdapterInvalidArgumentError(std::string("ParamStore: unsupported field type for ") + dir + ": " +
                                              std::string(field_name));
}
}  // namespace

bool IsEnumFieldType(Utils::TypeInfo* field_ti) noexcept
{
    if (!field_ti)
        return false;
    for (auto* e : EnumTypeInfos())
    {
        if (e == field_ti)
            return true;
    }
    return false;
}

std::any FieldToAny(std::string_view field_name, Utils::TypeInfo* field_ti, const void* ptr)
{
    if (!field_ti || !ptr)
        ThrowUnsupported(field_name, "FieldToAny");

    if (field_ti == Utils::GetTypeInfo<float>())
        return std::any(static_cast<double>(*static_cast<const float*>(ptr)));
    if (field_ti == Utils::GetTypeInfo<double>())
        return std::any(*static_cast<const double*>(ptr));
    if (field_ti == Utils::GetTypeInfo<int>())
        return std::any(static_cast<int64_t>(*static_cast<const int*>(ptr)));
    if (field_ti == Utils::GetTypeInfo<int64_t>())
        return std::any(*static_cast<const int64_t*>(ptr));
    if (field_ti == Utils::GetTypeInfo<std::string>())
        return std::any(*static_cast<const std::string*>(ptr));
    if (field_ti == Utils::GetTypeInfo<const char*>())
    {
        const char* s = *static_cast<const char* const*>(ptr);
        return s ? std::any(std::string(s)) : std::any(nullptr);
    }
    if (IsEnumFieldType(field_ti))
        return std::any(static_cast<int64_t>(*static_cast<const int*>(ptr)));

    ThrowUnsupported(field_name, "FieldToAny");
}

void AnyToField(const std::any& value, Utils::TypeInfo* field_ti, void* ptr, StringArena& arena)
{
    if (!field_ti || !ptr)
        ThrowUnsupported("<load>", "AnyToField");

    // Aggregate NULL: an empty any or one holding nullptr is uniformly treated as NULL.
    const bool is_null = !value.has_value() || value.type() == typeid(std::nullptr_t);

    if (field_ti == Utils::GetTypeInfo<float>())
    {
        if (is_null)
        {
            *static_cast<float*>(ptr) = 0.0f;
            return;
        }
        if (value.type() != typeid(double))
            ThrowUnsupported("float", "AnyToField");
        *static_cast<float*>(ptr) = static_cast<float>(std::any_cast<double>(value));
        return;
    }
    if (field_ti == Utils::GetTypeInfo<double>())
    {
        if (is_null)
        {
            *static_cast<double*>(ptr) = 0.0;
            return;
        }
        if (value.type() != typeid(double))
            ThrowUnsupported("double", "AnyToField");
        *static_cast<double*>(ptr) = std::any_cast<double>(value);
        return;
    }
    if (field_ti == Utils::GetTypeInfo<int>())
    {
        if (is_null)
        {
            *static_cast<int*>(ptr) = 0;
            return;
        }
        if (value.type() != typeid(int64_t))
            ThrowUnsupported("int", "AnyToField");
        *static_cast<int*>(ptr) = static_cast<int>(std::any_cast<int64_t>(value));
        return;
    }
    if (field_ti == Utils::GetTypeInfo<int64_t>())
    {
        if (is_null)
        {
            *static_cast<int64_t*>(ptr) = 0;
            return;
        }
        if (value.type() != typeid(int64_t))
            ThrowUnsupported("int64_t", "AnyToField");
        *static_cast<int64_t*>(ptr) = std::any_cast<int64_t>(value);
        return;
    }
    if (field_ti == Utils::GetTypeInfo<std::string>())
    {
        if (is_null)
        {
            static_cast<std::string*>(ptr)->clear();
            return;
        }
        if (value.type() != typeid(std::string))
            ThrowUnsupported("std::string", "AnyToField");
        *static_cast<std::string*>(ptr) = std::any_cast<std::string>(value);
        return;
    }
    if (field_ti == Utils::GetTypeInfo<const char*>())
    {
        const char** dst = static_cast<const char**>(ptr);
        if (is_null)
        {
            *dst = nullptr;
            return;
        }
        if (value.type() != typeid(std::string))
            ThrowUnsupported("const char*", "AnyToField");
        const auto& s = std::any_cast<const std::string&>(value);
        auto buf = std::make_unique<char[]>(s.size() + 1);
        std::memcpy(buf.get(), s.c_str(), s.size() + 1);
        *dst = buf.get();
        arena.push_back(std::move(buf));
        return;
    }
    if (IsEnumFieldType(field_ti))
    {
        if (is_null)
        {
            *static_cast<int*>(ptr) = 0;
            return;
        }
        if (value.type() != typeid(int64_t))
            ThrowUnsupported("enum", "AnyToField");
        *static_cast<int*>(ptr) = static_cast<int>(std::any_cast<int64_t>(value));
        return;
    }

    ThrowUnsupported("<unknown>", "AnyToField");
}

void ConfigStringsToOwning(Utils::TypeInfo* cfgTi, void* cfg)
{
    if (!cfgTi || !cfg)
        return;
    Utils::AnyObject obj(cfgTi, cfg);
    obj.ForeachField(
        [](std::string_view, Utils::AnyObject child)
        {
            if (child.get_type_info() != Utils::GetTypeInfo<const char*>())
                return;
            const char** slot = static_cast<const char**>(child.get_data());
            const char* src = *slot;
            if (!src)
                return;
            const std::size_t n = std::strlen(src);
            char* dup = static_cast<char*>(std::malloc(n + 1));
            if (!dup)
                return;
            std::memcpy(dup, src, n + 1);
            *slot = dup;
        });
}

void FreeConfigStrings(Utils::TypeInfo* cfgTi, void* cfg)
{
    if (!cfgTi || !cfg)
        return;
    Utils::AnyObject obj(cfgTi, cfg);
    obj.ForeachField(
        [](std::string_view, Utils::AnyObject child)
        {
            if (child.get_type_info() != Utils::GetTypeInfo<const char*>())
                return;
            const char** slot = static_cast<const char**>(child.get_data());
            std::free(const_cast<char*>(*slot));
            *slot = nullptr;
        });
}

}  // namespace HsBa::Slicer
