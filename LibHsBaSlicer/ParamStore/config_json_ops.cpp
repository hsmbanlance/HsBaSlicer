/** @file config_json_ops.cpp
 * @brief Implements the reflection-driven PipelineConfig <-> JSON codec; see config_json_ops.hpp.
 *
 * Mirrors the JSON path of restapi/config_bridge (walk TypeInfo::fields, converge each field
 * through FieldToAny/AnyToField, emit with the rapidjson DOM), but JSON-only and living in the
 * shared core so the C ABI and in-process GUI consumers reach it without the REST server.
 */
#include "config_json_ops.hpp"

#include <algorithm>
#include <any>
#include <cstddef>
#include <string>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "base/error.hpp"
#include "fileoperator/param_convert.hpp"
#include "fileoperator/param_reflect.hpp"

namespace HsBa::Slicer
{
namespace
{
using Utils::TypeInfo;

/** @brief Converged scalar category of a reflected field (same taxonomy as config_bridge). */
enum class Kind
{
    CStr,        ///< const char*
    Double,      ///< float / double
    Int64,       ///< int / int64_t / enum
    Unsupported, ///< anything else (rejected)
};

Kind Classify(TypeInfo* ti)
{
    if (ti == Utils::GetTypeInfo<const char*>())
    {
        return Kind::CStr;
    }
    if (ti == Utils::GetTypeInfo<float>() || ti == Utils::GetTypeInfo<double>())
    {
        return Kind::Double;
    }
    if (ti == Utils::GetTypeInfo<int>() || ti == Utils::GetTypeInfo<int64_t>() || IsEnumFieldType(ti))
    {
        return Kind::Int64;
    }
    return Kind::Unsupported;
}

/** @brief (name, child TypeInfo*, offset) triple, sorted by name for deterministic output. */
struct FieldDesc
{
    std::string name;
    TypeInfo* child_ti = nullptr;
    std::size_t offset = 0;
};

std::vector<FieldDesc> CollectFields(TypeInfo* cfgTi)
{
    std::vector<FieldDesc> out;
    out.reserve(cfgTi->fields.size());
    for (const auto& [name, field] : cfgTi->fields)
    {
        out.push_back(FieldDesc{std::string(name), field.first, field.second});
    }
    std::sort(out.begin(), out.end(), [](const FieldDesc& a, const FieldDesc& b) { return a.name < b.name; });
    return out;
}

// Lib kind -> fileoperator PipelineConfigTag (explicit switch, not relying on the integer order).
PipelineConfigTag ToTag(ParamPipelineKind kind)
{
    switch (kind)
    {
    case ParamPipelineKind::Fdm:
        return PipelineConfigTag::Fdm;
    case ParamPipelineKind::Sla:
        return PipelineConfigTag::Sla;
    case ParamPipelineKind::Sls:
        return PipelineConfigTag::Sls;
    case ParamPipelineKind::Slm:
        return PipelineConfigTag::Slm;
    case ParamPipelineKind::Lom:
        return PipelineConfigTag::Lom;
    case ParamPipelineKind::Tdp:
        return PipelineConfigTag::Tdp;
    case ParamPipelineKind::Waam:
        return PipelineConfigTag::Waam;
    case ParamPipelineKind::Custom:
        return PipelineConfigTag::Custom;
    case ParamPipelineKind::FileTransfer:
        return PipelineConfigTag::FileTransfer;
    }
    return PipelineConfigTag::Unknown;
}

// Ensure the reflection tables are populated and return the kind's TypeInfo*; nullptr when unknown.
TypeInfo* TiOf(ParamPipelineKind kind)
{
    RegisterPipelineConfigTypes();
    return TypeInfoOf(ToTag(kind));
}

void SerializeJson(TypeInfo* cfgTi, const void* cfg, std::string& out)
{
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    for (const auto& fd : CollectFields(cfgTi))
    {
        const void* raw = static_cast<const char*>(cfg) + fd.offset;
        const std::any value = FieldToAny(fd.name, fd.child_ti, raw);

        rapidjson::Value key(fd.name.c_str(), alloc);
        if (!value.has_value() || value.type() == typeid(std::nullptr_t))
        {
            doc.AddMember(key, rapidjson::Value(rapidjson::kNullType), alloc);
        }
        else if (value.type() == typeid(double))
        {
            doc.AddMember(key, rapidjson::Value(std::any_cast<double>(value)), alloc);
        }
        else if (value.type() == typeid(int64_t))
        {
            doc.AddMember(key, rapidjson::Value(static_cast<int64_t>(std::any_cast<int64_t>(value))), alloc);
        }
        else if (value.type() == typeid(std::string))
        {
            const std::string s = std::any_cast<std::string>(value);
            doc.AddMember(key, rapidjson::Value(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc), alloc);
        }
        else
        {
            throw RuntimeError("config_json: unexpected FieldToAny result for " + fd.name);
        }
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    out = buffer.GetString();
}

void DeserializeJson(TypeInfo* cfgTi, const std::string& body, void* cfg)
{
    rapidjson::Document doc;
    doc.Parse(body.c_str());
    if (doc.HasParseError() || !doc.IsObject())
    {
        throw RuntimeError("config_json: invalid JSON body");
    }

    StringArena arena;
    for (const auto& fd : CollectFields(cfgTi))
    {
        if (!doc.HasMember(fd.name.c_str()))
        {
            continue;
        }
        const rapidjson::Value& v = doc[fd.name.c_str()];
        void* raw = static_cast<char*>(cfg) + fd.offset;
        switch (Classify(fd.child_ti))
        {
        case Kind::CStr:
            if (v.IsNull())
            {
                AnyToField(std::any(nullptr), fd.child_ti, raw, arena);
            }
            else if (v.IsString())
            {
                AnyToField(std::any(std::string(v.GetString())), fd.child_ti, raw, arena);
            }
            break;
        case Kind::Double:
            if (v.IsNumber())
            {
                AnyToField(std::any(v.GetDouble()), fd.child_ti, raw, arena);
            }
            break;
        case Kind::Int64:
            if (v.IsInt64())
            {
                AnyToField(std::any(v.GetInt64()), fd.child_ti, raw, arena);
            }
            else if (v.IsDouble())
            {
                AnyToField(std::any(static_cast<int64_t>(v.GetDouble())), fd.child_ti, raw, arena);
            }
            break;
        default:
            break; // unsupported fields are left at their base value
        }
    }
    // The arena (new[]) dies with this scope; swap the landed const char* fields into malloc
    // ownership so they can be released across the C ABI (base fields start NULL by contract).
    ConfigStringsToOwning(cfgTi, cfg);
}

} // namespace

std::string ConfigToJson(ParamPipelineKind kind, const void* cfg)
{
    if (!cfg)
    {
        return {};
    }
    TypeInfo* ti = TiOf(kind);
    if (!ti)
    {
        return {};
    }
    std::string out;
    SerializeJson(ti, cfg, out);
    return out;
}

std::string DefaultConfigJson(ParamPipelineKind kind)
{
    TypeInfo* ti = TiOf(kind);
    if (!ti)
    {
        return {};
    }
    // Serialize a stack default built from the same factories the pipeline entry points use.
    switch (kind)
    {
    case ParamPipelineKind::Fdm:
    {
        auto cfg = HsBaFdmConfigDefault();
        return ConfigToJson(kind, &cfg);
    }
    case ParamPipelineKind::Sla:
    {
        auto cfg = HsBaSlaConfigDefault();
        return ConfigToJson(kind, &cfg);
    }
    case ParamPipelineKind::Sls:
    {
        auto cfg = HsBaSlsConfigDefault();
        return ConfigToJson(kind, &cfg);
    }
    case ParamPipelineKind::Slm:
    {
        auto cfg = HsBaSlmConfigDefault();
        return ConfigToJson(kind, &cfg);
    }
    case ParamPipelineKind::Lom:
    {
        auto cfg = HsBaLomConfigDefault();
        return ConfigToJson(kind, &cfg);
    }
    case ParamPipelineKind::Tdp:
    {
        auto cfg = HsBaTdpConfigDefault();
        return ConfigToJson(kind, &cfg);
    }
    case ParamPipelineKind::Waam:
    {
        auto cfg = HsBaWaamConfigDefault();
        return ConfigToJson(kind, &cfg);
    }
    case ParamPipelineKind::Custom:
    {
        auto cfg = HsBaCustomConfigDefault();
        return ConfigToJson(kind, &cfg);
    }
    case ParamPipelineKind::FileTransfer:
    {
        auto cfg = HsBaFileTransferConfigDefault();
        return ConfigToJson(kind, &cfg);
    }
    }
    return {};
}

bool ConfigFromJson(ParamPipelineKind kind, std::string_view json, void* cfg)
{
    if (!cfg || json.empty())
    {
        return false;
    }
    TypeInfo* ti = TiOf(kind);
    if (!ti)
    {
        return false;
    }
    try
    {
        DeserializeJson(ti, std::string(json), cfg);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

} // namespace HsBa::Slicer
