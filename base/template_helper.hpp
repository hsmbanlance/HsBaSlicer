/** @file template_helper.hpp
 * @brief A collection of template utilities and helper functions for the HsBa Slicer project.
 * This file includes various template classes and functions that provide functionality such as fixed-size string
 * handling, named pointers, enum utilities, and more. These utilities are designed to facilitate common programming
 * tasks and improve code readability and maintainability within the project. The file also includes necessary header
 * files and uses C++20 features to ensure modern and efficient code.
 * @author HsBa
 * @date 2024-06
 */
#pragma once
#ifndef HSBA_SLICER_TEMPLATE_HELPHER_HPP
#define HSBA_SLICER_TEMPLATE_HELPHER_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <exception>
#include <future>
#include <list>
#include <optional>
#include <ranges>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#if __cpp_lib_coroutine && __cpp_impl_coroutine
#include <coroutine>
#endif
#ifdef __cpp_lib_generator
#include <generator>
#endif  // __cpp_lib_generator

#include <magic_enum/magic_enum.hpp>

#include "concepts.hpp"
#include "error.hpp"

namespace HsBa::Slicer::Utils
{
/** @brief A template string class for handling fixed-size strings.
 * This class provides various constructors for different string types, as well as utility functions for string
 * manipulation and comparison. It also supports conversion to std::basic_string_view and std::basic_string, making it
 * versatile for various use cases. The class is designed to be used as a template parameter for compile-time string
 * handling, allowing for efficient and type-safe string operations at compile time.
 */
template <CharType T, size_t N>
struct TemplateString
{
    /** @brief The character type for the string. */
    using Char = T;
    /** @brief The string view type for the string. */
    using StringView = std::basic_string_view<T>;
    /** @brief The string type for the string. */
    using String = std::basic_string<T>;
    inline constexpr TemplateString() = default;
    /** @brief Constructs a TemplateString from a null-terminated character array.
     * If the input string is longer than N, it will be truncated to fit the size of the TemplateString. If the input
     * string is shorter than N, the remaining characters in the TemplateString will be default-initialized (typically
     * null characters).
     * @param str The null-terminated character array to construct the TemplateString from.
     * @tparam N The size of the TemplateString, which determines how many characters from
     */
    inline constexpr TemplateString(const T (&str)[N]) { std::copy(str, str + N, this->str); }
    /** @brief Constructs a TemplateString from a character array of a different size.
     * If the input string is longer than N, it will be truncated to fit the size of the TemplateString. If the input
     * string is shorter than N, the remaining characters in the TemplateString will be default-initialized (typically
     * null characters).
     * @param str The character array to construct the TemplateString from.
     * @tparam M The size of the input character array.
     */
    template <size_t M>
    inline constexpr TemplateString(const T (&str)[M])
    {
        if (M > N)
        {
            std::copy(str, str + N, this->str);
        }
        else
        {
            std::copy(str, str + M, this->str);
        }
    }
    /** @brief Constructs a TemplateString from a std::array of characters.
     * If the input array is larger than N, it will be truncated to fit the size of the TemplateString. If the input
     * array is smaller than N, the remaining characters in the TemplateString will be default-initialized (typically
     * null characters).
     * @param arr The std::array of characters to construct the TemplateString from.
     * @tparam M The size of the input std::array.
     */
    inline constexpr TemplateString(const std::array<T, N>& arr) { std::copy(arr.begin(), arr.end(), this->str); }
    /** @brief Constructs a TemplateString from a std::basic_string_view.
     * If the input string view is longer than N, it will be truncated to fit the size of the TemplateString. If the
     * input string view is shorter than N, the remaining characters in the TemplateString will be default-initialized
     * (typically null characters).
     * @param sv The std::basic_string_view to construct the TemplateString from.
     */
    inline constexpr TemplateString(std::basic_string_view<T> sv)
    {
        if (sv.size() > N)
        {
            std::copy(sv.data(), sv.data() + N, this->str);
        }
        else
        {
            std::copy(sv.data(), sv.data() + sv.size(), this->str);
        }
    }
    /** @brief Returns an iterator to the beginning of the string.
     * @return An iterator to the beginning of the string.
     */
    inline constexpr T* begin() { return str; }
    /** @brief Returns an iterator to the end of the string.
     * @return An iterator to the end of the string.
     */
    inline constexpr T* end() { return str + N; }
    /** @brief Returns a constant iterator to the beginning of the string.
     * @return A constant iterator to the beginning of the string.
     */
    inline constexpr const T* begin() const { return str; }
    /** @brief Returns a constant iterator to the end of the string.
     * @return A constant iterator to the end of the string.
     */
    inline constexpr const T* end() const { return str + N; }
    /** @brief Returns the size of the string.
     * @return The size of the string.
     */
    inline constexpr size_t size() const { return N; }
    /** @brief Returns a reference to the character at the specified index.
     * @param index The index of the character to return.
     * @return A reference to the character at the specified index.
     */
    inline constexpr T& operator[](size_t index) { return str[index]; }
    /** @brief Returns a constant reference to the character at the specified index.
     * @param index The index of the character to return.
     * @return A constant reference to the character at the specified index.
     */
    inline constexpr const T& operator[](size_t index) const { return str[index]; }
    /** @brief Checks if the string is empty.
     * @return True if the string is empty, false otherwise.
     */
    inline constexpr bool empty() const { return N == 0; }
    /** @brief Compares two TemplateString objects for equality.
     * @param o The TemplateString to compare with.
     * @return True if the strings are equal, false otherwise.
     */
    template <size_t M>
    inline constexpr bool operator==(const TemplateString<T, M>& o) const
    {
        return static_cast<StringView>(*this) == static_cast<StringView>(o);
    }
    /** @brief Compares a TemplateString with a std::basic_string_view for equality.
     * @param o The std::basic_string_view to compare with.
     * @return True if the strings are equal, false otherwise.
     */
    inline constexpr bool operator==(const StringView& o) const { return static_cast<StringView>(*this) == o; }
    /** @brief Converts the TemplateString to a StringView.
     * @return The StringView representing the TemplateString.
     */
    inline explicit constexpr operator StringView() const
    {
        if (str[N - 1] == '\0')
        {
            return StringView{str};
        }
        return StringView{str, N};
    }
    /** @brief Converts the TemplateString to a String.
     * @return The String representing the TemplateString.
     */
    inline constexpr operator String() const
    {
        if (str[N - 1] == '\0')
        {
            return String{str};
        }
        return String{str, N};
    }
    /** @brief Converts the TemplateString to a pointer to the character array.
     * @return A pointer to the character array representing the TemplateString.
     */
    inline explicit constexpr operator const T*() const { return str; }
    /** @brief Converts the TemplateString to a pointer to the character array.
     * @return A pointer to the character array representing the TemplateString.
     */
    inline constexpr StringView ToStringView() const { return static_cast<StringView>(*this); }
    /** @brief Converts the TemplateString to a String.
     * @return The String representing the TemplateString.
     */
    inline constexpr String ToString() const { return static_cast<String>(*this); }
    /** @brief Converts the TemplateString to uppercase using the specified locale.
     * @param loc The locale to use for the conversion. If not specified, the default locale will be used.
     * @return A new TemplateString with all characters converted to uppercase.
     */
    inline TemplateString ToUpper(std::locale loc = std::locale()) const
    {
        return *this | std::views::transform([&loc](T c) { return std::toupper(c, loc); });
    }
    /** @brief Converts the TemplateString to lowercase using the specified locale.
     * @param loc The locale to use for the conversion. If not specified, the default locale will be used.
     * @return A new TemplateString with all characters converted to lowercase.
     */
    inline TemplateString ToLower(std::locale loc = std::locale()) const
    {
        return *this | std::views::transform([&loc](T c) { return std::tolower(c, loc); });
    }
    /** @brief Splits the TemplateString into a container of substrings based on a delimiter.
     * @param delimiter The delimiter to use for splitting the string.
     * @tparam Container The type of container to return, which must support emplace_back
     * @return A container of substrings resulting from splitting the TemplateString by the specified delimiter.
     */
    template <typename Container>
    inline constexpr Container Split(StringView delimiter) const
    {
        Container result;
        for (const auto& part : ToStringView() | std::views::split(delimiter))
        {
            result.emplace_back(part.begin(), part.end());
        }
        return result;
    }
    /** @brief Constructs a TemplateString from a range of characters defined by two iterators.
     * If the input range is larger than N, it will be truncated to fit the size of the TemplateString. If the input
     * range is smaller than N, the remaining characters in the TemplateString will be default-initialized (typically
     * null characters).
     * @param begin The iterator pointing to the beginning of the character range.
     * @param end The iterator pointing to the end of the character range.
     * @tparam It The type of the iterators, which must be input iterators.
     */
    template <typename It>
    constexpr TemplateString(It begin, It end)
    {
        It it = begin;
        for (size_t i = 0; i < N && i < end - begin; ++i)
        {
            str[i] = *it;
            ++it;
        }
    }
    /** @brief Constructs a TemplateString from a range of characters.
     * If the input range is larger than N, it will be truncated to fit the size of the TemplateString. If the input
     * range is smaller than N, the remaining characters in the TemplateString will be default-initialized (typically
     * null characters).
     * @param range The range of characters to initialize the TemplateString with.
     * @tparam R The type of the range, which must satisfy std::ranges::range.
     */
    template <std::ranges::range R>
    constexpr TemplateString(const R& range)
    {
        auto it = std::ranges::begin(range);
        for (size_t i = 0; i < N && it != std::ranges::end(range); ++i, ++it)
        {
            str[i] = *it;
        }
    }
    /** @brief Concatenates two TemplateString objects and returns a new TemplateString.
     * If the resulting string is larger than N, it will be truncated to fit the size of the TemplateString. If the
     * resulting string is smaller than N, the remaining characters in the TemplateString will be default-initialized
     * (typically null characters).
     * @param o The TemplateString to concatenate with.
     * @tparam M The size of the other TemplateString.
     * @return A new TemplateString resulting from concatenating the two TemplateStrings.
     */
    template <size_t M>
    constexpr TemplateString<T, M + N> operator+(const TemplateString<T, M>& o)
    {
        TemplateString<T, M + N> res;
        std::copy(str, str + N, res.str);
        auto n = ToStringView().size();
        std::copy(o.str, o.str + M, res.str + n);
        return res;
    }
    /** @brief Returns a reference to the character at the specified index with bounds checking.
     * If the index is out of range, an std::out_of_range exception will be thrown.
     * @param index The index of the character to return.
     * @return A reference to the character at the specified index.
     * @throws std::out_of_range if the index is out of range.
     */
    T& at(size_t index)
    {
        if (index >= N)
        {
            throw std::out_of_range("Index out of range");
        }
        return str[index];
    }
    /** @brief Returns a constant reference to the character at the specified index with bounds checking.
     * If the index is out of range, an std::out_of_range exception will be thrown.
     * @param index The index of the character to return.
     * @return A constant reference to the character at the specified index.
     * @throws std::out_of_range if the index is out of range.
     */
    const T& at(size_t index) const
    {
        if (index >= N)
        {
            throw std::out_of_range("Index out of range");
        }
        return str[index];
    }
    T str[N]{};
};

/** @brief Overloads the << operator for TemplateString objects.
 * @param os The output stream to write to.
 * @param ts The TemplateString to write.
 * @return The output stream.
 */
template <CharType T, size_t N>
std::basic_ostream<T>& operator<<(std::basic_ostream<T>& os, const TemplateString<T, N>& ts)
{
    return os << static_cast<std::basic_string_view<T>>(ts);
}

/**
 * @brief template call
 * @tparam Callback callback function
 * @tparam ...Args function arguments
 * @tparam th template helper
 * @param callback callback function
 * @param ...args arguments
 * @return callback return
 */
template <TemplateString th, typename Callback, typename... Args>
requires std::invocable<Callback, typename decltype(th)::StringView, Args...> inline auto
template_call(Callback&& callback, Args&&... args)
{
    return std::forward<Callback>(callback)(th.ToStringView(), std::forward<Args>(args)...);
}

/**
 * @brief invoke function
 * @tparam Callback callback function
 * @tparam ...Args function arguments
 * @param callback callback function
 * @param ...args callback arguments
 * @return callback return
 */
template <typename Callback, typename... Args>
requires std::invocable<Callback, Args...> inline constexpr auto Invoke(Callback&& callback, Args&&... args)
{
    return std::forward<Callback>(callback)(std::forward<Args>(args)...);
}

/**
 * @brief async invoke function
 * @tparam Callback callback function
 * @tparam ...Args function arguments
 * @param callback callback function
 * @param ...args callback arguments
 * @return future
 */
template <typename Callback, typename... Args>
requires std::invocable<Callback, Args...> inline auto AsyncInvoke(Callback&& callback, Args&&... args)
{
    return std::async(std::launch::async, std::forward<Callback>(callback), std::forward<Args>(args)...);
}

/**
 * @brief overloaded lambda
 * @tparam ...Ts lambda types
 */
template <typename... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};

template <typename... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

/**
 * @brief enum to string name, only support enum with continuous value, negative value will be ignored
 * @tparam EnumValue enum value
 * @return enum name
 */
template <auto EnumValue>
constexpr auto EnumName()
{
    return magic_enum::enum_name<EnumValue>();
}
/**
 * @brief enum to string name in function argument, only support enum with continuous value, negative value will be
 * ignored
 * @tparam T enum class
 * @param value enum value
 * @return enum name
 */
template <Enum T>
constexpr auto EnumName(T value)
{
    return magic_enum::enum_name<T>(value);
}

/**
 * @brief enum from string name, only support enum with continuous value, negative value will be ignored
 * @tparam T enum class
 * @param name enum name
 * @return enum value, if not found or negative value, return default value
 */
template <Enum T>
constexpr auto EnumFromName(std::string_view name)
{
    auto opt = magic_enum::enum_cast<T>(name);
    if (opt.has_value() && static_cast<std::underlying_type_t<T>>(opt.value()) >= 0)
    {
        return opt.value();
    }
    else
    {
        return T{};
    }
}

/** @brief Arrays of string views for different categories of enum names.
 */
constexpr std::array<std::string_view, 4> UnknownEnumNames = {"Unknown", "UNKNOWN", "unknown", "UnKnown"};
/** @brief Arrays of string views for undefined enum names.
 */
constexpr std::array<std::string_view, 4> UnDefinedEnumNames = {"Undefined", "UNDEFINED", "undefined", "UnDefined"};
/** @brief Arrays of string views for invalid enum names.
 */
constexpr std::array<std::string_view, 4> InvalidEnumNames = {"Invalid", "INVALID", "invalid", "InValid"};
/** @brief Arrays of string views for none enum names.
 */
constexpr std::array<std::string_view, 4> NoneEnumNames = {"None", "NONE", "none", "NoNe"};
/** @brief Arrays of string views for minimum enum names.
 */
constexpr std::array<std::string_view, 3> MinEnumNames = {"Min", "MIN", "min"};
/** @brief Arrays of string views for maximum enum names.
 */
constexpr std::array<std::string_view, 3> MaxEnumNames = {"Max", "MAX", "max"};

/** @brief Creates a table of valid enum values within a specified range.
 * @tparam T The enum type.
 * @tparam Min The minimum value of the range.
 * @tparam Max The maximum value of the range.
 * @return A table indicating which values in the range are valid enum values.
 */
template <typename T, int64_t Min = 0x00, int64_t Max = 0xff>
    requires Enum<T> && (Min <= Max) constexpr auto MakeEnumValidTable()
{
    std::array<bool, static_cast<size_t>(Max - Min + 1)> table{};
    constexpr auto enumEntries = magic_enum::enum_entries<T>();
    constexpr auto stringviewEndOf = [](std::string_view str, std::string_view suffix) -> bool
    { return str.size() >= suffix.size() && str.substr(str.size() - suffix.size(), suffix.size()) == suffix; };
    for (const auto& [value, _] : enumEntries)
    {
        auto intValue = static_cast<int64_t>(value);
        std::string_view name = magic_enum::enum_name<T>(value);
        // for invalid/undefined/unknown/min/max name, if the name contains any of the keyword, it will be considered as
        // invalid, this is to avoid the case where the enum value is valid but has a name that ends with the keyword,
        // which can cause confusion
        {
            if (std::ranges::any_of(UnknownEnumNames, [&](auto& n) { return stringviewEndOf(name, n); }) ||
                std::ranges::any_of(UnDefinedEnumNames, [&](auto& n) { return stringviewEndOf(name, n); }) ||
                std::ranges::any_of(InvalidEnumNames, [&](auto& n) { return stringviewEndOf(name, n); }) ||
                std::ranges::any_of(NoneEnumNames, [&](auto& n) { return stringviewEndOf(name, n); }) ||
                std::ranges::any_of(MinEnumNames, [&](auto& n) { return stringviewEndOf(name, n); }) ||
                std::ranges::any_of(MaxEnumNames, [&](auto& n) { return stringviewEndOf(name, n); }))
            {
                continue;
            }
        }
        if (intValue >= Min && intValue <= Max)
        {
            table[static_cast<size_t>(intValue - Min)] = true;
        }
    }
    return table;
}

/** @brief Checks if a given enum value is valid within a specified range and invokes a callback function if it is
 * valid. If the enum value is not valid, it returns a default-constructed value of the callback's return type (if it's
 * not void).
 * @tparam T The enum type.
 * @tparam Min The minimum value of the range.
 * @tparam Max The maximum value of the range.
 * @tparam Fn The type of the callback function to invoke if the enum value is valid.
 * @tparam Args The types of the arguments to pass to the callback function.
 * @param value The enum value to check for validity.
 * @param fn The callback function to invoke if the enum value is valid.
 * @param args The arguments to pass to the callback function.
 * @return The result of invoking the callback function if the enum value is valid, or a default-constructed value of
 * the callback's return type if it's not void and the enum value is not valid.
 */
template <typename T, int64_t Min = 0x00, int64_t Max = 0xff, typename Fn, typename... Args>
    requires Enum<T> && (Min <= Max) constexpr auto AllValidEnum(T value, Fn&& fn, Args&&... args)
{
    using ReturnType = std::invoke_result_t<Fn, Args...>;
    constexpr auto validTable = MakeEnumValidTable<T, Min, Max>();
    auto intValue = static_cast<int64_t>(value);
    if (intValue >= Min && intValue <= Max)
    {
        if (validTable[static_cast<size_t>(intValue - Min)])
        {
            return std::forward<Fn>(fn)(std::forward<Args>(args)...);
        }
    }
    if constexpr (!std::is_void_v<ReturnType>)
    {
        return ReturnType{};
    }
}

/** @brief A trait to check if all types in a parameter pack are the same.
 * @tparam ...Ts The types to check.
 */
template <typename T, typename... Us>
struct AllTheSame
{
    static_assert((std::is_same_v<T, Us> && ...));
    using type = T;
};

/** @brief A variable template to check if all types in a parameter pack are void.
 * @tparam ...Us The types to check.
 */
template <typename... Us>
constexpr bool HasVoidV = (std::is_void_v<Us> || ...);

/** @brief Checks if a given enum value is valid within a specified range and invokes a callback function if it is
 * valid. If the enum value is not valid, it returns a default-constructed value of the callback's return type (if it's
 * not void).
 * @tparam T The enum type.
 * @tparam Min The minimum value of the range.
 * @tparam Max The maximum value of the range.
 * @tparam Fn The type of the callback function to invoke if the enum value is valid.
 * @tparam DefaultFn The type of the default callback function to invoke if the enum value is not valid.
 * @tparam Args The types of the arguments to pass to the callback functions.
 * @param value The enum value to check for validity.
 * @param fn The callback function to invoke if the enum value is valid.
 * @param defaultFn The default callback function to invoke if the enum value is not valid.
 * @param args The arguments to pass to the callback functions.
 * @return The result of invoking the appropriate callback function.
 */
template <typename T, int64_t Min = 0x00, int64_t Max = 0xff, typename Fn, typename DefaultFn, typename... Args>
    requires Enum<T> &&
    (Min <= Max) constexpr auto AllValidEnum(T value, Fn&& fn, DefaultFn&& defaultFn, Args&&... args)
{
    using ReturnType = AllTheSame<std::invoke_result_t<Fn, Args...>, std::invoke_result_t<DefaultFn, Args...>>::type;
    constexpr auto validTable = MakeEnumValidTable<T, Min, Max>();
    auto intValue = static_cast<int64_t>(value);
    if (intValue >= Min && intValue <= Max)
    {
        if (validTable[static_cast<size_t>(intValue - Min)])
        {
            return std::forward<Fn>(fn)(std::forward<Args>(args)...);
        }
    }
    return std::forward<DefaultFn>(defaultFn)(std::forward<Args>(args)...);
}


/** @brief A template struct for named raw pointers, providing utility functions for accessing the pointer and its name.
 * This struct is designed to hold a raw pointer along with a compile-time string name, allowing for easier debugging
 * and code readability when working with raw pointers. It provides member functions to access the pointer, dereference
 * it, and check if it is valid (non-null). The specialization for void pointers includes a deprecated operator-> to
 * prevent dereferencing void pointers, which is not allowed in C++.
 */
template <TemplateString th, typename T>
struct NamedRawPtr
{
    T* ptr{nullptr};
    /** @brief Returns the name associated with the pointer as a string view.
     * @return The name of the pointer as a string view.
     */
    constexpr auto Name() const { return th.ToStringView(); }
    /** @brief The type of the value pointed to by the raw pointer. */
    using ValueType = T;
    /** @brief Returns the raw pointer.
     * @return The raw pointer.
     */
    const T* Get() const { return ptr; }
    /** @brief Returns the raw pointer.
     * @return The raw pointer.
     */
    T* Get() { return ptr; }
    /** @brief Dereferences the raw pointer.
     * @return A reference to the value pointed to by the raw pointer.
     */
    T* operator->() { return ptr; }
    /** @brief Dereferences the raw pointer.
     * @return A reference to the value pointed to by the raw pointer.
     */
    const T* operator->() const { return ptr; }
    /** @brief Dereferences the raw pointer.
     * @return A reference to the value pointed to by the raw pointer.
     */
    T& operator*() { return *ptr; }
    /** @brief Dereferences the raw pointer.
     * @return A reference to the value pointed to by the raw pointer.
     */
    const T& operator*() const { return *ptr; }
    /** @brief Checks if the raw pointer is valid (non-null).
     * @return True if the pointer is valid, false otherwise.
     */
    operator bool() const { return ptr != nullptr; }
};

template <TemplateString th>
struct NamedRawPtr<th, void>
{
    void* ptr = nullptr;
    constexpr auto Name() const { return th.ToStringView(); }
    using ValueType = void;
    const void* Get() const { return ptr; }
    void* Get() { return ptr; }
    /** @brief Dereferences the raw pointer.
     * @return A reference to the value pointed to by the raw pointer.
     */
    [[deprecated("void pointer cannot be dereferenced")]]
    void* operator->()
    {
        return ptr;
    }
    operator bool() const { return ptr != nullptr; }
};

/** @brief A template struct for named smart pointers, providing utility functions for accessing the pointer and its
 * name. This struct is designed to hold a smart pointer along with a compile-time string name, allowing for easier
 * debugging and code readability when working with smart pointers. It provides member functions to access the pointer,
 * dereference it, and check if it is valid (non-null).
 */
template <TemplateString th, typename T, template <typename> typename Ptr>
struct NamedPtr
{
    Ptr<T> ptr{nullptr};
    /** @brief Returns the name associated with the pointer as a string view.
     * @return The name of the pointer as a string view.
     */
    constexpr auto Name() const { return th.ToStringView(); }
    /** @brief The type of the value pointed to by the smart pointer. */
    using ValueType = T;
    /** @brief Returns the raw pointer managed by the smart pointer.
     * @return The raw pointer managed by the smart pointer.
     */
    const T* Get() const { return ptr.get(); }
    /** @brief Returns the raw pointer managed by the smart pointer.
     * @return The raw pointer managed by the smart pointer.
     */
    T* Get() { return ptr.get(); }
    /** @brief Dereferences the smart pointer.
     * @return A reference to the value pointed to by the smart pointer.
     */
    T* operator->() { return ptr.get(); }
    /** @brief Dereferences the smart pointer.
     * @return A reference to the value pointed to by the smart pointer.
     */
    const T* operator->() const { return ptr.get(); }
    /** @brief Dereferences the smart pointer.
     * @return A reference to the value pointed to by the smart pointer.
     */
    T& operator*() { return *ptr; }
    /** @brief Dereferences the smart pointer.
     * @return A reference to the value pointed to by the smart pointer.
     */
    const T& operator*() const { return *ptr; }
    /** @brief Checks if the smart pointer is valid (non-null).
     * @return True if the pointer is valid, false otherwise.
     */
    operator bool() const { return ptr != nullptr; }
};

#ifdef HSBA_NO_NTTP_FP
struct FloatTemplateArgs
{
    int32_t value;
    constexpr FloatTemplateArgs(float v) : value(std::bit_cast<int32_t>(v)) {}
    constexpr operator float() const { return std::bit_cast<float>(value); }
};
struct DoubleTemplateArgs
{
    int64_t value;
    constexpr DoubleTemplateArgs(double v) : value(std::bit_cast<int64_t>(v)) {}
    constexpr operator double() const { return std::bit_cast<double>(value); }
};

// long double is not supported in NTTP
#else
using FloatTemplateArgs = float;
using DoubleTemplateArgs = double;

#endif  // HSBA_NO_NTTP_FP

#ifndef __cpp_explicit_this_parameter
/** @brief A template struct for implementing the Y combinator, allowing for recursive lambda functions in C++.
 * This struct takes a callable object (such as a lambda) and provides an operator() that allows the callable to call
 * itself recursively by passing itself as the first argument. The Y combinator is a functional programming technique
 * that enables recursion without naming the function, making it useful for defining recursive behavior in anonymous
 * functions.
 */
template <typename Func>
struct YCombinator
{
    Func func;
    template <typename... Args>
    constexpr decltype(auto) operator()(Args&&... args) const
    {
        return func(*this, std::forward<Args>(args)...);
    }
};

template <typename Func>
YCombinator(Func) -> YCombinator<Func>;
#endif  // !__cpp_explicit_this_parameter

/** @brief Creates a non-copyable array of the specified type and size.
 * @tparam T The type of the elements in the array.
 * @tparam N The size of the array.
 * @tparam Args The types of the arguments for constructing the elements.
 * @param args The arguments for constructing the elements.
 * @return A non-copyable array of the specified type and size.
 */
template <typename T, size_t N, typename... Args>
requires(std::move_constructible<T> && !std::copy_constructible<T>) constexpr auto MakeNonCopyableArray(Args&&... args)
{
    return [&args...]<size_t... Is>(std::index_sequence<Is...>)
    { return std::array<T, N>{((void)Is, T{std::forward<Args>(args)...})...}; }(std::make_index_sequence<N>());
}

inline namespace TemplateStringLiterals
{
template <TemplateString s>
constexpr auto operator""_ts()
{
    return s;
}
}  // namespace TemplateStringLiterals
}  // namespace HsBa::Slicer::Utils

#include <format>
#include <iomanip>
#include <sstream>

template <typename C, size_t N>
struct std::formatter<HsBa::Slicer::Utils::TemplateString<C, N>, C>
{
    bool quoted = false;

    template <typename ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (*it == '#')
        {
            quoted = true;
            ++it;
        }
        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template <typename FmtContext>
    FmtContext::iterator format(HsBa::Slicer::Utils::TemplateString<C, N> s, FmtContext& ctx) const
    {
        std::basic_stringstream<C> out;
        if (quoted)
            out << std::quoted(s.ToStringView());
        else
            out << s;

        return std::ranges::copy(std::move(out).str(), ctx.out()).out;
    }
};
#endif  // !HSBA_SLICER_TEMPLATE_HELPHER_HPP
