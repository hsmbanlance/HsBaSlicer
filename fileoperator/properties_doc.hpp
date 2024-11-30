#pragma once

#ifndef HSBA_PROPERTIES_DOC_HPP
#define HSBA_PROPERTIES_DOC_HPP

#include <array>
#include <string_view>
#include <string>
#include <list>
#include <optional>
#include <concepts>
#include <exception>
#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace HsBa::Slicer::Config
{
	// not ptree style, simple json doc
	class PropertiesDoc final
	{
	public:
		PropertiesDoc() = default;
		bool FromJson(std::string_view path);
		bool Write(std::string_view path);
		std::optional<int> GetOptionalInt(std::string_view key) const noexcept;
		std::optional<double> GetOptionalDouble(std::string_view key) const noexcept;
		std::optional<bool> GetOptionBool(std::string_view key) const noexcept;
		std::optional<std::string> GetOptionString(std::string_view key) const noexcept;
		template<typename T>
		inline std::optional<T> GetOptionValue(std::string_view key) const noexcept
		{
			if (doc_.HasMember(key.data()))
			{
				const auto& json_item = doc_[key.data()];
				if (json_item.Is<T>())
				{
					return json_item.Get<T>();
				}
			}
			return std::nullopt;
		}
		std::optional<std::list<int>> GetOptionalIntArr(std::string_view key) const noexcept;
		std::optional<std::list<double>> GetOptionalDoubleArr(std::string_view key) const noexcept;
		std::optional<std::list<bool>> GetOptionalBoolArr(std::string_view key) const noexcept;
		std::optional<std::list<std::string>> GetOptionalStrArr(std::string_view key) const noexcept;
		template<typename T>
		inline std::optional<std::list<T>> GetOptionalStrArr(std::string_view key) const noexcept
		{
			if (doc_.HasMember(key.data()))
			{
				const auto& json_item = doc_[key.data()];
				if (json_item.IsArray())
				{
					std::list<T> res;
					for (const auto& i : json_item.GetArray())
					{
						if (i.Is<T>())
						{
							res.emplace_back(i.Get<T>());
						}
						else
						{
							return std::nullopt;
						}
					}
					return res;
				}
				return std::nullopt;
			}
			return std::nullopt;
		}

		void AddInt(std::string_view name, const int v);
		void AddDouble(std::string_view name, const double v);
		void AddBool(std::string_view name, const bool v);
		void AddString(std::string_view name, const std::string& v);
		template<typename T>
		inline void AddMember(std::string_view name, const T& v)
		{
			rapidjson::Value value;
			value.Set<T>(v);
			rapidjson::Value name_v;
			name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
			doc_.AddMember(name_v, value, doc_.GetAllocator());
		}
		void AddIntArr(std::string_view name, const std::list<int>& arr);
		void AddDoubleArr(std::string_view name, const std::list<double>& arr);
		void AddBoolArr(std::string_view name, const std::list<bool>& arr);
		void AddStringArr(std::string_view name, const std::list<std::string>& arr);
		template<typename T>
		inline void AddMemberArr(std::string_view name, const std::list<T>& arr)
		{
			rapidjson::Value name_v;
			name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
			auto& allocator = doc_.GetAllocator();
			rapidjson::Value value;
			for (const auto& i : arr)
			{
				rapidjson::Value i_v;
				i_v.Set<T>(i);
				value.PushBack(i_v, allocator);
			}
			doc_.AddMember(name_v, value, allocator);
		}

		const rapidjson::Value& GetValue(std::string_view key) const;
		rapidjson::Value& GetValue(std::string_view key);
		void AddValue(std::string_view name, rapidjson::Value& value);
	private:
		rapidjson::Document doc_;
	};
}// namespace HsBa::Slicer::Config
#endif // !HSBA_PROPERTIES_DOC_HPP
