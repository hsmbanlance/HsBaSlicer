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
	template<typename T, size_t N>
	struct Template_Helper
	{
		inline constexpr Template_Helper(const T(&arr)[N])
		{
			std::copy(arr, arr + N, this->arr);
		}
		T arr[N];
	};


	/// <summary>
	/// template call
	/// </summary>
	/// <typeparam name="Callback">callback function</typeparam>
	/// <typeparam name="...Args">function arguments</typeparam>
	/// <typeparam name="th">template helper</typeparam>
	/// <param name="callback">callback function</param>
	/// <param name="...args">arguments</param>
	/// <returns>callback return</returns>
	template<Template_Helper th, typename Callback, typename... Args>
		requires std::invocable<Callback, decltype(th.arr), Args...>
	inline auto template_call(Callback&& callback, Args&&... args)
	{
		return std::forward<Callback>(callback)(th.arr, std::forward<Args>(args)...);
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

}// namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_TEMPLATE_HELPHER_HPP
