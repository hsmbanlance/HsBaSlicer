#pragma once
#ifndef HSBA_SLICER_RW_PRTREE_HPP
#define HSBA_SLICER_RW_PRTREE_HPP

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <any>
#include <optional>

#include <boost/property_tree/ptree.hpp>

#include "base/concepts.hpp"
#include "base/error.hpp"
#include "base/any_visit.hpp"

namespace HsBa::Slicer::Config
{
	boost::property_tree::ptree from_ini(const std::string& path);
	boost::property_tree::ptree from_xml(const std::string& path);
	boost::property_tree::ptree from_json(const std::string& path);

	//error boost::property_tree::file_parser_error
	void to_ini(const std::string& path, const boost::property_tree::ptree& ptree);
	//error boost::property_tree::file_parser_error
	void to_xml(const std::string& path, const boost::property_tree::ptree& ptree);
	//error boost::property_tree::file_parser_error
	void to_json(const std::string& path, const boost::property_tree::ptree& ptree);

	class IConfigMap
	{
	public:
		virtual ~IConfigMap() = default;
	};

	class AnyConfigMap;

	template<typename... Args>
	class VariantConfigMap : public IConfigMap
	{
	public:
		virtual ~VariantConfigMap() = default;
		template<typename T>
		inline std::optional<T> GetOptional(const std::string& key) const noexcept
		{
			if (config_map_.contains(key))
			{
				try
				{
					return std::get<T>(config_map_.at(key));
				}
				catch(const std::bad_variant_access&)
				{
					return std::nullopt;
				}
			}
			else
			{
				return std::nullopt;
			}
		}
		template<typename T>
		inline bool AddOrChangeValue(const std::string& key, T value)
		{
			if (config_map_.contains(key))
			{
				try
				{
					T v = std::get<T>(config_map_[key]);
					config_map_[key] = value;
					return true;
				}
				catch (const std::bad_variant_access&)
				{
					return false;
				}
			}
			config_map_[key] = value;
			return true;
		}
		inline boost::property_tree::ptree ToPtree() const
		{
			boost::property_tree::ptree ptree;
			for (const auto& [key, value] : config_map_)
			{
				std::visit([&key, &ptree](auto&& arg) {ptree.add(key, arg); }, value);
			}
			return ptree;
		}
		template<typename T>
		inline bool AddValueInPtree(const boost::property_tree::ptree& ptree, const std::string& key)
		{
			T value = ptree.get<T>(key);
			return AddOrChangeValue(key, value);
		}
		template<typename T,typename Translator>
		inline bool AddValueInPtree(const boost::property_tree::ptree& ptree, const std::string& key, Translator tr)
		{
			T value = ptree.get<T>(key, T{}, tr);
			return AddOrChangeValue(key, value);
		}
        template<typename T = AnyConfigMap>
        auto ToAnyMap() const ->std::enable_if_t<std::is_same_v<T,AnyConfigMap>,AnyConfigMap>
        {
            T map;
		    for (const auto& [key, value] : config_map_)
		    {
			    std::visit([&key, &map](auto&& args) {map.AddOrChangeValue(key, args); }, value);
		    }
		    return map;
        }
	private:
		std::unordered_map<std::string, std::variant<Args...>> config_map_;
	};

	class AnyConfigMap : public IConfigMap
	{
	public:
		virtual ~AnyConfigMap() = default;
		template<typename T>
		inline std::optional<T> GetOptional(const std::string& key) const noexcept
		{
			if (config_map_.contains(key))
			{
				try
				{
					T value = std::any_cast<T>(config_map_.at(key));
					return value;
				}
				catch (const std::bad_any_cast&)
				{
					return std::nullopt;
				}
			}
			return std::nullopt;
		}
		template<typename T>
		inline bool AddOrChangeValue(const std::string& key, T value)
		{
			if (config_map_.contains(key))
			{
				if (config_map_[key].type() == typeid(T))
				{
					config_map_[key] = value;
					return true;
				}
				return false;
			}
			else
			{
				config_map_[key] = value;
				return true;
			}
		}
		template<typename... Args>
		inline boost::property_tree::ptree ToPtree() const
		{
			boost::property_tree::ptree ptree;
			for (const auto& [key, value] : config_map_)
			{
				Utils::Visit<Args...>([&key, &ptree](auto&& arg) {ptree.add(key, arg); }, value);
			}
			return ptree;
		}
		template<typename T>
		inline bool AddValueInPtree(const boost::property_tree::ptree& ptree, const std::string& key)
		{
			T value = ptree.get<T>(key);
			return AddOrChangeValue(key, value);
		}
		template<typename T, typename Translator>
		inline bool AddValueInPtree(const boost::property_tree::ptree& ptree, const std::string& key, Translator tr)
		{
			T value = ptree.get<T>(key, T{}, tr);
			return AddOrChangeValue(key, value);
		}
		template<typename... Args>
		inline VariantConfigMap<Args...> ToVariantConfigMap() const
		{
			VariantConfigMap<Args...> map;
			for (const auto& [key, value] : config_map_)
			{
				Utils::Visit<Args...>([&key, &map](auto&& arg) {map.AddOrChangeValue(key, arg); }, value);
			}
			return map;
		}
	private:
		std::unordered_map<std::string, std::any> config_map_;
	};

	template<typename T,typename Translator> requires StrTranslator<T,Translator>
	inline bool ChangeTranslator(/*ref*/boost::property_tree::ptree& ptree, const std::string& key, Translator tr)
	{
		boost::optional<T> value = ptree.get_optional<T>(key, tr);
		if (value.has_value())
		{
			ptree.put<T>(key, value.value(), tr);
		}
		return false;
	}

	template<typename T, typename OldTranslator,typename Translator> requires StrTranslator<T,OldTranslator> &&StrTranslator<T, Translator>
	inline bool ChangeTranslator(/*ref*/boost::property_tree::ptree& ptree, const std::string& key, OldTranslator tr_old, Translator tr)
	{
		boost::optional<T> value = ptree.get_optional<T>(key, tr_old);
		if (value.has_value())
		{
			ptree.put<T>(key, value.value(), tr);
		}
		return false;
	}
}// namespace HsBa::Slicer::Config

#endif // !HSBA_SLICER_RW_PRTREE_HPP
