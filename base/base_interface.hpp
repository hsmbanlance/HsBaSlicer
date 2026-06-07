/**
 * @file base_interface.hpp
 * @author HsBa
 * @date 2024-06-01
 */
#pragma once
#ifndef HSBA_SLICER_BASE_INTERFACE_HPP
#define HSBA_SLICER_BASE_INTERFACE_HPP

#include <string>

namespace HsBa::Slicer::Utils
{
/**
 * @brief A translator interface for converting between a type and its string representation.
 * @tparam T The type to be translated.
 */
template <typename T>
class ITranslator
{
public:
    virtual std::string put_value(const T&) = 0;
    virtual T get_value(const std::string&) = 0;
};
}  // namespace HsBa::Slicer::Utils

#endif  // !HSBA_SLICER_BASE_INTERFACE_HPP