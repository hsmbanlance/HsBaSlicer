# pragma once

#ifndef HSBA_SLICER_STRUCT_CONCEPTS_HPP

#include <type_traits>
#include <concepts>
#include <iostream>
#include <fstream>

#include "base/static_reflect.hpp"

namespace HsBa::Slicer::Utils
{
	template<typename T>
	concept Aggregte = std::is_aggregate_v<T> && !std::is_union_v<T>;

    	// Reflectable: wrapper concept pointing to the static reflect system
	template<typename T>
	concept Reflectable = StaticReflect::Reflectable<T>;
} // namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_STRUCT_CONCEPTS_HPP