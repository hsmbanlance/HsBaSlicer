#pragma once
#ifndef HSBA_SLICER_STRUCT_JSON_HPP
#define HSBA_SLICER_STRUCT_JSON_HPP

#include <ranges>

#include <boost/pfr.hpp>
#include <boost/pfr/core_name.hpp>

#include <rapidjson/document.h>

#include "base/concepts.hpp"
#include "base/error.hpp"
#include "base/template_helper.hpp"

namespace HsBa::Slicer::Utils
{
    template<typename T>
	concept Aggregte = std::is_aggregate_v<T> && !std::is_union_v<T>;

	template<typename T>
	concept RapidJsonValueConvertible =
		requires(const T& value, rapidjson::Value& json, rapidjson::Document::AllocatorType& doc) {
			{ value.to_json(json, doc) } -> std::same_as<void>;
			{ T::from_json(json) } -> std::same_as<T>;
		};

	namespace detail
	{
		template<typename T>
		void to_json_impl(const T& value, rapidjson::Value& json, rapidjson::Document::AllocatorType& allocator)
		{
			constexpr std::array<std::string_view, boost::pfr::tuple_size_v<T>> field_names =
				boost::pfr::names_as_array<T>();
			boost::pfr::for_each_field(value, [&](const auto& field,size_t idx) {
				std::string_view field_name = field_names[idx];
				using FieldType = std::remove_cvref_t<decltype(field)>;
				if constexpr (RapidJsonValueConvertible<FieldType>)
				{
					rapidjson::Value field_json(rapidjson::kObjectType);
					field.to_json(field_json, allocator);
					json.AddMember(rapidjson::StringRef(field_name.data()), field_json, allocator);
				}
				else if constexpr (std::is_enum_v<FieldType>)
				{
					const std::string enum_str{ EnumName(field).data(),EnumName(field).size()};
					json.AddMember(rapidjson::StringRef(field_name.data()), rapidjson::Value(enum_str.c_str(), allocator), allocator);
				}
				else if constexpr (Aggregte<FieldType>)
				{
					rapidjson::Value field_json(rapidjson::kObjectType);
					to_json_impl(field, field_json,allocator);
					json.AddMember(rapidjson::StringRef(field_name.data()), field_json, allocator);
				}
				else if constexpr (std::is_same_v<FieldType, std::string>)
				{
					json.AddMember(rapidjson::StringRef(field_name.data()), rapidjson::Value(field.c_str(), allocator), allocator);
				}
				else if constexpr (std::is_arithmetic_v<FieldType>)
				{
					json.AddMember(rapidjson::StringRef(field_name.data()), field, allocator);
				}
				else if constexpr (std::ranges::range<FieldType>)
				{
					rapidjson::Value array_json(rapidjson::kArrayType);
					for (const auto& item : field)
					{
						using ItemType = std::remove_cvref_t<decltype(item)>;
						if constexpr (RapidJsonValueConvertible<ItemType>)
						{
							rapidjson::Value item_json(rapidjson::kObjectType);
							item.to_json(item_json, allocator);
							array_json.PushBack(item_json, allocator);
						}
						else if constexpr (Aggregte<ItemType>)
						{
							rapidjson::Value item_json(rapidjson::kObjectType);
							to_json_impl(item, item_json, allocator);
							array_json.PushBack(item_json, allocator);
						}
						else if constexpr (std::is_same_v<ItemType, std::string>)
						{
							array_json.PushBack(rapidjson::Value(item.c_str(), allocator), allocator);
						}
						else if constexpr (std::is_arithmetic_v<ItemType>)
						{
							array_json.PushBack(item, allocator);
						}
						else
						{
							throw RuntimeError("Unsupported item type in to_json_impl for range field");
						}
					}
					json.AddMember(rapidjson::StringRef(field_name.data()), array_json, allocator);
				}
				else
				{
					throw RuntimeError("Unsupported field type in to_json_impl");
				}
			});
		}

		template<typename T>
		void from_json_impl(const rapidjson::Value& json, T& value)
		{
			constexpr std::array<std::string_view, boost::pfr::tuple_size_v<T>> field_names =
				boost::pfr::names_as_array<T>();
			boost::pfr::for_each_field(value, [&](auto& field,size_t idx) {
				std::string_view field_name = field_names[idx];
				using FieldType = std::remove_cvref_t<decltype(field)>;
				if constexpr (RapidJsonValueConvertible<FieldType>)
				{
					if (json.HasMember(field_name.data()))
					{
						field = field.from_json(json[field_name.data()]);
					}
				}
				else if constexpr (std::is_enum_v<FieldType>)
				{
					if (json.HasMember(field_name.data()) && json[field_name.data()].IsString())
					{
						const auto n = json[field_name.data()].GetString();
						field = EnumFromName<FieldType>(json[field_name.data()].GetString());
					}
				}
				else if constexpr (Aggregte<FieldType>)
				{
					if (json.HasMember(field_name.data()))
					{
						from_json_impl(json[field_name.data()], field);
					}
				}
				else if constexpr(std::is_same_v<FieldType, std::string>)
				{
					if (json.HasMember(field_name.data()) && json[field_name.data()].IsString())
					{
						field = json[field_name.data()].GetString();
					}
				}
				else if constexpr (std::is_arithmetic_v<FieldType>)
				{
					if (json.HasMember(field_name.data()) && json[field_name.data()].IsNumber())
					{
						field = json[field_name.data()].Get<FieldType>();
					}
				}
				else if constexpr (std::ranges::range<FieldType>)
				{
					if (json.HasMember(field_name.data()) && json[field_name.data()].IsArray())
					{
						using ItemType = std::remove_cvref_t<typename FieldType::value_type>;
						field.clear();
						for (const auto& item_json : json[field_name.data()].GetArray())
						{
							if constexpr (RapidJsonValueConvertible<ItemType>)
							{
								ItemType item;
								item.from_json(item_json);
								field.push_back(std::move(item));
							}
							else if constexpr (Aggregte<ItemType>)
							{
								ItemType item;
								from_json_impl(item_json, item);
								field.push_back(std::move(item));
							}
							else if constexpr (std::is_same_v<ItemType, std::string>)
							{
								if (item_json.IsString())
								{
									field.push_back(item_json.GetString());
								}
							}
							else if constexpr (std::is_arithmetic_v<ItemType>)
							{
								if (item_json.IsNumber())
								{
									field.push_back(item_json.Get<ItemType>());
								}
							}
							else
							{
								throw RuntimeError("Unsupported item type in from_json_impl for range field");
							}
						}
					}
				}
				else
				{
					throw RuntimeError("Unsupported field type in from_json_impl");
				}
			});
		}
	} // namespace detail

	template<Aggregte T>
	rapidjson::Document to_json(const T& value)
	{
		rapidjson::Document doc;
		doc.SetObject();
		detail::to_json_impl(value, doc,doc.GetAllocator());
		return doc;
	}

	template<Aggregte T>
	rapidjson::Document to_json(const T& value, rapidjson::Document::AllocatorType& allocator)
	{
		rapidjson::Document doc(&allocator);
		doc.SetObject();
		detail::to_json_impl(value, doc);
		return doc;
	}

	template<RapidJsonValueConvertible T>
	rapidjson::Document to_json(const T& value)
	{
		rapidjson::Document doc;
		doc.SetObject();
		value.to_json(doc, doc.GetAllocator());
		return doc;
	}

	template<RapidJsonValueConvertible T>
	rapidjson::Document to_json(const T& value, rapidjson::Document::AllocatorType& allocator)
	{
		rapidjson::Document doc(&allocator);
		doc.SetObject();
		value.to_json(doc, doc.GetAllocator());
		return doc;
	}

	template<Aggregte T>
	T from_json(const rapidjson::Value& json)
	{
		T value{};
		if (!json.IsObject())
		{
			throw InvalidArgumentError("JSON value is not an object");
		}
		detail::from_json_impl(json, value);
		return value;
	}

	template<Aggregte T>
	T from_json(const rapidjson::Document& doc)
	{
		if(!doc.IsObject())
		{
			throw InvalidArgumentError("JSON document is not an object");
		}
		return from_json<T>(static_cast<const rapidjson::Value&>(doc));
	}

	template<RapidJsonValueConvertible T>
	T from_json(const rapidjson::Value& json)
	{
		T value{};
		if (!json.IsObject())
		{
			throw InvalidArgumentError("JSON value is not an object");
		}
		return T::from_json(json);
	}

	template<RapidJsonValueConvertible T>
	T from_json(const rapidjson::Document& doc)
	{
		if (!doc.IsObject())
		{
			throw InvalidArgumentError("JSON document is not an object");
		}
		return from_json<T>(static_cast<const rapidjson::Value&>(doc));
	}
} // namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_STRUCT_JSON_HPP