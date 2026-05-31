#pragma once
#ifndef HSBA_SLICER_STRUCT_XML_HPP
#define HSBA_SLICER_STRUCT_XML_HPP

#include <limits>
#include <memory>
#include <ranges>
#include <sstream>

#include <boost/pfr.hpp>
#include <boost/pfr/core_name.hpp>

#include <tinyxml2.h>

#include "base/concepts.hpp"
#include "base/error.hpp"
#include "base/static_reflect.hpp"
#include "base/template_helper.hpp"

#include "struct_concepts.hpp"

namespace HsBa::Slicer::Utils
{

template <typename T>
concept TinyXmlConvertible = requires(const T& value, tinyxml2::XMLElement* element, tinyxml2::XMLDocument* doc)
{
    {
        value.to_xml(element, doc)
    } -> std::same_as<void>;
    {
        T::from_xml(element)
    } -> std::same_as<T>;
};

namespace detail
{
// Forward declarations for recursive / dependent calls
template <Aggregte T>
requires(!Reflectable<T>) void to_xml_impl(const T& value, tinyxml2::XMLElement* element, tinyxml2::XMLDocument* doc);

template <Aggregte T>
requires(!Reflectable<T>) void from_xml_impl(const tinyxml2::XMLElement* element, T& value);

// Forward decls for reflectable types
template <Reflectable T>
void to_xml_impl(const T& value, tinyxml2::XMLElement* element, tinyxml2::XMLDocument* doc);

template <Reflectable T>
void from_xml_impl(const tinyxml2::XMLElement* element, T& value);

template <typename FieldType>
void add_element(tinyxml2::XMLElement* parent, std::string_view name, const FieldType& field,
                 tinyxml2::XMLDocument* doc);

// Helper: expand reflectable fields into add_element calls
template <typename FieldType, std::size_t... Is>
void add_reflectable_elements_impl(tinyxml2::XMLElement* parent, const FieldType& field, tinyxml2::XMLDocument* doc,
                                   std::index_sequence<Is...>)
{
    if constexpr (sizeof...(Is) > 0)
    {
        int dummy[] = {((add_element(parent, StaticReflect::Reflector<FieldType>::template FieldName<Is>(),
                                     StaticReflect::Reflector<FieldType>::template GetField<Is>(field), doc),
                         0))...};
        (void)dummy;
    }
}

// Forward declaration so dependent calls in set_reflectable_fields_impl can find overloads
template <typename FieldType>
void set_field_from_xml(FieldType& field, const tinyxml2::XMLElement* field_element);

// Helper: expand reflectable fields into set_field_from_xml calls
template <typename T, std::size_t... Is>
void set_reflectable_fields_impl(const tinyxml2::XMLElement* element, T& value, std::index_sequence<Is...>)
{
    if constexpr (sizeof...(Is) > 0)
    {
        int dummy[] = {((element->FirstChildElement(StaticReflect::Reflector<T>::template FieldName<Is>().data())
                             ? (set_field_from_xml(StaticReflect::Reflector<T>::template GetField<Is>(value),
                                                   element->FirstChildElement(
                                                       StaticReflect::Reflector<T>::template FieldName<Is>().data())),
                                0)
                             : 0),
                        0)...};
        (void)dummy;
    }
}

template <TinyXmlConvertible FieldType>
void add_element(tinyxml2::XMLElement* parent, std::string_view name, const FieldType& field,
                 tinyxml2::XMLDocument* doc)
{
    tinyxml2::XMLElement* field_element = doc->NewElement(name.data());
    field.to_xml(field_element, doc);
    parent->InsertEndChild(field_element);
}

template <typename FieldType>
requires std::is_enum_v<FieldType> void add_element(tinyxml2::XMLElement* parent, std::string_view name,
                                                    const FieldType& field, tinyxml2::XMLDocument* doc)
{
    tinyxml2::XMLElement* field_element = doc->NewElement(name.data());
    field_element->SetText(EnumName(field).data());
    parent->InsertEndChild(field_element);
}

template <Aggregte FieldType>
requires(!Reflectable<FieldType>) void add_element(tinyxml2::XMLElement* parent, std::string_view name,
                                                   const FieldType& field, tinyxml2::XMLDocument* doc)
{
    tinyxml2::XMLElement* field_element = doc->NewElement(name.data());
    to_xml_impl(field, field_element, doc);
    parent->InsertEndChild(field_element);
}

// Reflectable field serialization
template <Reflectable FieldType>
void add_element(tinyxml2::XMLElement* parent, std::string_view name, const FieldType& field,
                 tinyxml2::XMLDocument* doc)
{
    tinyxml2::XMLElement* field_element = doc->NewElement(name.data());
    constexpr size_t N = StaticReflect::Reflector<FieldType>::FieldCount();
    add_reflectable_elements_impl<FieldType>(field_element, field, doc, std::make_index_sequence<N>{});
    parent->InsertEndChild(field_element);
}

template <typename FieldType>
requires std::is_same_v<FieldType, std::string> void add_element(tinyxml2::XMLElement* parent, std::string_view name,
                                                                 const FieldType& field, tinyxml2::XMLDocument* doc)
{
    tinyxml2::XMLElement* field_element = doc->NewElement(name.data());
    field_element->SetText(field.c_str());
    parent->InsertEndChild(field_element);
}

template <typename FieldType>
requires std::is_arithmetic_v<FieldType> void add_element(tinyxml2::XMLElement* parent, std::string_view name,
                                                          const FieldType& field, tinyxml2::XMLDocument* doc)
{
    tinyxml2::XMLElement* field_element = doc->NewElement(name.data());
    if constexpr (std::is_integral_v<FieldType>)
    {
        field_element->SetText(static_cast<int64_t>(field));
    }
    else
    {
        field_element->SetText(static_cast<double>(field));
    }
    parent->InsertEndChild(field_element);
}

template <std::ranges::range Range>
requires(!std::is_same_v<Range, std::string>) void add_element(tinyxml2::XMLElement* parent, std::string_view name,
                                                               const Range& rng, tinyxml2::XMLDocument* doc)
{
    tinyxml2::XMLElement* array_element = doc->NewElement(name.data());
    for (const auto& item : rng)
    {
        using ItemType = std::remove_cvref_t<decltype(item)>;
        tinyxml2::XMLElement* item_element = doc->NewElement("item");
        if constexpr (TinyXmlConvertible<ItemType>)
        {
            item.to_xml(item_element, doc);
        }
        else if constexpr (Reflectable<ItemType>)
        {
            to_xml_impl(item, item_element, doc);
        }
        else if constexpr (Aggregte<ItemType>)
        {
            to_xml_impl(item, item_element, doc);
        }
        else if constexpr (std::is_same_v<ItemType, std::string>)
        {
            item_element->SetText(item.c_str());
        }
        else if constexpr (std::is_arithmetic_v<ItemType>)
        {
            if constexpr (std::is_integral_v<ItemType>)
            {
                item_element->SetText(static_cast<int64_t>(item));
            }
            else
            {
                item_element->SetText(item);
            }
        }
        else if constexpr (OptionalLike<ItemType>)
        {
            if (item.has_value())
            {
                using ValueType = std::remove_cvref_t<typename ItemType::value_type>;
                if constexpr (TinyXmlConvertible<ValueType>)
                {
                    item->to_xml(item_element, doc);
                }
                else if constexpr (Reflectable<ValueType>)
                {
                    to_xml_impl(*item, item_element, doc);
                }
                else if constexpr (Aggregte<ValueType>)
                {
                    to_xml_impl(*item, item_element, doc);
                }
                else if constexpr (std::is_same_v<ValueType, std::string>)
                {
                    item_element->SetText(item->c_str());
                }
                else if constexpr (std::is_arithmetic_v<ValueType>)
                {
                    if constexpr (std::is_integral_v<ValueType>)
                    {
                        item_element->SetText(static_cast<int64_t>(*item));
                    }
                    else
                    {
                        item_element->SetText(*item);
                    }
                }
                else if constexpr (std::is_enum_v<ValueType>)
                {
                    item_element->SetText(EnumName(*item).data());
                }
                else
                {
                    throw RuntimeError("Unsupported item type in to_xml_impl for range field with optional");
                }
            }
            else
            {
                // For null optional, perhaps don't add the item or add an empty element
                item_element->SetAttribute("null", "true");
            }
        }
        else
        {
            throw RuntimeError("Unsupported item type in to_xml_impl for range field");
        }
        array_element->InsertEndChild(item_element);
    }
    parent->InsertEndChild(array_element);
}

template <OptionalLike Opt>
void add_element(tinyxml2::XMLElement* parent, std::string_view name, const Opt& opt, tinyxml2::XMLDocument* doc)
{
    using ValueType = std::remove_cvref_t<typename Opt::value_type>;
    tinyxml2::XMLElement* field_element = doc->NewElement(name.data());
    if (opt.has_value())
    {
        if constexpr (TinyXmlConvertible<ValueType>)
        {
            opt->to_xml(field_element, doc);
        }
        else if constexpr (Aggregte<ValueType>)
        {
            to_xml_impl(*opt, field_element, doc);
        }
        else if constexpr (std::is_same_v<ValueType, std::string>)
        {
            field_element->SetText(opt->c_str());
        }
        else if constexpr (std::is_arithmetic_v<ValueType>)
        {
            if constexpr (std::is_integral_v<ValueType>)
            {
                field_element->SetText(static_cast<int64_t>(*opt));
            }
            else
            {
                field_element->SetText(*opt);
            }
        }
        else if constexpr (std::is_enum_v<ValueType>)
        {
            field_element->SetText(EnumName(*opt).data());
        }
        else
        {
            throw RuntimeError("Unsupported field type in to_xml_impl for optional field");
        }
    }
    else
    {
        field_element->SetAttribute("null", "true");
    }
    parent->InsertEndChild(field_element);
}

// fallback for unsupported field types
template <typename FieldType>
void add_element(tinyxml2::XMLElement*, std::string_view, const FieldType&, tinyxml2::XMLDocument*)
{
    throw RuntimeError("Unsupported field type in to_xml_impl");
}

template <Aggregte T>
requires(!Reflectable<T>) void to_xml_impl(const T& value, tinyxml2::XMLElement* element, tinyxml2::XMLDocument* doc)
{
    constexpr std::array<std::string_view, boost::pfr::tuple_size_v<T>> field_names = boost::pfr::names_as_array<T>();
    boost::pfr::for_each_field(value, [&](const auto& field, size_t idx)
                               { add_element(element, field_names[idx], field, doc); });
}

// Reflectable to_xml_impl using StaticReflect::Reflector<T>
template <Reflectable T>
void to_xml_impl(const T& value, tinyxml2::XMLElement* element, tinyxml2::XMLDocument* doc)
{
    constexpr size_t N = StaticReflect::Reflector<T>::FieldCount();
    add_reflectable_elements_impl<T>(element, value, doc, std::make_index_sequence<N>{});
}

// -------------------- from_xml helpers --------------------
template <TinyXmlConvertible FieldType>
void set_field_from_xml(FieldType& field, const tinyxml2::XMLElement* field_element)
{
    field = FieldType::from_xml(field_element);
}

template <typename FieldType>
requires std::is_enum_v<FieldType> void set_field_from_xml(FieldType& field, const tinyxml2::XMLElement* field_element)
{
    const char* text = field_element->GetText();
    if (text)
    {
        field = EnumFromName<FieldType>(text);
    }
}

template <Aggregte FieldType>
requires(!Reflectable<FieldType>) void set_field_from_xml(FieldType& field, const tinyxml2::XMLElement* field_element)
{
    from_xml_impl(field_element, field);
}

// Reflectable field deserialization
template <Reflectable FieldType>
void set_field_from_xml(FieldType& field, const tinyxml2::XMLElement* field_element)
{
    from_xml_impl(field_element, field);
}

template <typename FieldType>
requires std::is_same_v<FieldType, std::string> void set_field_from_xml(FieldType& field,
                                                                        const tinyxml2::XMLElement* field_element)
{
    const char* text = field_element->GetText();
    if (text)
        field = text;
}

template <typename FieldType>
requires std::is_arithmetic_v<FieldType> void set_field_from_xml(FieldType& field,
                                                                 const tinyxml2::XMLElement* field_element)
{
    if constexpr (std::is_integral_v<FieldType>)
    {
        int64_t tmp = 0;
        if (field_element->QueryInt64Text(&tmp) == tinyxml2::XML_SUCCESS)
        {
            field = static_cast<FieldType>(tmp);
        }
        else
        {
            throw RuntimeError("Failed to parse integer text");
        }
    }
    else
    {
        double tmp = 0.0;
        if (field_element->QueryDoubleText(&tmp) == tinyxml2::XML_SUCCESS)
        {
            field = static_cast<FieldType>(tmp);
        }
        else
        {
            throw RuntimeError("Failed to parse double text");
        }
    }
}

template <std::ranges::range Range>
requires(!std::is_same_v<Range, std::string>) void set_field_from_xml(Range& field,
                                                                      const tinyxml2::XMLElement* array_element)
{
    using ItemType = std::remove_cvref_t<typename Range::value_type>;
    field.clear();
    for (const tinyxml2::XMLElement* item_element = array_element->FirstChildElement("item"); item_element;
         item_element = item_element->NextSiblingElement("item"))
    {
        if constexpr (TinyXmlConvertible<ItemType>)
        {
            ItemType item = ItemType::from_xml(item_element);
            field.push_back(std::move(item));
        }
        else if constexpr (Reflectable<ItemType>)
        {
            ItemType item;
            from_xml_impl(item_element, item);
            field.push_back(std::move(item));
        }
        else if constexpr (Aggregte<ItemType>)
        {
            ItemType item;
            from_xml_impl(item_element, item);
            field.push_back(std::move(item));
        }
        else if constexpr (std::is_same_v<ItemType, std::string>)
        {
            const char* text = item_element->GetText();
            if (text)
                field.push_back(text);
        }
        else if constexpr (std::is_arithmetic_v<ItemType>)
        {
            ItemType item;
            if constexpr (std::is_integral_v<ItemType>)
            {
                int64_t tmp = 0;
                if (item_element->QueryInt64Text(&tmp) == tinyxml2::XML_SUCCESS)
                {
                    field.push_back(static_cast<ItemType>(tmp));
                }
            }
            else
            {
                double tmp = 0;
                if (item_element->QueryDoubleText(&tmp) == tinyxml2::XML_SUCCESS)
                {
                    field.push_back(static_cast<ItemType>(tmp));
                }
            }
        }
        else
        {
            throw RuntimeError("Unsupported item type in from_xml_impl for range field");
        }
    }
}

template <OptionalLike Opt>
void set_field_from_xml(Opt& opt, const tinyxml2::XMLElement* field_element)
{
    using ValueType = std::remove_cvref_t<typename Opt::value_type>;
    if (field_element->Attribute("null"))
    {
        opt.reset();
        return;
    }
    if constexpr (TinyXmlConvertible<ValueType>)
    {
        opt = ValueType::from_xml(field_element);
    }
    else if constexpr (Aggregte<ValueType>)
    {
        ValueType v;
        from_xml_impl(field_element, v);
        opt = std::move(v);
    }
    else if constexpr (std::is_same_v<ValueType, std::string>)
    {
        const char* text = field_element->GetText();
        if (text)
            opt = text;
    }
    else if constexpr (std::is_arithmetic_v<ValueType>)
    {
        ValueType v;
        if constexpr (std::is_integral_v<ValueType>)
        {
            int64_t tmp = 0;
            if (field_element->QueryInt64Text(&tmp) == tinyxml2::XML_SUCCESS)
            {
                v = static_cast<ValueType>(tmp);
            }
        }
        else
        {
            double tmp = 0.0;
            if (field_element->QueryDoubleText(&tmp) == tinyxml2::XML_SUCCESS)
            {
                v = static_cast<ValueType>(tmp);
            }
        }
        opt = v;
    }
    else if constexpr (std::is_enum_v<ValueType>)
    {
        const char* text = field_element->GetText();
        if (text)
            opt = EnumFromName<ValueType>(text);
    }
    else
    {
        throw RuntimeError("Unsupported field type in from_xml_impl for optional field");
    }
}

// fallback
template <typename FieldType>
void set_field_from_xml(FieldType&, const tinyxml2::XMLElement*)
{
    throw RuntimeError("Unsupported field type in from_xml_impl");
}

template <Aggregte T>
requires(!Reflectable<T>) void from_xml_impl(const tinyxml2::XMLElement* element, T& value)
{
    constexpr std::array<std::string_view, boost::pfr::tuple_size_v<T>> field_names = boost::pfr::names_as_array<T>();
    boost::pfr::for_each_field(value,
                               [&](auto& field, size_t idx)
                               {
                                   auto name = field_names[idx];
                                   const tinyxml2::XMLElement* field_element = element->FirstChildElement(name.data());
                                   if (!field_element)
                                       return;
                                   set_field_from_xml(field, field_element);
                               });
}

// Reflectable from_xml_impl using StaticReflect::Reflector<T>
template <Reflectable T>
void from_xml_impl(const tinyxml2::XMLElement* element, T& value)
{
    constexpr size_t N = StaticReflect::Reflector<T>::FieldCount();
    set_reflectable_fields_impl<T>(element, value, std::make_index_sequence<N>{});
}
}  // namespace detail

template <Aggregte T>
requires(!Reflectable<T>) std::unique_ptr<tinyxml2::XMLDocument> to_xml(const T& value, const char* root_name = "root")
{
    auto doc = std::make_unique<tinyxml2::XMLDocument>();
    tinyxml2::XMLElement* root = doc->NewElement(root_name);
    doc->InsertFirstChild(root);
    detail::to_xml_impl(value, root, doc.get());
    return doc;
}

// Reflectable public to_xml overloads
template <Reflectable T>
std::unique_ptr<tinyxml2::XMLDocument> to_xml(const T& value, const char* root_name = "root")
{
    auto doc = std::make_unique<tinyxml2::XMLDocument>();
    tinyxml2::XMLElement* root = doc->NewElement(root_name);
    doc->InsertFirstChild(root);
    detail::to_xml_impl(value, root, doc.get());
    return doc;
}

template <TinyXmlConvertible T>
std::unique_ptr<tinyxml2::XMLDocument> to_xml(const T& value, const char* root_name = "root")
{
    auto doc = std::make_unique<tinyxml2::XMLDocument>();
    tinyxml2::XMLElement* root = doc->NewElement(root_name);
    doc->InsertFirstChild(root);
    value.to_xml(root, doc.get());
    return doc;
}

template <std::ranges::range Range>
requires(!std::is_same_v<Range, std::string>) std::unique_ptr<tinyxml2::XMLDocument> to_xml(
    const Range& rng, const char* root_name = "root")
{
    auto doc = std::make_unique<tinyxml2::XMLDocument>();
    tinyxml2::XMLElement* root = doc->NewElement(root_name);
    doc->InsertFirstChild(root);
    tinyxml2::XMLElement* array_element = doc->NewElement("array");
    for (const auto& item : rng)
    {
        using ItemType = std::remove_cvref_t<decltype(item)>;
        tinyxml2::XMLElement* item_element = doc->NewElement("item");
        if constexpr (TinyXmlConvertible<ItemType>)
        {
            item.to_xml(item_element, doc.get());
        }
        else if constexpr (Reflectable<ItemType>)
        {
            detail::to_xml_impl(item, item_element, doc.get());
        }
        else if constexpr (Aggregte<ItemType>)
        {
            detail::to_xml_impl(item, item_element, doc.get());
        }
        else if constexpr (std::is_same_v<ItemType, std::string>)
        {
            item_element->SetText(item.c_str());
        }
        else if constexpr (std::is_arithmetic_v<ItemType>)
        {
            if constexpr (std::is_integral_v<ItemType>)
            {
                item_element->SetText(static_cast<int64_t>(item));
            }
            else
            {
                item_element->SetText(item);
            }
        }
        else
        {
            throw RuntimeError("Unsupported item type in to_xml for range");
        }
        array_element->InsertEndChild(item_element);
    }
    root->InsertEndChild(array_element);
    return doc;
}

template <Aggregte T>
requires(!Reflectable<T>) T from_xml(const tinyxml2::XMLElement* element)
{
    T value{};
    detail::from_xml_impl(element, value);
    return value;
}

// Reflectable public from_xml overloads
template <Reflectable T>
T from_xml(const tinyxml2::XMLElement* element)
{
    T value{};
    detail::from_xml_impl(element, value);
    return value;
}

template <Aggregte T>
requires(!Reflectable<T>) T from_xml(const tinyxml2::XMLDocument& doc)
{
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
    {
        throw InvalidArgumentError("XML document has no root element");
    }
    return from_xml<T>(root);
}

template <Reflectable T>
T from_xml(const tinyxml2::XMLDocument& doc)
{
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
    {
        throw InvalidArgumentError("XML document has no root element");
    }
    return from_xml<T>(root);
}

template <TinyXmlConvertible T>
T from_xml(const tinyxml2::XMLElement* element)
{
    return T::from_xml(element);
}

template <TinyXmlConvertible T>
T from_xml(const tinyxml2::XMLDocument& doc)
{
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
    {
        throw InvalidArgumentError("XML document has no root element");
    }
    return from_xml<T>(root);
}

template <std::ranges::range Range>
requires(!std::is_same_v<Range, std::string>) Range from_xml(const tinyxml2::XMLDocument& doc)
{
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
    {
        throw InvalidArgumentError("XML document has no root element");
    }
    const tinyxml2::XMLElement* array_element = root->FirstChildElement("array");
    if (!array_element)
    {
        throw InvalidArgumentError("No array element");
    }
    Range result;
    for (const tinyxml2::XMLElement* item_element = array_element->FirstChildElement("item"); item_element;
         item_element = item_element->NextSiblingElement("item"))
    {
        using ItemType = std::remove_cvref_t<typename Range::value_type>;
        if constexpr (TinyXmlConvertible<ItemType>)
        {
            ItemType item = ItemType::from_xml(item_element);
            result.push_back(std::move(item));
        }
        else if constexpr (Reflectable<ItemType>)
        {
            ItemType item;
            detail::from_xml_impl(item_element, item);
            result.push_back(std::move(item));
        }
        else if constexpr (Aggregte<ItemType>)
        {
            ItemType item;
            detail::from_xml_impl(item_element, item);
            result.push_back(std::move(item));
        }
        else if constexpr (std::is_same_v<ItemType, std::string>)
        {
            const char* text = item_element->GetText();
            if (text)
                result.push_back(text);
        }
        else if constexpr (std::is_arithmetic_v<ItemType>)
        {
            ItemType item;
            if constexpr (std::is_integral_v<ItemType>)
            {
                int64_t tmp = 0;
                if (item_element->QueryInt64Text(&tmp) == tinyxml2::XML_SUCCESS)
                {
                    result.push_back(static_cast<ItemType>(tmp));
                }
            }
            else
            {
                double tmp = 0;
                if (item_element->QueryDoubleText(&tmp) == tinyxml2::XML_SUCCESS)
                {
                    result.push_back(static_cast<ItemType>(tmp));
                }
            }
        }
        else
        {
            throw RuntimeError("Unsupported item type in from_xml for range");
        }
    }
    return result;
}


template <typename T>
std::ostream& write_xml(std::ostream& os, const T& value, const char* root_name = "root")
{
    auto doc = to_xml(value, root_name);
    tinyxml2::XMLPrinter printer;
    doc->Accept(&printer);
    os << printer.CStr();
    return os;
}

template <typename T>
std::string write_xml(const T& value, const char* root_name = "root")
{
    auto doc = to_xml(value, root_name);
    tinyxml2::XMLPrinter printer;
    doc->Accept(&printer);
    return printer.CStr();
}

template <typename T>
void write_xml(std::string_view path, const T& value, const char* root_name = "root")
{
    std::ofstream ofs(path.data());
    if (!ofs)
    {
        throw IOError("Failed to open file for writing: " + std::string(path));
    }
    write_xml(ofs, value, root_name);
    ofs.close();
}

template <typename T>
T read_xml(std::istream& is)
{
    std::string xml_str((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml_str.c_str()) != tinyxml2::XML_SUCCESS)
    {
        throw RuntimeError("XML parse error");
    }
    return from_xml<T>(doc);
}

template <typename T>
T read_xml_from_file(std::string_view path)
{
    std::ifstream ifs(path.data());
    if (!ifs)
    {
        throw IOError("Failed to open file for reading: " + std::string(path));
    }
    T value = read_xml<T>(ifs);
    ifs.close();
    return value;
}

template <typename T>
T read_xml_from_string(const std::string& xml_str)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml_str.c_str()) != tinyxml2::XML_SUCCESS)
    {
        throw RuntimeError("XML parse error");
    }
    return from_xml<T>(doc);
}
}  // namespace HsBa::Slicer::Utils

#endif  // !HSBA_SLICER_STRUCT_XML_HPP