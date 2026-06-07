/**
 * @file struct_concepts.hpp
 * @brief Concepts for aggregate and reflectable types.
 *
 * Defines C++20 concepts to identify aggregate types and types that are compatible
 * with the static reflection system. These concepts are used to constrain template
 * functions for serialization, deserialization, and other utilities that operate
 * on structured data.
 *
 * @author HsBa Team
 */
#pragma once

#ifndef HSBA_SLICER_STRUCT_CONCEPTS_HPP
#define HSBA_SLICER_STRUCT_CONCEPTS_HPP

#include <concepts>
#include <fstream>
#include <iostream>
#include <type_traits>

#include "base/static_reflect.hpp"

namespace HsBa::Slicer::Utils
{
/**
 * @brief Concept for aggregate types.
 *
 * An aggregate type is a class or struct that has no user-declared constructors,
 * no private or protected non-static data members, no virtual functions, and no
 * virtual base classes. This concept is used to identify types that can be
 * initialized with aggregate initialization and can be introspected with Boost.PFR.
 *
 * @tparam T Candidate type.
 */
template <typename T>
concept Aggregte = std::is_aggregate_v<T> && !std::is_union_v<T>;

/**
 * @brief Concept for reflectable types.
 *
 * A reflectable type is a type that is compatible with the StaticReflect library,
 * meaning it has been registered with the reflection system and provides metadata
 * about its fields. This concept is used to identify types that can be serialized
 * and deserialized using the reflection-based utilities.
 *
 * @tparam T Candidate type.
 */
template <typename T>
concept Reflectable = StaticReflect::Reflectable<T>;
}  // namespace HsBa::Slicer::Utils

#endif  // !HSBA_SLICER_STRUCT_CONCEPTS_HPP