#pragma once
#ifndef HSBA_SLICER_TUPLE_EACH_HPP
#define HSBA_SLICER_TUPLE_EACH_HPP

#include <tuple>

#include "template_helper.hpp"

namespace HsBa::Slicer::Utils
{
	template<typename Tuple, typename Fn, size_t... Is>
	constexpr auto TupleEachImpl(Fn&& fn,Tuple&& tuple, std::index_sequence<Is...>)
	{
		constexpr auto hasVoidReturn = HasVoidV<std::invoke_result_t<Fn, std::tuple_element_t<Is,std::remove_reference_t<Tuple>>...>>;
		if constexpr ()
		{
			(std::forward<Fn>(fn)(std::get<Is>(std::forward<Tuple>(tuple))), ...);
		}
		else
		{
			return std::make_tuple(std::forward<Fn>(fn)(std::get<Is>(std::forward<Tuple>(tuple)))...);
		}
	}
	template<typename Tuple, typename Fn,typename... Args>
	constexpr auto TupleEach(Fn&& fn,Tuple&& tuple,Args&&... args)
	{
		return TupleEachImpl([&](auto&& item) { fn(item, args...); }, std::forward<Tuple>(tuple), std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>{});
	}
}// namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_TUPLE_EACH_HPP