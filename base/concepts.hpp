#pragma once
#ifndef HSBA_SLICER_CONCEPTS_HPP

#include <string>
#include <string_view>
#include <concepts>
#include <iostream>
#include <type_traits>

namespace HsBa::Slicer
{
	/// <summary>
	/// streamable type,which can be used in stream operator,include input and output stream
	/// </summary>
	template<typename T>
	concept CharStream = requires(T t, std::ostream & os, std::istream & is) {
		{ os << t };
		{ is >> t };
	};

	/// <summary>
	/// wstreamable type,which can be used in stream operator,include input and output stream
	/// </summary>
	template<typename T>
	concept WCharStream = requires(T t, std::wostream & os, std::wistream & is) {
		{ os << t };
		{ is >> t };
	};

	/// <summary>
	/// put_value and get_value member function,for boost::ptree
	/// </summary>
	template<typename T, typename Tran>
	concept StrTranslator = requires(T t, std::string str, Tran tr) {
		{ tr.put_value(t) }->std::convertible_to<std::string>;
		{ tr.get_value(str) }->std::same_as<T>;
	};

	/// <summary>
	/// is enumeration type
	/// </summary>
	template<typename T>
	concept Enum = std::is_enum_v<T>;

	/// <summary>
	/// is c style pointer
	/// </summary>
	template<typename T>
	concept CStylePointer = std::is_pointer_v<T>;
	/// <summary>
	/// can be dereferenced
	/// </summary>
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

	/// <summary>
	/// is c style array
	/// </summary>
	template<typename T>
	concept CStyleArray = std::is_array_v<T>;

	/// <summary>
	/// left value reference
	/// </summary>
	template<typename T>
	concept LvalueRef = std::is_lvalue_reference_v<T>;
	/// <summary>
	/// right value reference
	/// </summary>
	template<typename T>
	concept RvalueRef = std::is_rvalue_reference_v<T>;
	/// <summary>
	/// is reference
	/// </summary>
	template<typename T>
	concept Ref = std::is_reference_v<T>;

	/// <summary>
	/// is standard arithmetic type
	/// </summary>
	template<typename T>
	concept StandardArithmetic = std::is_arithmetic_v<T>;

	/// <summary>
	/// is c style union
	/// </summary>
	template<typename T>
	concept CStypleUnion = std::is_union_v<T>;

	/// <summary>
	/// has std::hash
	/// </summary>
	template<typename T>
	concept StdHash = requires(T t, std::hash<T> hasher)
	{
		{ hasher(t) }->std::convertible_to<size_t>;
	};
	/// <summary>
	/// Hasher is T hasher, like std::hash
	/// </summary>
	template<typename T, typename Hasher>
	concept Hash = requires(T t, Hasher hasher)
	{
		{ hasher(t) }->std::convertible_to<size_t>;
	};
	/// <summary>
	/// has std::hash and operator==
	/// </summary>
	template<typename T>
	concept EqualAndStdHash = std::equality_comparable<T> && StdHash<T>;
	/// <summary>
	/// T has operator== and Hasher is T hasher, like std::hash
	/// </summary>
	template<typename T, typename Hasher>
	concept EqualAndHash = std::equality_comparable<T> && Hash<T, Hasher>;

	/// <summary>
	/// std string cotainer
	/// </summary>
	template<typename T>
	concept StdString = std::same_as<T, std::string> || std::same_as<T, std::wstring>
		|| std::same_as<T, std::u16string> || std::same_as<T, std::u32string> ||
		std::same_as<T, std::u8string>;
	/// <summary>
	/// const char* or const wchar_t* or const char16_t* or const char32_t* or const char8_t*
	/// </summary>
	template<typename T>
	concept CStyleString = std::same_as<T, const char*> || std::same_as<T, const wchar_t*>
		|| std::same_as<T, const char16_t*> || std::same_as<T, const char32_t*> ||
		std::same_as<T, const char8_t*>;
	/// <summary>
	/// std string container or c style string
	/// </summary>
	template<typename T>
	concept String = StdString<T> || CStyleString<T>;

	/// <summary>
	/// std string view
	/// </summary>
	template<typename T>
	concept StringView = std::same_as<T, std::string_view> || std::same_as<T, std::wstring_view> ||
		std::same_as<T, std::u16string_view> || std::same_as<T, std::u32string_view> ||
		std::same_as<T, std::u8string_view>;

	/// <summary>
	/// allocator for T
	/// </summary>
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
