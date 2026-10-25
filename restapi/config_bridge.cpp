/** @file config_bridge.cpp
 * @brief Implements the reflection-driven config <-> JSON/XML codec; see config_bridge.hpp.
 * @author HsBa
 *
 * The flat C-ABI config structs are described by the param_reflect field table
 * (Utils::TypeInfo::fields maps a field name to (child TypeInfo*, byte offset)).
 * Every field value is converged through param_convert's FieldToAny (to double /
 * int64_t / std::string / nullptr) and emitted into a natural JSON object or XML
 * document via the rapidjson / tinyxml2 DOM APIs. Using the DOM writers (rather
 * than string concatenation) keeps the encoding correct for escaping/number
 * formatting; the reverse path rebuilds the converged std::any and lands it with
 * AnyToField. Response view types (errors, job status) still go through the
 * project's Utils serializers (see serialize_value.hpp).
 */
#include "config_bridge.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <tinyxml2.h>

#include "base/error.hpp"

namespace HsBa::Slicer::RestApi
{
namespace
{
using Utils::TypeInfo;

/** @brief Converged scalar category of a reflected field. */
enum class Kind
{
    CStr,         ///< const char*
    Double,       ///< float / double
    Int64,        ///< int / int64_t / enum
    Unsupported,  ///< anything else (rejected)
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
    if (!cfgTi)
    {
        throw InvalidArgumentError("config_bridge: null TypeInfo");
    }
    std::vector<FieldDesc> out;
    out.reserve(cfgTi->fields.size());
    for (const auto& [name, field] : cfgTi->fields)
    {
        out.push_back(FieldDesc{std::string(name), field.first, field.second});
    }
    std::sort(out.begin(), out.end(), [](const FieldDesc& a, const FieldDesc& b) { return a.name < b.name; });
    return out;
}

std::string JsonToString(const rapidjson::Document& doc, bool pretty)
{
    rapidjson::StringBuffer buffer;
    if (pretty)
    {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
    }
    else
    {
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
    }
    return buffer.GetString();
}

std::string XmlToString(const tinyxml2::XMLDocument& doc)
{
    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    return printer.CStr();
}

// ---------------------------- serialize ----------------------------
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
            throw RuntimeError("config_bridge: unexpected FieldToAny result for " + fd.name);
        }
    }
    out = JsonToString(doc, true);
}

void SerializeXml(TypeInfo* cfgTi, const void* cfg, std::string& out)
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* root = doc.NewElement("Config");
    doc.InsertFirstChild(root);

    for (const auto& fd : CollectFields(cfgTi))
    {
        const void* raw = static_cast<const char*>(cfg) + fd.offset;
        const std::any value = FieldToAny(fd.name, fd.child_ti, raw);
        if (!value.has_value() || value.type() == typeid(std::nullptr_t))
        {
            continue;  // nulls are omitted; the reverse path leaves the base value untouched
        }

        tinyxml2::XMLElement* el = doc.NewElement(fd.name.c_str());
        if (value.type() == typeid(double))
        {
            el->SetText(std::any_cast<double>(value));
        }
        else if (value.type() == typeid(int64_t))
        {
            el->SetText(static_cast<int64_t>(std::any_cast<int64_t>(value)));
        }
        else if (value.type() == typeid(std::string))
        {
            el->SetText(std::any_cast<std::string>(value).c_str());
        }
        else
        {
            throw RuntimeError("config_bridge: unexpected FieldToAny result for " + fd.name);
        }
        root->InsertEndChild(el);
    }
    out = XmlToString(doc);
}

// ---------------------------- deserialize ----------------------------
// Land a converged std::any into the config field described by fd.
void ApplyAny(const FieldDesc& fd, const std::any& value, void* cfg, StringArena& arena)
{
    void* raw = static_cast<char*>(cfg) + fd.offset;
    AnyToField(value, fd.child_ti, raw, arena);
}

void DeserializeJson(TypeInfo* cfgTi, const std::string& body, void* cfg, StringArena& arena)
{
    rapidjson::Document doc;
    doc.Parse(body.c_str());
    if (doc.HasParseError() || !doc.IsObject())
    {
        throw RuntimeError("config_bridge: invalid JSON body");
    }

    for (const auto& fd : CollectFields(cfgTi))
    {
        if (!doc.HasMember(fd.name.c_str()))
        {
            continue;
        }
        const rapidjson::Value& v = doc[fd.name.c_str()];
        switch (Classify(fd.child_ti))
        {
        case Kind::CStr:
            if (v.IsNull())
            {
                ApplyAny(fd, std::any(nullptr), cfg, arena);
            }
            else if (v.IsString())
            {
                ApplyAny(fd, std::any(std::string(v.GetString())), cfg, arena);
            }
            break;
        case Kind::Double:
            if (v.IsNumber())
            {
                ApplyAny(fd, std::any(v.GetDouble()), cfg, arena);
            }
            break;
        case Kind::Int64:
            if (v.IsInt64())
            {
                ApplyAny(fd, std::any(v.GetInt64()), cfg, arena);
            }
            else if (v.IsDouble())
            {
                ApplyAny(fd, std::any(static_cast<int64_t>(v.GetDouble())), cfg, arena);
            }
            break;
        default:
            break;  // unsupported fields are left at their base value
        }
    }
}

void DeserializeXml(TypeInfo* cfgTi, const std::string& body, void* cfg, StringArena& arena)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(body.c_str()) != tinyxml2::XML_SUCCESS)
    {
        throw RuntimeError("config_bridge: invalid XML body");
    }
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
    {
        throw RuntimeError("config_bridge: XML body has no root");
    }

    for (const auto& fd : CollectFields(cfgTi))
    {
        const tinyxml2::XMLElement* el = root->FirstChildElement(fd.name.c_str());
        if (!el)
        {
            continue;
        }
        switch (Classify(fd.child_ti))
        {
        case Kind::CStr:
            ApplyAny(fd, std::any(std::string(el->GetText() ? el->GetText() : "")), cfg, arena);
            break;
        case Kind::Double:
        {
            double d = 0.0;
            if (el->QueryDoubleText(&d) == tinyxml2::XML_SUCCESS)
            {
                ApplyAny(fd, std::any(d), cfg, arena);
            }
            break;
        }
        case Kind::Int64:
        {
            int64_t i = 0;
            double d = 0.0;
            if (el->QueryInt64Text(&i) == tinyxml2::XML_SUCCESS)
            {
                ApplyAny(fd, std::any(i), cfg, arena);
            }
            else if (el->QueryDoubleText(&d) == tinyxml2::XML_SUCCESS)
            {
                ApplyAny(fd, std::any(static_cast<int64_t>(d)), cfg, arena);
            }
            break;
        }
        default:
            break;
        }
    }
}
}  // namespace

std::string SerializeConfig(TypeInfo* cfgTi, const void* cfg, DataFormat fmt)
{
    if (!cfg)
    {
        throw InvalidArgumentError("config_bridge: null config");
    }
    std::string out;
    if (IsXml(fmt))
    {
        SerializeXml(cfgTi, cfg, out);
    }
    else
    {
        SerializeJson(cfgTi, cfg, out);
    }
    return out;
}

void DeserializeConfig(TypeInfo* cfgTi, const std::string& body, DataFormat fmt, void* cfg, StringArena& arena)
{
    if (!cfg)
    {
        throw InvalidArgumentError("config_bridge: null config");
    }
    if (IsXml(fmt))
    {
        DeserializeXml(cfgTi, body, cfg, arena);
    }
    else
    {
        DeserializeJson(cfgTi, body, cfg, arena);
    }
}

}  // namespace HsBa::Slicer::RestApi
