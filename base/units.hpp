#pragma once
#ifndef HSBA_SLICER_UNITS_HPP
#define HSBA_SLICER_UNITS_HPP

#include <boost/units/unit.hpp>
#include <boost/units/systems/si.hpp>
#include <boost/units/base_unit.hpp>
#include <boost/units/io.hpp>
#include <boost/units/systems/temperature/celsius.hpp>
#include <boost/units/systems/temperature/fahrenheit.hpp>
#include <boost/units/dimension.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace HsBa::Slicer::Units
{
	// currency enum
	enum class Currency
	{
		USD,
		EUR,
		JPY,
		CNY
	};

	struct currency_base_dimension : 
		boost::units::base_dimension<currency_base_dimension, 1> {};

	using currency_type = currency_base_dimension::dimension_type;

	template<Currency N>
	struct currency_base_unit :
		boost::units::base_unit<currency_base_unit<N>, currency_type, 1 + static_cast<int>(N)> 
	{
	};

	using usd_base_unit = currency_base_unit<Currency::USD>;
	using eur_base_unit = currency_base_unit<Currency::EUR>;
	using cny_base_unit = currency_base_unit<Currency::CNY>;
	using jpy_base_unit = currency_base_unit<Currency::JPY>;

	using usd_unit = usd_base_unit::unit_type;
	using eur_unit = eur_base_unit::unit_type;
	using cny_unit = cny_base_unit::unit_type;
	using jpy_unit = jpy_base_unit::unit_type;

	inline namespace literals
	{
		inline auto operator"" _USD(long double value)
		{
			return boost::multiprecision::cpp_dec_float_50(value) * usd_unit();
		}
		inline auto operator"" _USD(unsigned long long value)
		{
			return boost::multiprecision::cpp_dec_float_50(value) * usd_unit();
		}
		inline auto operator"" _EUR(long double value)
		{
			return boost::multiprecision::cpp_dec_float_50(value) * eur_unit();
		}
		inline auto operator"" _EUR(unsigned long long value)
		{
			return boost::multiprecision::cpp_dec_float_50(value) * eur_unit();
		}
		inline auto operator"" _CNY(long double value)
		{
			return boost::multiprecision::cpp_dec_float_50(value) * cny_unit();
		}
		inline auto operator"" _CNY(unsigned long long value)
		{
			return boost::multiprecision::cpp_dec_float_50(value) * cny_unit();
		}
		inline auto operator"" _JPY(long double value)
		{
			return boost::multiprecision::cpp_dec_float_50(value) * jpy_unit();
		}
		inline auto operator"" _JPY(unsigned long long value)
		{
			return boost::multiprecision::cpp_dec_float_50(value) * jpy_unit();
		}

		constexpr auto operator"" _kg(long double value)
		{
			return value * boost::units::si::kilogram;
		}
		constexpr auto operator"" _kg(unsigned long long value)
		{
			return static_cast<long double>(value) * boost::units::si::kilogram;
		}
		constexpr auto operator"" _m(long double value)
		{
			return value * boost::units::si::meter;
		}
		constexpr auto operator"" _m(unsigned long long value)
		{
			return static_cast<long double>(value) * boost::units::si::meter;
		}
		constexpr auto operator"" _m3(long double value)
		{
			return value * boost::units::si::cubic_meter;
		}
		constexpr auto operator"" _m3(unsigned long long value)
		{
			return static_cast<long double>(value) * boost::units::si::cubic_meter;
		}

		constexpr auto operator"" _L(long double value)
		{
			return value * 0.001l * boost::units::si::cubic_meter;
		}
		constexpr auto operator"" _L(unsigned long long value)
		{
			return static_cast<long double>(value) * 0.001l * boost::units::si::cubic_meter;
		}
		constexpr auto operator"" _mL(long double value)
		{
			return value * 0.000001l * boost::units::si::cubic_meter;
		}
		constexpr auto operator"" _mL(unsigned long long value)
		{
			return static_cast<long double>(value) * 0.000001l * boost::units::si::cubic_meter;
		}
		constexpr auto operator"" _mm(long double value)
		{
			return value * 0.001l * boost::units::si::meter;
		}
		constexpr auto operator"" _mm(unsigned long long value)
		{
			return static_cast<long double>(value) * 0.001l * boost::units::si::meter;
		}
		constexpr auto operator"" _cm(long double value)
		{
			return value * 0.01l * boost::units::si::meter;
		}
		constexpr auto operator"" _cm(unsigned long long value)
		{
			return static_cast<long double>(value) * 0.01l * boost::units::si::meter;
		}
		constexpr auto operator"" _km(long double value)
		{
			return value * 1000.0l * boost::units::si::meter;
		}
		constexpr auto operator"" _km(unsigned long long value)
		{
			return static_cast<long double>(value) * 1000.0l * boost::units::si::meter;
		}
		constexpr auto operator"" _g(long double value)
		{
			return value * 0.001l * boost::units::si::kilogram;
		}
		constexpr auto operator"" _g(unsigned long long value)
		{
			return static_cast<long double>(value) * 0.001l * boost::units::si::kilogram;
		}
		constexpr auto operator"" _mg(long double value)
		{
			return value * 0.000001l * boost::units::si::kilogram;
		}
		constexpr auto operator"" _mg(unsigned long long value)
		{
			return static_cast<long double>(value) * 0.000001l * boost::units::si::kilogram;
		}

		constexpr auto operator"" _kg_per_m3(long double value)
		{
			return value * boost::units::si::kilogram / boost::units::si::cubic_meter;
		}
		constexpr auto operator"" _kg_per_m3(unsigned long long value)
		{
			return static_cast<long double>(value) * boost::units::si::kilogram / boost::units::si::cubic_meter;
		}
		constexpr auto operator"" _g_per_cm3(long double value)
		{
			return value * 1000.0l * boost::units::si::kilogram / boost::units::si::cubic_meter;
		}
		constexpr auto operator"" _g_per_cm3(unsigned long long value)
		{
			return static_cast<long double>(value) * 1000.0l * boost::units::si::kilogram / boost::units::si::cubic_meter;
		}
		constexpr auto operator"" _kg_per_L(long double value)
		{
			return value * 1000.0l * boost::units::si::kilogram / boost::units::si::cubic_meter;
		}
		constexpr auto operator"" _kg_per_L(unsigned long long value)
		{
			return static_cast<long double>(value) * 1000.0l * boost::units::si::kilogram / boost::units::si::cubic_meter;
		}

		constexpr auto operator"" _C(long double value)
		{
			return value * boost::units::celsius::degrees;
		}
		constexpr auto operator"" _C(unsigned long long value)
		{
			return static_cast<long double>(value) * boost::units::celsius::degrees;
		}
		constexpr auto operator"" _F(long double value)
		{
			return value * boost::units::fahrenheit::degrees;
		}
		constexpr auto operator"" _F(unsigned long long value)
		{
			return static_cast<long double>(value) * boost::units::fahrenheit::degrees;
		}
		constexpr auto operator"" _K(long double value)
		{
			return value * boost::units::si::kelvin;
		}
		constexpr auto operator"" _K(unsigned long long value)
		{
			return static_cast<long double>(value) * boost::units::si::kelvin;
		}

		inline auto operator"" _USD_per_kg(long double value)
		{
			return value * usd_unit() / boost::units::si::kilogram;
		}
		inline auto operator"" _USD_per_kg(unsigned long long value)
		{
			return static_cast<long double>(value) * usd_unit() / boost::units::si::kilogram;
		}
		inline auto operator"" _EUR_per_kg(long double value)
		{
			return value * eur_unit() / boost::units::si::kilogram;
		}
		inline auto operator"" _EUR_per_kg(unsigned long long value)
		{
			return static_cast<long double>(value) * eur_unit() / boost::units::si::kilogram;
		}
		inline auto operator"" _CNY_per_kg(long double value)
		{
			return value * cny_unit() / boost::units::si::kilogram;
		}
		inline auto operator"" _CNY_per_kg(unsigned long long value)
		{
			return static_cast<long double>(value) * cny_unit() / boost::units::si::kilogram;
		}
		inline auto operator"" _JPY_per_kg(long double value)
		{
			return value * jpy_unit() / boost::units::si::kilogram;
		}
		inline auto operator"" _JPY_per_kg(unsigned long long value)
		{
			return static_cast<long double>(value) * jpy_unit() / boost::units::si::kilogram;
		}

		constexpr auto operator"" _s(long double value)
		{
			return value * boost::units::si::second;
		}
		constexpr auto operator"" _s(unsigned long long value)
		{
			return static_cast<long double>(value) * boost::units::si::second;
		}
		constexpr auto operator"" _min(long double value)
		{
			return value * 60.0l * boost::units::si::second;
		}
		constexpr auto operator"" _min(unsigned long long value)
		{
			return static_cast<long double>(value) * 60.0l * boost::units::si::second;
		}
		constexpr auto operator"" _h(long double value)
		{
			return value * 3600.0l * boost::units::si::second;
		}
		constexpr auto operator"" _h(unsigned long long value)
		{
			return static_cast<long double>(value) * 3600.0l * boost::units::si::second;
		}
		constexpr auto operator"" _ms(long double value)
		{
			return value * 0.001l * boost::units::si::second;
		}
		constexpr auto operator"" _ms(unsigned long long value)
		{
			return static_cast<long double>(value) * 0.001l * boost::units::si::second;
		}

		constexpr auto operator"" _J(long double value)
		{
			return value * boost::units::si::joule;
		}
		constexpr auto operator"" _J(unsigned long long value)
		{
			return static_cast<long double>(value) * boost::units::si::joule;
		}
		constexpr auto operator"" _kJ(long double value)
		{
			return value * 1000.0l * boost::units::si::joule;
		}
		constexpr auto operator"" _kJ(unsigned long long value)
		{
			return static_cast<long double>(value) * 1000.0l * boost::units::si::joule;
		}

		constexpr auto operator"" _kWh(long double value)
		{
			return value * 3600000.0l * boost::units::si::joule;
		}
		constexpr auto operator"" _kWh(unsigned long long value)
		{
			return static_cast<long double>(value) * 3600000.0l * boost::units::si::joule;
		}

	}

} // namespace HsBa::Slicer::Units

#endif // !HSBA_SLICER_UNITS_HPP
