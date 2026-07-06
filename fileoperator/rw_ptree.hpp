#pragma once
#ifndef HSBA_SLICER_RW_PRTREE_HPP
#define HSBA_SLICER_RW_PRTREE_HPP

#include <any>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include <boost/property_tree/ptree.hpp>

#include "base/any_visit.hpp"
#include "base/concepts.hpp"
#include "base/error.hpp"

namespace HsBa::Slicer::Config
{
/**
 * @brief Load configuration from an INI file into a property tree.
 * @param path Path to the INI file.
 * @return Property tree containing the configuration.
 * @throws boost::property_tree::file_parser_error if parsing fails.
 */
boost::property_tree::ptree from_ini(const std::string& path);

/**
 * @brief Load configuration from an XML file into a property tree.
 * @param path Path to the XML file.
 * @return Property tree containing the configuration.
 * @throws boost::property_tree::file_parser_error if parsing fails.
 */
boost::property_tree::ptree from_xml(const std::string& path);

/**
 * @brief Load configuration from a JSON file into a property tree.
 * @param path Path to the JSON file.
 * @return Property tree containing the configuration.
 * @throws boost::property_tree::file_parser_error if parsing fails.
 */
boost::property_tree::ptree from_json(const std::string& path);

// error boost::property_tree::file_parser_error
/**
 * @brief Save property tree to an INI file.
 * @param path Output path for the INI file.
 * @param ptree Property tree to save.
 * @throws boost::property_tree::file_parser_error if writing fails.
 */
void to_ini(const std::string& path, const boost::property_tree::ptree& ptree);

// error boost::property_tree::file_parser_error
/**
 * @brief Save property tree to an XML file.
 * @param path Output path for the XML file.
 * @param ptree Property tree to save.
 * @throws boost::property_tree::file_parser_error if writing fails.
 */
void to_xml(const std::string& path, const boost::property_tree::ptree& ptree);

// error boost::property_tree::file_parser_error
/**
 * @brief Save property tree to a JSON file.
 * @param path Output path for the JSON file.
 * @param ptree Property tree to save.
 * @throws boost::property_tree::file_parser_error if writing fails.
 */
void to_json(const std::string& path, const boost::property_tree::ptree& ptree);

/**
 * @brief Base interface for configuration map implementations.
 *
 * This abstract class defines the common interface for type-safe and type-erased
 * configuration storage systems.
 */
class IConfigMap
{
public:
    virtual ~IConfigMap() = default;
};

class AnyConfigMap;

/**
 * @brief Type-safe configuration map using std::variant.
 *
 * This template class provides a type-safe configuration storage system where
 * values are stored in a variant with compile-time type checking.
 * @tparam Args Variadic template parameters specifying allowed types.
 */
template <typename... Args>
class VariantConfigMap : public IConfigMap
{
public:
    virtual ~VariantConfigMap() = default;

    /**
     * @brief Get an optional typed value by key.
     * @tparam T Type of the value to retrieve.
     * @param key Configuration key.
     * @return Optional typed value (nullopt if key doesn't exist or type mismatch).
     */
    template <typename T>
    inline std::optional<T> GetOptional(const std::string& key) const noexcept
    {
        if (config_map_.contains(key))
        {
            try
            {
                return std::get<T>(config_map_.at(key));
            }
            catch (const std::bad_variant_access&)
            {
                return std::nullopt;
            }
        }
        else
        {
            return std::nullopt;
        }
    }

    /**
     * @brief Add or change a typed value by key.
     * @tparam T Type of the value.
     * @param key Configuration key.
     * @param value Value to add or update.
     * @return true if operation succeeded, false if type mismatch on existing key.
     */
    template <typename T>
    inline bool AddOrChangeValue(const std::string& key, T value)
    {
        if (config_map_.contains(key))
        {
            try
            {
                T v = std::get<T>(config_map_[key]);
                config_map_[key] = value;
                return true;
            }
            catch (const std::bad_variant_access&)
            {
                return false;
            }
        }
        config_map_[key] = value;
        return true;
    }

    /**
     * @brief Convert configuration map to a Boost property tree.
     * @return Property tree representation.
     */
    inline boost::property_tree::ptree ToPtree() const
    {
        boost::property_tree::ptree ptree;
        for (const auto& [key, value] : config_map_)
        {
            std::visit([&key, &ptree](auto&& arg) { ptree.add(key, arg); }, value);
        }
        return ptree;
    }

    /**
     * @brief Add a value from a property tree by key (with automatic type conversion).
     * @tparam T Type of the value to extract.
     * @param ptree Source property tree.
     * @param key Key to look up in the property tree.
     * @return true if operation succeeded, false otherwise.
     */
    template <typename T>
    inline bool AddValueInPtree(const boost::property_tree::ptree& ptree, const std::string& key)
    {
        T value = ptree.get<T>(key);
        return AddOrChangeValue(key, value);
    }

    /**
     * @brief Add a value from a property tree by key with custom translator.
     * @tparam T Type of the value to extract.
     * @tparam Translator Type of the custom translator.
     * @param ptree Source property tree.
     * @param key Key to look up in the property tree.
     * @param tr Custom translator for type conversion.
     * @return true if operation succeeded, false otherwise.
     */
    template <typename T, typename Translator>
    inline bool AddValueInPtree(const boost::property_tree::ptree& ptree, const std::string& key, Translator tr)
    {
        T value = ptree.get<T>(key, T{}, tr);
        return AddOrChangeValue(key, value);
    }

    /**
     * @brief Convert to type-erased AnyConfigMap.
     * @return AnyConfigMap containing all configuration values.
     */
    AnyConfigMap ToAnyMap() const;

private:
    std::unordered_map<std::string, std::variant<Args...>> config_map_;  ///< Type-safe configuration storage
};

/**
 * @brief Type-erased configuration map using std::any.
 *
 * This class provides a flexible configuration storage system where values
 * are stored as std::any, allowing runtime type flexibility at the cost of
 * compile-time type safety.
 */
class AnyConfigMap : public IConfigMap
{
public:
    virtual ~AnyConfigMap() = default;

    /**
     * @brief Get an optional typed value by key.
     * @tparam T Type of the value to retrieve.
     * @param key Configuration key.
     * @return Optional typed value (nullopt if key doesn't exist or type mismatch).
     */
    template <typename T>
    inline std::optional<T> GetOptional(const std::string& key) const noexcept
    {
        if (config_map_.contains(key))
        {
            try
            {
                T value = std::any_cast<T>(config_map_.at(key));
                return value;
            }
            catch (const std::bad_any_cast&)
            {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Add or change a typed value by key.
     * @tparam T Type of the value.
     * @param key Configuration key.
     * @param value Value to add or update.
     * @return true if operation succeeded, false if type mismatch on existing key.
     */
    template <typename T>
    inline bool AddOrChangeValue(const std::string& key, T value)
    {
        if (config_map_.contains(key))
        {
            if (config_map_[key].type() == typeid(T))
            {
                config_map_[key] = value;
                return true;
            }
            return false;
        }
        else
        {
            config_map_[key] = value;
            return true;
        }
    }

    /**
     * @brief Convert configuration map to a Boost property tree.
     * @tparam Args Variadic template parameters specifying types to visit.
     * @return Property tree representation.
     */
    template <typename... Args>
    inline boost::property_tree::ptree ToPtree() const
    {
        boost::property_tree::ptree ptree;
        for (const auto& [key, value] : config_map_)
        {
            Utils::Visit<Args...>([&key, &ptree](auto&& arg) { ptree.add(key, arg); }, value);
        }
        return ptree;
    }

    /**
     * @brief Add a value from a property tree by key (with automatic type conversion).
     * @tparam T Type of the value to extract.
     * @param ptree Source property tree.
     * @param key Key to look up in the property tree.
     * @return true if operation succeeded, false otherwise.
     */
    template <typename T>
    inline bool AddValueInPtree(const boost::property_tree::ptree& ptree, const std::string& key)
    {
        T value = ptree.get<T>(key);
        return AddOrChangeValue(key, value);
    }

    /**
     * @brief Add a value from a property tree by key with custom translator.
     * @tparam T Type of the value to extract.
     * @tparam Translator Type of the custom translator.
     * @param ptree Source property tree.
     * @param key Key to look up in the property tree.
     * @param tr Custom translator for type conversion.
     * @return true if operation succeeded, false otherwise.
     */
    template <typename T, typename Translator>
    inline bool AddValueInPtree(const boost::property_tree::ptree& ptree, const std::string& key, Translator tr)
    {
        T value = ptree.get<T>(key, T{}, tr);
        return AddOrChangeValue(key, value);
    }

    /**
     * @brief Convert to type-safe VariantConfigMap.
     * @tparam Args Variadic template parameters specifying allowed types.
     * @return VariantConfigMap containing all compatible configuration values.
     */
    template <typename... Args>
    inline VariantConfigMap<Args...> ToVariantConfigMap() const
    {
        VariantConfigMap<Args...> map;
        for (const auto& [key, value] : config_map_)
        {
            Utils::Visit<Args...>([&key, &map](auto&& arg) { map.AddOrChangeValue(key, arg); }, value);
        }
        return map;
    }

private:
    std::unordered_map<std::string, std::any> config_map_;  ///< Type-erased configuration storage
};

/**
 * @brief Implementation of VariantConfigMap::ToAnyMap().
 * @tparam Args Variadic template parameters from VariantConfigMap.
 * @return Converted AnyConfigMap.
 */
template <typename... Args>
AnyConfigMap VariantConfigMap<Args...>::ToAnyMap() const
{
    AnyConfigMap map;
    for (const auto& [key, value] : config_map_)
    {
        std::visit([&key, &map](auto&& args) { map.AddOrChangeValue(key, args); }, value);
    }
    return map;
}

/**
 * @brief Change value translator in property tree (single translator version).
 *
 * This function attempts to get a value with a translator and updates it in place.
 * @tparam T Type of the value.
 * @tparam Translator Type of the translator.
 * @param ptree Reference to the property tree to modify.
 * @param key Key to look up and update.
 * @param tr Translator for type conversion.
 * @return false (reserved for future use).
 */
template <typename T, typename Translator>
requires StrTranslator<T, Translator> inline bool ChangeTranslator(/*ref*/ boost::property_tree::ptree& ptree,
                                                                   const std::string& key, Translator tr)
{
    boost::optional<T> value = ptree.get_optional<T>(key, tr);
    if (value.has_value())
    {
        ptree.put<T>(key, value.value(), tr);
    }
    return false;
}

/**
 * @brief Change value translator in property tree (old-to-new translator version).
 *
 * This function migrates a value from old translator format to new translator format.
 * @tparam T Type of the value.
 * @tparam OldTranslator Type of the old translator.
 * @tparam Translator Type of the new translator.
 * @param ptree Reference to the property tree to modify.
 * @param key Key to look up and update.
 * @param tr_old Old translator for reading the value.
 * @param tr New translator for writing the value.
 * @return false (reserved for future use).
 */
template <typename T, typename OldTranslator, typename Translator>
requires StrTranslator<T, OldTranslator>&& StrTranslator<T, Translator> inline bool
ChangeTranslator(/*ref*/ boost::property_tree::ptree& ptree, const std::string& key, OldTranslator tr_old,
                 Translator tr)
{
    boost::optional<T> value = ptree.get_optional<T>(key, tr_old);
    if (value.has_value())
    {
        ptree.put<T>(key, value.value(), tr);
    }
    return false;
}
}  // namespace HsBa::Slicer::Config

#endif  // !HSBA_SLICER_RW_PRTREE_HPP
