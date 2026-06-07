/** @file tuple_each.hpp
 * @brief A header file containing the definition of a utility function for applying a function to each element of a tuple.
 * This file defines a utility function `TupleEach` that allows you to apply a given function to each element of a tuple. The function can be used with tuples of any size and can handle functions that return void or non-void types. The implementation uses C++20 features such as fold expressions and index sequences to achieve this functionality in a concise and efficient manner.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_TUPLE_EACH_HPP
#define HSBA_SLICER_TUPLE_EACH_HPP

#include <tuple>

#include "template_helper.hpp"

namespace HsBa::Slicer::Utils
{
template <typename Tuple, typename Fn, size_t... Is>
constexpr auto TupleEachImpl(Fn&& fn, Tuple&& tuple, std::index_sequence<Is...>)
{
    if constexpr ((std::is_void_v<decltype(std::forward<Fn>(fn)(std::get<Is>(std::forward<Tuple>(tuple))))> && ...))
    {
        (std::forward<Fn>(fn)(std::get<Is>(std::forward<Tuple>(tuple))), ...);
    }
    else
    {
        return std::make_tuple(std::forward<Fn>(fn)(std::get<Is>(std::forward<Tuple>(tuple)))...);
    }
}
/** @brief Apply a function to each element of a tuple.
 * @param fn The function to apply.
 * @param tuple The tuple to iterate over.
 * @param args The additional arguments to pass to the function.
 * @return The result of applying the function to each element of the tuple.
 */
template <typename Tuple, typename Fn, typename... Args>
constexpr auto TupleEach(Fn&& fn, Tuple&& tuple, Args&&... args)
{
    return TupleEachImpl([&](auto&& item) { fn(item, args...); }, std::forward<Tuple>(tuple),
                         std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>{});
}
}  // namespace HsBa::Slicer::Utils

#endif  // !HSBA_SLICER_TUPLE_EACH_HPP