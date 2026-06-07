/** @file boost_std_convert.hpp
 * @brief A collection of conversion functions between Boost and standard library types.
 * @author HsBa
 * @date 2024-06-01
 */
#pragma once
#ifndef HSBA_SLICER_BOOST_STD_CONVERT_HPP
#define HSBA_SLICER_BOOST_STD_CONVERT_HPP

#include <optional>
#include <variant>

#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include <boost/variant2.hpp>

namespace HsBa::Slicer::Utils
{
/** @brief Converts a Boost optional to a standard optional.
 * @tparam T The type of the optional.
 * @param o The Boost optional to convert.
 * @return The converted standard optional.
 */
template <typename T>
inline std::optional<T> CopyToOptional(const boost::optional<T>& o)
{
    if (o.has_value())
    {
        return std::optional<T>(o.value());
    }
    return std::nullopt;
}
/** @brief Converts a standard optional to a Boost optional.
 * @tparam T The type of the optional.
 * @param o The standard optional to convert.
 * @return The converted Boost optional.
 */
template <typename T>
inline boost::optional<T> CopyToBoostOptional(const std::optional<T>& o)
{
    if (o.has_value())
    {
        return boost::optional<T>(o.value());
    }
    return boost::none;
}
/** @brief Converts a standard variant to a Boost variant.
 * @tparam Args The types of the variant.
 * @param o The standard variant to convert.
 * @return The converted Boost variant.
 */
template <typename... Args>
inline boost::variant<Args...> CopyToBoostVariant(const std::variant<Args...>& o)
{
    boost::variant<Args...> res;
    std::visit([&res](auto&& arg) { res = arg; }, o);
    return res;
}
/** @brief Converts a Boost variant2 to a Boost variant.
 * @tparam Args The types of the variant.
 * @param o The Boost variant2 to convert.
 * @return The converted Boost variant.
 */
template <typename... Args>
inline boost::variant<Args...> CopyToBoostVariant(const boost::variant2::variant<Args...>& o)
{
    boost::variant<Args...> res;
    boost::variant2::visit([&res](auto&& arg) { res = arg; }, o);
    return res;
}
/** @brief Converts a Boost variant to a standard variant.
 * @tparam Args The types of the variant.
 * @param o The Boost variant to convert.
 * @return The converted standard variant.
 */
template <typename... Args>
inline std::variant<Args...> CopyToVariant(const boost::variant<Args...>& o)
{
    std::variant<Args...> res;
    boost::apply_visitor([&res](auto&& arg) { res = arg; }, o);
    return res;
}
/** @brief Converts a Boost variant2 to a standard variant.
 * @tparam Args The types of the variant.
 * @param o The Boost variant2 to convert.
 * @return The converted standard variant.
 */
template <typename... Args>
inline std::variant<Args...> CopyToVariant(const boost::variant2::variant<Args...>& o)
{
    std::variant<Args...> res;
    boost::variant2::visit([&res](auto&& arg) { res = arg; }, o);
    return res;
}
/** @brief Converts a Boost variant to a Boost variant2.
 * @tparam Args The types of the variant.
 * @param o The Boost variant to convert.
 * @return The converted Boost variant2.
 */
template <typename... Args>
inline boost::variant2::variant<Args...> CopyToBoostVariant2(const boost::variant<Args...>& o)
{
    boost::variant2::variant<Args...> res;
    boost::apply_visitor([&res](auto&& arg) { res = arg; }, o);
    return res;
}
/** @brief Converts a standard variant to a Boost variant2.
 * @tparam Args The types of the variant.
 * @param o The standard variant to convert.
 * @return The converted Boost variant2.
 */
template <typename... Args>
inline boost::variant2::variant<Args...> CopyToBoostVariant2(const std::variant<Args...>& o)
{
    boost::variant2::variant<Args...> res;
    std::visit([&res](auto&& arg) { res = arg; }, o);
    return res;
}
}  // namespace HsBa::Slicer::Utils

#endif  // !HSBA_SLICER_BOOST_STD_CONVERT_HPP
