#pragma once
#ifndef HSBA_SLICER_CONCEPTS_HPP

#include <string>
#include <string_view>
#include <concepts>
#include <iostream>
#include <type_traits>

namespace HsBa::Slicer
{
	/**
	 * @brief streamable type,which can be used in stream operator,include input and output streamstreamable type,which can be used in stream operator,include input and output stream
	 */
	template<typename T>
	concept CharStream = requires(T t, std::ostream & os, std::istream & is) {
		{ os << t };
		{ is >> t };
	};

	/**
	 * @brief wstreamable type,which can be used in stream operator,include input and output stream
	 */
	template<typename T>
	concept WCharStream = requires(T t, std::wostream & os, std::wistream & is) {
		{ os << t };
		{ is >> t };
	};

	/**
	 * @brief put_value and get_value member function,for boost::ptree
	 */
	template<typename T, typename Tran>
	concept StrTranslator = requires(T t, std::string str, Tran tr) {
		{ tr.put_value(t) }->std::convertible_to<std::string>;
		{ tr.get_value(str) }->std::same_as<T>;
	};

	/**
	 * @brief is enumeration type
	 */
	template<typename T>
	concept Enum = std::is_enum_v<T>;

	/**
	 * @brief is c style pointer
	 */
	template<typename T>
	concept CStylePointer = std::is_pointer_v<T>;
	/**
	 * @brief can be dereferenced
	 */
	template<typename T>
	concept PointerOrIt = requires(T t) {
		{ *t };
	};

	template<typename T>
	concept PointerLike = requires(T t) {
		{ t.operator->() } -> std::same_as<T*>;
		{ t.operator*() } -> std::same_as<T&>;
		{ t.operator bool() } -> std::convertible_to<bool>;
	};

	template<typename T>
	concept OptionalLike = requires(T t) {
		{ t.has_value() } -> std::convertible_to<bool>;
		{ *t } -> std::same_as<typename T::value_type&>;
		{ t.value() } -> std::same_as<typename T::value_type&>;
	};

	/**
	 * @brief is c style array
	 */
	template<typename T>
	concept CStyleArray = std::is_array_v<T>;

	/**
	 * @brief left value reference
	 */
	template<typename T>
	concept LvalueRef = std::is_lvalue_reference_v<T>;
	/**
	 * @brief right value reference
	 */
	template<typename T>
	concept RvalueRef = std::is_rvalue_reference_v<T>;
	/**
	 * @brief is reference
	 */
	template<typename T>
	concept Ref = std::is_reference_v<T>;

	/**
	 * @brief is standard arithmetic type
	 */
	template<typename T>
	concept StandardArithmetic = std::is_arithmetic_v<T>;

	/**
	 * @brief is c style union
	 */
	template<typename T>
	concept CStypleUnion = std::is_union_v<T>;

	/**
	 * @brief has std::hash
	 */
	template<typename T>
	concept StdHash = requires(T t, std::hash<T> hasher)
	{
		{ hasher(t) }->std::convertible_to<size_t>;
	};
	/**
	 * @brief Hasher is T hasher, like std::hash
	 */
	template<typename T, typename Hasher>
	concept Hash = requires(T t, Hasher hasher)
	{
		{ hasher(t) }->std::convertible_to<size_t>;
	};
	/**
	 * @brief has std::hash and operator==
	 */
	template<typename T>
	concept EqualAndStdHash = std::equality_comparable<T> && StdHash<T>;
	/**
	 * @brief T has operator== and Hasher is T hasher, like std::hash
	 */
	template<typename T, typename Hasher>
	concept EqualAndHash = std::equality_comparable<T> && Hash<T, Hasher>;

	/**
	 * @brief std string cotainer
	 */
	template<typename T>
	concept StdString = std::same_as<T, std::string> || std::same_as<T, std::wstring>
		|| std::same_as<T, std::u16string> || std::same_as<T, std::u32string> ||
		std::same_as<T, std::u8string>;
	/**
	 * @brief const char* or const wchar_t* or const char16_t* or const char32_t* or const char8_t*
	 */
	template<typename T>
	concept CStyleString = std::same_as<T, const char*> || std::same_as<T, const wchar_t*>
		|| std::same_as<T, const char16_t*> || std::same_as<T, const char32_t*> ||
		std::same_as<T, const char8_t*>;
	/**
	 * @brief std string container or c style string
	 */
	template<typename T>
	concept String = StdString<T> || CStyleString<T>;

	/**
	 * @brief std string view
	 */
	template<typename T>
	concept StringView = std::same_as<T, std::string_view> || std::same_as<T, std::wstring_view> ||
		std::same_as<T, std::u16string_view> || std::same_as<T, std::u32string_view> ||
		std::same_as<T, std::u8string_view>;

	/**
	 * @brief allocator for T
	 */
	template<typename T, typename Allocator>
	concept TAllocator = requires(Allocator alloc, std::size_t n, T * p) {
		{ alloc.allocate(n) } -> std::same_as<T*>;
		{ alloc.deallocate(p, n) };
			requires std::is_constructible_v<Allocator>;
			requires std::is_destructible_v<Allocator>;
	};

	template<typename T>
	concept CharType = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, signed char> || std::is_same_v<T, unsigned char>
		|| std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t> || std::is_same_v<T, char8_t>;

	template<typename T>
	concept Addable = requires(T t, T u) {
		{ t + u } -> std::same_as<T>;
	};

	template<typename T>
	concept Subtractable = requires(T t, T u) {
		{ t - u } -> std::same_as<T>;
	};

	template<typename T>
	concept Multipliable = requires(T t, T u) {
		{ t * u } -> std::same_as<T>;
	};

	template<typename T>
	concept Dividable = requires(T t, T u) {
		{ t / u } -> std::same_as<T>;
	};
}// namespace HsBa::Slicer

#endif // !HSBA_SLICER_CONCEPTS_HPP
