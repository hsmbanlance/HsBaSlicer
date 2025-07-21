#pragma once
#ifndef HSBA_SLICER_TEMPLATE_HELPHER_HPP
#define HSBA_SLICER_TEMPLATE_HELPHER_HPP

#include <utility>
#include <algorithm>
#include <type_traits>
#include <concepts>
#include <future>
#include <set>
#include <vector>
#include <list>
#include <unordered_set>
#include <optional>
#include <exception>
#include <ranges>
#include <array>

#if __cpp_lib_coroutine && __cpp_impl_coroutine
#include <coroutine>
#endif
#ifdef __cpp_lib_generator
#include <generator>
#endif // __cpp_lib_generator

#include "concepts.hpp"
#include "error.hpp"

namespace HsBa::Slicer::Utils
{
	template<CharType T, size_t N>
	struct TemplateString
	{
		using Char = T;
		using StringView = std::basic_string_view<T>;
		using String = std::basic_string<T>;
		inline constexpr TemplateString() = default;
		inline constexpr TemplateString(const T(&str)[N])
		{
			std::copy(str, str + N, this->str);
		}
		template<size_t M>
		inline constexpr TemplateString(const T(&str)[M])
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
		inline constexpr TemplateString(const std::array<T, N>& arr)
		{
			std::copy(arr.begin(), arr.end(), this->str);
		}
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
		inline constexpr T* begin() { return str; }
		inline constexpr T* end() { return str + N; }
		inline constexpr const T* begin() const { return str; }
		inline constexpr const T* end() const { return str + N; }
		inline constexpr size_t size() const { return N; }
		inline constexpr T& operator[](size_t index) { return str[index]; }
		inline constexpr const T& operator[](size_t index) const { return str[index]; }
		inline constexpr bool empty() const { return N == 0; }
		template<size_t M>
		inline constexpr bool operator==(const TemplateString<T, M>& o) const
		{
			return static_cast<StringView>(*this) == static_cast<StringView>(o);
		}
		inline constexpr bool operator==(const StringView& o) const
		{
			return static_cast<StringView>(*this) == o;
		}
		inline explicit constexpr operator StringView() const
		{
			if(str[N - 1] == '\0')
			{
				return StringView{ str};
			}
			return StringView{ str, N };
		}
		inline explicit constexpr operator String() const
		{
			if(str[N - 1] == '\0')
			{
				return String{ str };
			}
			return String{ str, N };
		}
		inline explicit constexpr operator T*() const
		{
			return str;
		}
		inline constexpr StringView ToStringView() const
		{
			return static_cast<StringView>(*this);
		}
		inline constexpr String ToString() const
		{
			return static_cast<String>(*this);
		}
		inline TemplateString ToUpper(std::locale loc = std::locale()) const
		{
			return *this | std::views::transform([&loc](T c) { return std::toupper(c, loc); });
		}
		inline TemplateString ToLower(std::locale loc = std::locale()) const
		{
			return *this | std::views::transform([&loc](T c) { return std::tolower(c, loc); });
		}
		template<typename Container>
		inline constexpr Container Split(StringView delimiter) const
		{
			Container result;
			for (const auto& part : ToStringView() | std::views::split(delimiter))
			{
				result.emplace_back(part.begin(), part.end());
			}
			return result;
		}
		template<typename It>
		constexpr TemplateString(It begin, It end)
		{
			It it = begin;
			for (size_t i = 0; i < N && i < end - begin; ++i)
			{
				str[i] = *it;
				++it;
			}
		}
		template<size_t M>
		constexpr TemplateString<T, M + N> operator+(const TemplateString<T, M>& o)
		{
			TemplateString<T, M + N> res;
			std::copy(str, str + N, res.str);
			auto n = ToStringView().size();
			std::copy(o.str, o.str + M, res.str + n);
			return res;
		}
		T str[N]{};
	};

	template<CharType T, size_t N>
	std::basic_ostream<T>& operator<<(std::basic_ostream<T>& os, const TemplateString<T, N>& ts)
	{
		return os << static_cast<std::basic_string_view<T>>(ts);
	}

	/// <summary>
	/// template call
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="...Args">function arguments</typeparam>
	/// <typeparam name="th">template helper</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="...args">arguments</param>
	/// <returns>callback return</returns>
	template<TemplateString th, typename Callback, typename... Args>
		requires std::invocable<Callback, typename decltype(th)::StringView, Args...>
	inline auto template_call(Callback&& callback, Args&&... args)
	{
		return std::forward<Callback>(callback)(th.ToStringView(), std::forward<Args>(args)...);
	}

	/// <summary>
	/// invoke function
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="...Args">function arguments</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="...args">callback arguments</param>
	/// <returns>callback return</returns>
	template<typename Callback, typename... Args>
		requires std::invocable<Callback, Args...>
	inline constexpr auto Invoke(Callback&& callback, Args&&... args)
	{
		return std::forward<Callback>(callback)(std::forward<Args>(args)...);
	}

	/// <summary>
	/// async invoke function
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="...Args">function arguments</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="...args">callback arguments</param>
	/// <returns>future</returns>
	template<typename Callback, typename... Args>
		requires std::invocable<Callback, Args...>
	inline auto AsyncInvoke(Callback&& callback, Args&&... args)
	{
		return std::async(std::launch::async, std::forward<Callback>(callback), std::forward<Args>(args)...);
	}

	template<typename... Ts>
	struct Overloaded : Ts...
	{
		using Ts::operator()...;
	};

	template<typename... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;
	
	/// <summary>
	/// enum to string name with template, only support enum with continuous value
	/// </summary>
	/// <typeparam name="T">enum value</typeparam>
	/// <returns>enum name</returns>
	template<auto T>
	constexpr auto EnumName()
	{
		std::string_view name;
#if __GNUC__ || __clang__
		name = __PRETTY_FUNCTION__;
		size_t start = name.find('=') + 2;
		size_t end = name.size() - 1;
		name = std::string_view{ name.data() + start, end - start };
		start = name.rfind("::");
#elif _MSC_VER
		name = __FUNCSIG__;
		size_t start = name.find('<') + 1;
		size_t end = name.rfind(">(");
		name = std::string_view{ name.data() + start, end - start };
		start = name.rfind("::");
#endif
		return start == std::string::npos ? name : std::string_view{
			name.data() + start + 2, name.size() - start - 2 };
	}

	/// <summary>
	/// enum max value, only support enum with continuous value, negative value will be ignored
	/// </summary>
	/// <typeparam name="T">enum class</typeparam>
	/// <typeparam name="N">ingore</typeparam>
	/// <returns>enum max value</returns>
	template<Enum T, size_t N = 0>
	constexpr auto EnumMax()
	{
		constexpr auto value = static_cast<T>(N);
		if constexpr (EnumName<value>().find(")") == std::string_view::npos)
		{
			return EnumMax<T, N + 1>();
		}
		else
		{
			return N;
		}
	}

	/// <summary>
	/// enum to string name in function argument, only support enum with continuous value, negative value will be ignored
	/// </summary>
	/// <typeparam name="T">enum class</typeparam>
	/// <param name="value">enum value</param>
	/// <returns>enum name</returns>
	template<Enum T>
	constexpr auto EnumName(T value)
	{
		constexpr auto num = EnumMax<T>();
		constexpr auto names = []<size_t... Is>(std::index_sequence<Is...>)
		{
			return std::array<std::string_view, num>{ EnumName<static_cast<T>(Is)>()... };
		}(std::make_index_sequence<num>{});
		return names[static_cast<size_t>(value)];
	}

	/// <summary>
	/// enum from string name, only support enum with continuous value, negative value will be ignored
	/// </summary>
	/// <typeparam name="T">enum class</typeparam>
	/// <param name="name">enum name</param>
	/// <returns>enum value, if not found or negative value, return default value</returns>
	template<Enum T>
	constexpr auto EnumFromName(std::string_view name)
	{
		constexpr auto num = EnumMax<T>();
		constexpr auto names = []<size_t... Is>(std::index_sequence<Is...>)
		{
			return std::array<std::string_view, num>{ EnumName<static_cast<T>(Is)>()... };
		}(std::make_index_sequence<num>{});
		for (size_t i = 0; i < num; i++)
		{
			if (names[i] == name)
			{
				return static_cast<T>(i);
			}
		}
		return T{};
	}

	template<typename T, typename... Us>
	struct AllTheSame {
		static_assert((std::is_same_v<T, Us> && ...));
		using type = T;
	};

	template<typename... Us>
	constexpr bool HasVoidV = (std::is_void_v<Us> || ...);
}// namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_TEMPLATE_HELPHER_HPP
