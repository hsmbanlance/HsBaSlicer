#pragma once

#ifndef HSBA_PROPERTIES_DOC_HPP
#define HSBA_PROPERTIES_DOC_HPP

#include <array>
#include <concepts>
#include <exception>
#include <fstream>
#include <list>
#include <optional>
#include <string>
#include <string_view>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace HsBa::Slicer::Config
{
// not ptree style, simple json doc
/**
 * @brief Simple JSON configuration document handler.
 *
 * This class provides a lightweight interface for reading and writing JSON configuration files
 * using RapidJSON library. It supports basic types (int, double, bool, string) and arrays.
 */
class PropertiesDoc final
{
public:
    PropertiesDoc() = default;

    /**
     * @brief Load configuration from a JSON file.
     * @param path Path to the JSON file.
     * @return true if loading succeeded, false otherwise.
     */
    bool FromJson(std::string_view path);

    /**
     * @brief Save configuration to a JSON file.
     * @param path Output path for the JSON file.
     * @return true if saving succeeded, false otherwise.
     */
    bool Write(std::string_view path);

    /**
     * @brief Get an optional integer value by key.
     * @param key Configuration key.
     * @return Optional integer value (nullopt if key doesn't exist or type mismatch).
     */
    std::optional<int> GetOptionalInt(std::string_view key) const noexcept;

    /**
     * @brief Get an optional double value by key.
     * @param key Configuration key.
     * @return Optional double value (nullopt if key doesn't exist or type mismatch).
     */
    std::optional<double> GetOptionalDouble(std::string_view key) const noexcept;

    /**
     * @brief Get an optional boolean value by key.
     * @param key Configuration key.
     * @return Optional boolean value (nullopt if key doesn't exist or type mismatch).
     */
    std::optional<bool> GetOptionBool(std::string_view key) const noexcept;

    /**
     * @brief Get an optional string value by key.
     * @param key Configuration key.
     * @return Optional string value (nullopt if key doesn't exist or type mismatch).
     */
    std::optional<std::string> GetOptionString(std::string_view key) const noexcept;

    /**
     * @brief Get an optional typed value by key (template version).
     * @tparam T Type of the value to retrieve.
     * @param key Configuration key.
     * @return Optional typed value (nullopt if key doesn't exist or type mismatch).
     */
    template <typename T>
    inline std::optional<T> GetOptionValue(std::string_view key) const noexcept
    {
        if (doc_.HasMember(key.data()))
        {
            const auto& json_item = doc_[key.data()];
            if (json_item.Is<T>())
            {
                return json_item.Get<T>();
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Get an optional array of integers by key.
     * @param key Configuration key.
     * @return Optional list of integers.
     */
    std::optional<std::list<int>> GetOptionalIntArr(std::string_view key) const noexcept;

    /**
     * @brief Get an optional array of doubles by key.
     * @param key Configuration key.
     * @return Optional list of doubles.
     */
    std::optional<std::list<double>> GetOptionalDoubleArr(std::string_view key) const noexcept;

    /**
     * @brief Get an optional array of booleans by key.
     * @param key Configuration key.
     * @return Optional list of booleans.
     */
    std::optional<std::list<bool>> GetOptionalBoolArr(std::string_view key) const noexcept;

    /**
     * @brief Get an optional array of strings by key.
     * @param key Configuration key.
     * @return Optional list of strings.
     */
    std::optional<std::list<std::string>> GetOptionalStrArr(std::string_view key) const noexcept;

    /**
     * @brief Get an optional array of typed values by key (template version).
     * @tparam T Type of array elements.
     * @param key Configuration key.
     * @return Optional list of typed values.
     */
    template <typename T>
    inline std::optional<std::list<T>> GetOptionalStrArr(std::string_view key) const noexcept
    {
        if (doc_.HasMember(key.data()))
        {
            const auto& json_item = doc_[key.data()];
            if (json_item.IsArray())
            {
                std::list<T> res;
                for (const auto& i : json_item.GetArray())
                {
                    if (i.Is<T>())
                    {
                        res.emplace_back(i.Get<T>());
                    }
                    else
                    {
                        return std::nullopt;
                    }
                }
                return res;
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    /**
     * @brief Add an integer member to the document.
     * @param name Member name.
     * @param v Integer value.
     */
    void AddInt(std::string_view name, const int v);

    /**
     * @brief Add a double member to the document.
     * @param name Member name.
     * @param v Double value.
     */
    void AddDouble(std::string_view name, const double v);

    /**
     * @brief Add a boolean member to the document.
     * @param name Member name.
     * @param v Boolean value.
     */
    void AddBool(std::string_view name, const bool v);

    /**
     * @brief Add a string member to the document.
     * @param name Member name.
     * @param v String value.
     */
    void AddString(std::string_view name, const std::string& v);

    /**
     * @brief Add a typed member to the document (template version).
     * @tparam T Type of the value.
     * @param name Member name.
     * @param v Value to add.
     */
    template <typename T>
    inline void AddMember(std::string_view name, const T& v)
    {
        rapidjson::Value value;
        value.Set<T>(v);
        rapidjson::Value name_v;
        name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
        doc_.AddMember(name_v, value, doc_.GetAllocator());
    }

    /**
     * @brief Add an array of integers to the document.
     * @param name Member name.
     * @param arr List of integers.
     */
    void AddIntArr(std::string_view name, const std::list<int>& arr);

    /**
     * @brief Add an array of doubles to the document.
     * @param name Member name.
     * @param arr List of doubles.
     */
    void AddDoubleArr(std::string_view name, const std::list<double>& arr);

    /**
     * @brief Add an array of booleans to the document.
     * @param name Member name.
     * @param arr List of booleans.
     */
    void AddBoolArr(std::string_view name, const std::list<bool>& arr);

    /**
     * @brief Add an array of strings to the document.
     * @param name Member name.
     * @param arr List of strings.
     */
    void AddStringArr(std::string_view name, const std::list<std::string>& arr);

    /**
     * @brief Add an array of typed values to the document (template version).
     * @tparam T Type of array elements.
     * @param name Member name.
     * @param arr List of values.
     */
    template <typename T>
    inline void AddMemberArr(std::string_view name, const std::list<T>& arr)
    {
        rapidjson::Value name_v;
        name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
        auto& allocator = doc_.GetAllocator();
        rapidjson::Value value;
        for (const auto& i : arr)
        {
            rapidjson::Value i_v;
            i_v.Set<T>(i);
            value.PushBack(i_v, allocator);
        }
        doc_.AddMember(name_v, value, allocator);
    }

    /**
     * @brief Get a JSON value by key (const version).
     * @param key Member key.
     * @return Const reference to the JSON value.
     */
    const rapidjson::Value& GetValue(std::string_view key) const;

    /**
     * @brief Get a JSON value by key (mutable version).
     * @param key Member key.
     * @return Reference to the JSON value.
     */
    rapidjson::Value& GetValue(std::string_view key);

    /**
     * @brief Add a pre-constructed JSON value to the document.
     * @param name Member name.
     * @param value JSON value to add.
     */
    void AddValue(std::string_view name, rapidjson::Value& value);

private:
    rapidjson::Document doc_;
};
}  // namespace HsBa::Slicer::Config
#endif  // !HSBA_PROPERTIES_DOC_HPP
