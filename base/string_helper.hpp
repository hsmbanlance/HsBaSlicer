#pragma once
#ifndef HSBA_SLICER_STRING_HELPER_HPP
#define HSBA_SLICER_STRING_HELPER_HPP

#include <string>
#include <string_view>
#include <algorithm>
#include <ranges>
#include <regex>

namespace HsBa::Slicer::Utils
{
	namespace detail
	{
		template<typename CharT>
		std::basic_string<CharT> _to_lower(std::basic_string_view<CharT> str, std::locale loc = std::locale())
		{
			std::basic_string<CharT> result(str);
			std::ranges::transform(result, result.begin(), [loc](CharT c) { return std::tolower(c,loc); });
			return result;
		}

		template<typename CharT>
		std::basic_string<CharT> _to_upper(std::basic_string_view<CharT> str,std::locale loc = std::locale())
		{
			std::basic_string<CharT> result(str);
			std::ranges::transform(result, result.begin(), [loc](CharT c) { return std::toupper(c,loc); });
			return result;
		}

		template<typename CharT>
		std::basic_string<CharT> _trim(std::basic_string_view<CharT> str)
		{
			auto is_space = [](CharT c) { return std::isspace(c); };
			auto first = std::find_if_not(str.begin(), str.end(), is_space);
			auto last = std::find_if_not(str.rbegin(), str.rend(), is_space).base();
			if (first >= last)
			{
				return std::basic_string<CharT>();
			}
			return std::basic_string<CharT>(first, last);
		}

		template<typename CharT>
		std::basic_string<CharT> _trim_left(std::basic_string_view<CharT> str)
		{
			auto is_space = [](CharT c) { return std::isspace(c); };
			auto first = std::find_if_not(str.begin(), str.end(), is_space);
			return std::basic_string<CharT>(first, str.end());
		}

		template<typename CharT>
		std::basic_string<CharT> _trim_right(std::basic_string_view<CharT> str)
		{
			auto is_space = [](CharT c) { return std::isspace(c); };
			auto last = std::find_if_not(str.rbegin(), str.rend(), is_space).base();
			return std::basic_string<CharT>(str.begin(), last);
		}

	} // namespace detail


	template<typename StringViewT>
	auto to_lower(StringViewT str, std::locale loc = std::locale()) -> std::basic_string<typename StringViewT::value_type>
	{
		using CharT = typename StringViewT::value_type;
		return detail::_to_lower<CharT>(str, loc);
	}

	template<typename StringViewT>
	auto to_upper(StringViewT str, std::locale loc = std::locale()) -> std::basic_string<typename StringViewT::value_type>
	{
		using CharT = typename StringViewT::value_type;
		return detail::_to_upper<CharT>(str, loc);
	}

	template<typename StringViewT, template<typename> typename Container = std::vector>
	constexpr Container<StringViewT> split(StringViewT str, StringViewT delimiter)
	{
		auto range = str | std::views::split(delimiter);
		Container<StringViewT> result;
		for (const auto& part : range)
		{
			result.emplace_back(part.begin(), part.end());
		}
		return result;
	}

	template<typename StringViewT, template<typename> typename Container = std::vector>
	auto regex_split(StringViewT str, StringViewT re) -> Container<std::basic_string<typename StringViewT::value_type>>
	{
		using CharT = typename StringViewT::value_type;
		using StringT = std::basic_string<CharT>;
		using IteratorT = typename std::regex_token_iterator<typename StringT::const_iterator>;
		std::basic_regex<CharT> rgx(re.data());
		if constexpr (std::is_same_v<StringViewT, StringT>)
		{
			IteratorT begin(str.begin(), str.end(), rgx, -1);
			IteratorT end;
			return Container<StringT>(begin, end);
		}
		else
		{
			StringT tmp(str);
			IteratorT begin(tmp.begin(), tmp.end(), rgx, -1);
			IteratorT end;
			return Container<StringT>(begin, end);
		}
	}


	template<typename StringViewT>
	auto trim(StringViewT str) -> std::basic_string<typename StringViewT::value_type>
	{
		using CharT = typename StringViewT::value_type;
		return detail::_trim<CharT>(str);
	}

	template<typename StringViewT>
	auto trim_left(StringViewT str) -> std::basic_string<typename StringViewT::value_type>
	{
		using CharT = typename StringViewT::value_type;
		return detail::_trim_left<CharT>(str);
	}

	template<typename StringViewT>
	auto trim_right(StringViewT str) -> std::basic_string<typename StringViewT::value_type>
	{
		using CharT = typename StringViewT::value_type;
		return detail::_trim_right<CharT>(str);
	}
} // namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_STRING_HELPER_HPP