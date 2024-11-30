#pragma once
#ifndef HSBA_SLICER_BOOST_STD_CONVERT_HPP
#define HSBA_SLICER_BOOST_STD_CONVERT_HPP

#include <variant>
#include <optional>

#include <boost/variant.hpp>
#include <boost/optional.hpp>
#include <boost/variant2.hpp>

namespace HsBa::Slicer::Utils
{
	template<typename T>
	inline std::optional<T> CopyToOptional(const boost::optional<T>& o)
	{
		if (o.has_value())
		{
			return std::optional<T>(o.value());
		}
		return std::nullopt;
	}
	template<typename T>
	inline boost::optional<T> CopyToBoostOptional(const std::optional<T>& o)
	{
		if (o.has_value())
		{
			return boost::optional<T>(o.value());
		}
		return boost::none;
	}

	template<typename... Args>
	inline boost::variant<Args...> CopyToBoostVariant(const std::variant<Args...>& o)
	{
		boost::variant<Args...> res;
		std::visit([&res](auto&& arg) {res = arg; }, o);
		return res;
	}
	template<typename... Args>
	inline boost::variant<Args...> CopyToBoostVariant(const boost::variant2::variant<Args...>& o)
	{
		boost::variant<Args...> res;
		boost::variant2::visit([&res](auto&& arg) {res = arg; }, o);
		return res;
	}

	template<typename... Args>
	inline std::variant<Args...> CopyToVariant(const boost::variant<Args...>& o)
	{
		std::variant<Args...> res;
		boost::apply_visitor([&res](auto&& arg) {res = arg; }, o);
		return res;
	}
	template<typename... Args>
	inline std::variant<Args...> CopyToVariant(const boost::variant2::variant<Args...>& o)
	{
		std::variant<Args...> res;
		boost::variant2::visit([&res](auto&& arg) {res = arg; }, o);
		return res;
	}

	template<typename... Args>
	inline boost::variant2::variant<Args...> CopyToBoostVariant2(const boost::variant<Args...>& o)
	{
		boost::variant2::variant<Args...> res;
		boost::apply_visitor([&res](auto&& arg) {res = arg; }, o);
		return res;
	}
	template<typename... Args>
	inline boost::variant2::variant<Args...> CopyToBoostVariant2(const std::variant<Args...>& o)
	{
		boost::variant2::variant<Args...> res;
		std::visit([&res](auto&& arg) {res = arg; }, o);
		return res;
	}
}// namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_BOOST_STD_CONVERT_HPP
