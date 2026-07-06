#pragma once
#ifndef HSBA_SLICER_STRUCT_YAML_HPP
#define HSBA_SLICER_STRUCT_YAML_HPP

#include <boost/pfr.hpp>
#include <boost/pfr/core_name.hpp>
#include <fstream>
#include <limits>
#include <memory>
#include <ranges>
#include <sstream>
#include <string_view>
#include <type_traits>

#include <yaml-cpp/yaml.h>

#include "base/concepts.hpp"
#include "base/error.hpp"
#include "base/static_reflect.hpp"
#include "base/template_helper.hpp"

#include "struct_concepts.hpp"

namespace HsBa::Slicer::Utils
{
/**
 * @brief Utilities for YAML serialization and deserialization.
 *
 * This module provides generic helpers to convert aggregate types,
 * reflectable types, optional-like values, and YAML-aware custom types
 * to and from yaml-cpp nodes.
 */

/**
 * @brief Concept for types that support YAML conversion APIs.
 *
 * Types satisfying this concept must implement a member function
 * `to_yaml(YAML::Node&)` and a static function `from_yaml(const YAML::Node&)`.
 *
 * @tparam T Candidate type.
 */
template <typename T>
concept YAMLValueConvertible = requires(const T& value, YAML::Node& node)
{
    {value.to_yaml(node)}->std::same_as<void>;
    {T::from_yaml(node)}->std::same_as<T>;
};


namespace detail
{
// Forward declarations for recursive / dependent calls
template <Aggregte T>
requires(!Reflectable<T>) void to_yaml_impl(const T& value, YAML::Node& node);

template <Aggregte T>
requires(!Reflectable<T>) void from_yaml_impl(const YAML::Node& node, T& value);

// Forward decls for reflectable types
template <Reflectable T>
void to_yaml_impl(const T& value, YAML::Node& node);

template <Reflectable T>
void from_yaml_impl(const YAML::Node& node, T& value);

template <typename FieldType>
void add_member(YAML::Node& parent, std::string_view name, const FieldType& field);

template <typename FieldType>
void set_field_from_yaml(FieldType& field, const YAML::Node& field_node);

// Helper: expand reflectable fields into add_member calls
template <typename FieldType, std::size_t... Is>
void add_reflectable_members_impl(YAML::Node& parent, const FieldType& field, std::index_sequence<Is...>)
{
    if constexpr (sizeof...(Is) > 0)
    {
        int dummy[] = {((add_member(parent, StaticReflect::Reflector<FieldType>::template FieldName<Is>(),
                                    StaticReflect::Reflector<FieldType>::template GetField<Is>(field)),
                         0))...};
        (void)dummy;
    }
}

// Helper: expand reflectable fields into set_field_from_yaml calls
template <typename T, std::size_t... Is>
void set_reflectable_fields_impl(const YAML::Node& node, T& value, std::index_sequence<Is...>);

template <typename FieldType>
void add_member(YAML::Node& parent, std::string_view name, const FieldType& field)
{
    if constexpr (YAMLValueConvertible<FieldType>)
    {
        YAML::Node field_node;
        field.to_yaml(field_node);
        parent[name.data()] = field_node;
    }
    else if constexpr (std::is_enum_v<FieldType>)
    {
        parent[name.data()] = std::string(EnumName(field).data(), EnumName(field).size());
    }
    else if constexpr (Reflectable<FieldType>)
    {
        YAML::Node field_node(YAML::NodeType::Map);
        constexpr size_t N = StaticReflect::Reflector<FieldType>::FieldCount();
        add_reflectable_members_impl<FieldType>(field_node, field, std::make_index_sequence<N>{});
        parent[name.data()] = field_node;
    }
    else if constexpr (Aggregte<FieldType>)
    {
        YAML::Node field_node(YAML::NodeType::Map);
        to_yaml_impl(field, field_node);
        parent[name.data()] = field_node;
    }
    else if constexpr (std::is_same_v<FieldType, std::string>)
    {
        parent[name.data()] = field;
    }
    else if constexpr (std::is_arithmetic_v<FieldType>)
    {
        parent[name.data()] = field;
    }
    else
    {
        throw RuntimeError("Unsupported field type in to_yaml_impl");
    }
}

template <std::ranges::range Range>
requires(!std::is_same_v<Range, std::string>) void add_member(YAML::Node& parent, std::string_view name,
                                                              const Range& rng)
{
    YAML::Node array_node(YAML::NodeType::Sequence);
    for (const auto& item : rng)
    {
        using ItemType = std::remove_cvref_t<decltype(item)>;
        if constexpr (YAMLValueConvertible<ItemType>)
        {
            YAML::Node item_node;
            item.to_yaml(item_node);
            array_node.push_back(item_node);
        }
        else if constexpr (Reflectable<ItemType>)
        {
            YAML::Node item_node(YAML::NodeType::Map);
            to_yaml_impl(item, item_node);
            array_node.push_back(item_node);
        }
        else if constexpr (Aggregte<ItemType>)
        {
            YAML::Node item_node(YAML::NodeType::Map);
            to_yaml_impl(item, item_node);
            array_node.push_back(item_node);
        }
        else if constexpr (std::is_same_v<ItemType, std::string>)
        {
            array_node.push_back(item);
        }
        else if constexpr (std::is_arithmetic_v<ItemType>)
        {
            array_node.push_back(item);
        }
        else if constexpr (OptionalLike<ItemType>)
        {
            if (item.has_value())
            {
                using ValueType = std::remove_cvref_t<typename ItemType::value_type>;
                if constexpr (YAMLValueConvertible<ValueType>)
                {
                    YAML::Node item_node;
                    item->to_yaml(item_node);
                    array_node.push_back(item_node);
                }
                else if constexpr (Reflectable<ValueType>)
                {
                    YAML::Node item_node(YAML::NodeType::Map);
                    to_yaml_impl(*item, item_node);
                    array_node.push_back(item_node);
                }
                else if constexpr (Aggregte<ValueType>)
                {
                    YAML::Node item_node(YAML::NodeType::Map);
                    to_yaml_impl(*item, item_node);
                    array_node.push_back(item_node);
                }
                else if constexpr (std::is_same_v<ValueType, std::string>)
                {
                    array_node.push_back(item->c_str());
                }
                else if constexpr (std::is_arithmetic_v<ValueType>)
                {
                    array_node.push_back(*item);
                }
                else if constexpr (std::is_enum_v<ValueType>)
                {
                    array_node.push_back(std::string(EnumName(*item).data(), EnumName(*item).size()));
                }
                else
                {
                    throw RuntimeError("Unsupported item type in to_yaml_impl for range field with optional");
                }
            }
            else
            {
                array_node.push_back(YAML::Node());
            }
        }
        else
        {
            throw RuntimeError("Unsupported item type in to_yaml_impl for range field");
        }
    }
    parent[name.data()] = array_node;
}

template <OptionalLike Opt>
void add_member(YAML::Node& parent, std::string_view name, const Opt& opt)
{
    using ValueType = std::remove_cvref_t<typename Opt::value_type>;
    if (opt.has_value())
    {
        if constexpr (YAMLValueConvertible<ValueType>)
        {
            YAML::Node field_node;
            opt->to_yaml(field_node);
            parent[name.data()] = field_node;
        }
        else if constexpr (Aggregte<ValueType>)
        {
            YAML::Node field_node(YAML::NodeType::Map);
            to_yaml_impl(*opt, field_node);
            parent[name.data()] = field_node;
        }
        else if constexpr (std::is_same_v<ValueType, std::string>)
        {
            parent[name.data()] = opt->c_str();
        }
        else if constexpr (std::is_arithmetic_v<ValueType>)
        {
            parent[name.data()] = *opt;
        }
        else if constexpr (std::is_enum_v<ValueType>)
        {
            parent[name.data()] = std::string(EnumName(*opt).data(), EnumName(*opt).size());
        }
        else
        {
            throw RuntimeError("Unsupported field type in to_yaml_impl for optional field");
        }
    }
    else
    {
        parent[name.data()] = YAML::Node();
    }
}

// fallback for unsupported field types
// template<typename FieldType>
// void add_member(YAML::Node&, std::string_view, const FieldType&)
// {
// 	throw RuntimeError("Unsupported field type in to_yaml_impl");
// }

template <Aggregte T>
requires(!Reflectable<T>) void to_yaml_impl(const T& value, YAML::Node& node)
{
    node = YAML::Node(YAML::NodeType::Map);
    constexpr std::array<std::string_view, boost::pfr::tuple_size_v<T>> field_names = boost::pfr::names_as_array<T>();
    boost::pfr::for_each_field(value,
                               [&](const auto& field, size_t idx) { add_member(node, field_names[idx], field); });
}

// Reflectable to_yaml_impl using StaticReflect::Reflector<T>
template <Reflectable T>
void to_yaml_impl(const T& value, YAML::Node& node)
{
    node = YAML::Node(YAML::NodeType::Map);
    constexpr size_t N = StaticReflect::Reflector<T>::FieldCount();
    add_reflectable_members_impl<T>(node, value, std::make_index_sequence<N>{});
}

// -------------------- from_yaml helpers --------------------
template <typename FieldType>
void set_field_from_yaml(FieldType& field, const YAML::Node& field_node)
{
    if constexpr (YAMLValueConvertible<FieldType>)
    {
        field = FieldType::from_yaml(field_node);
    }
    else if constexpr (std::is_enum_v<FieldType>)
    {
        if (field_node.IsScalar())
        {
            field = EnumFromName<FieldType>(field_node.as<std::string>());
        }
    }
    else if constexpr (Reflectable<FieldType>)
    {
        from_yaml_impl(field_node, field);
    }
    else if constexpr (Aggregte<FieldType>)
    {
        from_yaml_impl(field_node, field);
    }
    else if constexpr (std::is_same_v<FieldType, std::string>)
    {
        if (field_node.IsScalar())
            field = field_node.as<std::string>();
    }
    else if constexpr (std::is_arithmetic_v<FieldType>)
    {
        if (field_node.IsScalar())
            field = field_node.as<FieldType>();
    }
    else
    {
        throw RuntimeError("Unsupported field type in from_yaml_impl");
    }
}

template <std::ranges::range Range>
requires(!std::is_same_v<Range, std::string>) void set_field_from_yaml(Range& field, const YAML::Node& array_node)
{
    using ItemType = std::remove_cvref_t<typename Range::value_type>;
    field.clear();
    if (!array_node.IsSequence())
        return;
    for (const auto& item_node : array_node)
    {
        if constexpr (YAMLValueConvertible<ItemType>)
        {
            ItemType item = ItemType::from_yaml(item_node);
            field.push_back(std::move(item));
        }
        else if constexpr (Reflectable<ItemType>)
        {
            ItemType item;
            from_yaml_impl(item_node, item);
            field.push_back(std::move(item));
        }
        else if constexpr (Aggregte<ItemType>)
        {
            ItemType item;
            from_yaml_impl(item_node, item);
            field.push_back(std::move(item));
        }
        else if constexpr (std::is_same_v<ItemType, std::string>)
        {
            field.push_back(item_node.as<std::string>());
        }
        else if constexpr (std::is_arithmetic_v<ItemType>)
        {
            field.push_back(item_node.as<ItemType>());
        }
        else
        {
            throw RuntimeError("Unsupported item type in from_yaml_impl for range field");
        }
    }
}

template <OptionalLike Opt>
void set_field_from_yaml(Opt& opt, const YAML::Node& field_node)
{
    using ValueType = std::remove_cvref_t<typename Opt::value_type>;
    if (!field_node || field_node.IsNull())
    {
        opt.reset();
        return;
    }
    if constexpr (YAMLValueConvertible<ValueType>)
    {
        opt = ValueType::from_yaml(field_node);
    }
    else if constexpr (Aggregte<ValueType>)
    {
        ValueType v;
        from_yaml_impl(field_node, v);
        opt = std::move(v);
    }
    else if constexpr (std::is_same_v<ValueType, std::string>)
    {
        opt = field_node.as<std::string>();
    }
    else if constexpr (std::is_arithmetic_v<ValueType>)
    {
        opt = field_node.as<ValueType>();
    }
    else if constexpr (std::is_enum_v<ValueType>)
    {
        opt = EnumFromName<ValueType>(field_node.as<std::string>());
    }
    else
    {
        throw RuntimeError("Unsupported field type in from_yaml_impl for optional field");
    }
}

// fallback
// template<typename FieldType>
// void set_field_from_yaml(FieldType&, const YAML::Node&)
// {
// 	throw RuntimeError("Unsupported field type in from_yaml_impl");
// }

// Helper: expand reflectable fields into set_field_from_yaml calls
template <typename T, std::size_t... Is>
void set_reflectable_fields_impl(const YAML::Node& node, T& value, std::index_sequence<Is...>)
{
    if constexpr (sizeof...(Is) > 0)
    {
        int dummy[] = {((node[StaticReflect::Reflector<T>::template FieldName<Is>().data()]
                             ? (set_field_from_yaml(StaticReflect::Reflector<T>::template GetField<Is>(value),
                                                    node[StaticReflect::Reflector<T>::template FieldName<Is>().data()]),
                                0)
                             : 0),
                        0)...};
        (void)dummy;
    }
}

template <Aggregte T>
requires(!Reflectable<T>) void from_yaml_impl(const YAML::Node& node, T& value)
{
    if (!node.IsMap())
        return;
    constexpr std::array<std::string_view, boost::pfr::tuple_size_v<T>> field_names = boost::pfr::names_as_array<T>();
    boost::pfr::for_each_field(value,
                               [&](auto& field, size_t idx)
                               {
                                   auto name = field_names[idx];
                                   const YAML::Node& field_node = node[name.data()];
                                   if (!field_node)
                                       return;
                                   set_field_from_yaml(field, field_node);
                               });
}

// Reflectable from_yaml_impl using StaticReflect::Reflector<T>
template <Reflectable T>
void from_yaml_impl(const YAML::Node& node, T& value)
{
    if (!node.IsMap())
        return;
    constexpr size_t N = StaticReflect::Reflector<T>::FieldCount();
    set_reflectable_fields_impl<T>(node, value, std::make_index_sequence<N>{});
}
}  // namespace detail

/**
 * @brief Convert an aggregate type to a YAML node.
 *
 * @tparam T Aggregate type to serialize.
 * @param value The aggregate type instance.
 * @return YAML::Node The resulting YAML node.
 */
template <Aggregte T>
requires(!Reflectable<T>) YAML::Node to_yaml(const T& value)
{
    YAML::Node node(YAML::NodeType::Map);
    detail::to_yaml_impl(value, node);
    return node;
}

/**
 * @brief Convert a reflectable type to a YAML node.
 *
 * @tparam T Reflectable type to serialize.
 * @param value The reflectable type instance.
 * @return YAML::Node The resulting YAML node.
 */
// Reflectable public to_yaml overloads
template <Reflectable T>
YAML::Node to_yaml(const T& value)
{
    YAML::Node node(YAML::NodeType::Map);
    detail::to_yaml_impl(value, node);
    return node;
}

/**
 * @brief Convert a YAML value convertible type to a YAML node.
 *
 * @tparam T Type implementing YAML conversion helpers.
 * @param value The value convertible instance.
 * @return YAML::Node The resulting YAML node.
 */
template <YAMLValueConvertible T>
YAML::Node to_yaml(const T& value)
{
    YAML::Node node;
    value.to_yaml(node);
    return node;
}

/**
 * @brief Deserialize a YAML node into an aggregate type.
 *
 * @tparam T Aggregate type to construct.
 * @param node The YAML node to deserialize.
 * @return T Deserialized aggregate.
 * @throws InvalidArgumentError if the YAML node is not a map.
 */
template <Aggregte T>
requires(!Reflectable<T>) T from_yaml(const YAML::Node& node)
{
    if (!node.IsMap())
    {
        throw InvalidArgumentError("YAML node is not a map");
    }
    T value{};
    detail::from_yaml_impl(node, value);
    return value;
}

/**
 * @brief Deserialize a YAML node into a reflectable type.
 *
 * @tparam T Reflectable type to construct.
 * @param node The YAML node to deserialize.
 * @return T Deserialized reflectable.
 * @throws InvalidArgumentError if the YAML node is not a map.
 */
// Reflectable public from_yaml overloads
template <Reflectable T>
T from_yaml(const YAML::Node& node)
{
    if (!node.IsMap())
    {
        throw InvalidArgumentError("YAML node is not a map");
    }
    T value{};
    detail::from_yaml_impl(node, value);
    return value;
}

/**
 * @brief Deserialize a YAML node into a YAML value convertible type.
 *
 * @tparam T Type implementing YAML conversion helpers.
 * @param node The YAML node to deserialize.
 * @return T Deserialized value.
 * @throws InvalidArgumentError if the YAML node is not a map.
 */
template <YAMLValueConvertible T>
T from_yaml(const YAML::Node& node)
{
    if (!node.IsMap())
    {
        throw InvalidArgumentError("YAML node is not a map");
    }
    return T::from_yaml(node);
}

/**
 * @brief Serialize a value to a YAML text stream.
 *
 * @tparam T Type of value to serialize.
 * @param os Output stream to write YAML text to.
 * @param value Value to serialize.
 * @return std::ostream& The output stream.
 */
template <typename T>
std::ostream& write_yaml(std::ostream& os, const T& value)
{
    YAML::Node node = to_yaml(value);
    YAML::Emitter emitter;
    emitter << node;
    if (!emitter.good())
    {
        throw RuntimeError(std::string("YAML emit error: ") + emitter.GetLastError());
    }
    os << emitter.c_str();
    return os;
}

/**
 * @brief Serialize a value to a YAML string.
 *
 * @tparam T Type of value to serialize.
 * @param value Value to serialize.
 * @return std::string YAML string representation.
 */
template <typename T>
std::string write_yaml(const T& value)
{
    YAML::Node node = to_yaml(value);
    YAML::Emitter emitter;
    emitter << node;
    if (!emitter.good())
    {
        throw RuntimeError(std::string("YAML emit error: ") + emitter.GetLastError());
    }
    return std::string(emitter.c_str());
}

/**
 * @brief Write a value to a YAML file.
 *
 * @tparam T Type of value to serialize.
 * @param path File path to write YAML content.
 * @param value Value to serialize.
 */
template <typename T>
void write_yaml(std::string_view path, const T& value)
{
    std::ofstream ofs(path.data());
    if (!ofs)
    {
        throw IOError("Failed to open file for writing: " + std::string(path));
    }
    write_yaml(ofs, value);
    ofs.close();
}

/**
 * @brief Read a value from a YAML input stream.
 *
 * @tparam T Type to deserialize.
 * @param is Input stream containing YAML text.
 * @return T Deserialized value.
 */
template <typename T>
T read_yaml(std::istream& is)
{
    std::string yaml_str((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    YAML::Node node = YAML::Load(yaml_str);
    return from_yaml<T>(node);
}

/**
 * @brief Read a value from a YAML file.
 *
 * @tparam T Type to deserialize.
 * @param path File path to load YAML from.
 * @return T Deserialized value.
 */
template <typename T>
T read_yaml_from_file(std::string_view path)
{
    std::ifstream ifs(path.data());
    if (!ifs)
    {
        throw IOError("Failed to open file for reading: " + std::string(path));
    }
    T value = read_yaml<T>(ifs);
    ifs.close();
    return value;
}

/**
 * @brief Read a value from a YAML string.
 *
 * @tparam T Type to deserialize.
 * @param yaml_str String containing YAML text.
 * @return T Deserialized value.
 */
template <typename T>
T read_yaml_from_string(const std::string& yaml_str)
{
    YAML::Node node = YAML::Load(yaml_str);
    return from_yaml<T>(node);
}

}  // namespace HsBa::Slicer::Utils

#endif  // !HSBA_SLICER_STRUCT_YAML_HPP