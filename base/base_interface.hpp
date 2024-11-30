#pragma once
#ifndef HSBA_SLICER_BASE_INTERFACE_HPP
#define HSBA_SLICER_BASE_INTERFACE_HPP

#include <string>

namespace HsBa::Slicer::Utils
{
	/// <summary>
	/// translator interface used in boost::ptree
	/// </summary>
	/// <typeparam name="T"></typeparam>
	template<typename T>
	class ITranslator
	{
	public:
		virtual std::string put_value(const T&) = 0;
		virtual T get_value(const std::string&) = 0;
	};
}// namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_BASE_INTERFACE_HPP