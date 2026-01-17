#pragma once
#ifndef HSBA_SLICER_STRUCT_JSON_HPP
#define HSBA_SLICER_STRUCT_JSON_HPP

#include <ranges>

#include <boost/pfr.hpp>
#include <boost/pfr/core_name.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/reader.h>

#include "base/concepts.hpp"
#include "base/error.hpp"
#include "base/template_helper.hpp"
#include "base/static_reflect.hpp"

namespace HsBa::Slicer::Utils
{
	template<typename T>
	concept Aggregte = std::is_aggregate_v<T> && !std::is_union_v<T>;

	template<typename T>
	concept RapidJsonValueConvertible =
		requires(const T & value, rapidjson::Value & json, rapidjson::Document::AllocatorType & doc) {
			{ value.to_json(json, doc) } -> std::same_as<void>;
			{ T::from_json(json) } -> std::same_as<T>;
	};

	// Reflectable: wrapper concept pointing to the static reflect system
	template<typename T>
	concept Reflectable = StaticReflect::Reflectable<T>;

	namespace detail
	{
		// Forward declarations for recursive / dependent calls
		template<Aggregte T>
			requires(!Reflectable<T>)
		void to_json_impl(const T& value, rapidjson::Value& json, rapidjson::Document::AllocatorType& allocator);

		template<Aggregte T>
			requires(!Reflectable<T>)
		void from_json_impl(const rapidjson::Value& json, T& value);

		// Forward decls for reflectable types
		template<Reflectable T>
		void to_json_impl(const T& value, rapidjson::Value& json, rapidjson::Document::AllocatorType& allocator);

		template<Reflectable T>
		void from_json_impl(const rapidjson::Value& json, T& value);

		template<typename FieldType>
		void add_member(rapidjson::Value& parent, std::string_view name, const FieldType& field, rapidjson::Document::AllocatorType& allocator);

		// Helper: expand reflectable fields into add_member calls (works around MSVC template-lambda parsing)
		template<typename FieldType, std::size_t... Is>
		void add_reflectable_members_impl(rapidjson::Value& parent, const FieldType& field, rapidjson::Document::AllocatorType& allocator, std::index_sequence<Is...>)
		{
			if constexpr (sizeof...(Is) > 0)
			{
				int dummy[] = { ((add_member(parent, StaticReflect::Reflector<FieldType>::template FieldName<Is>(), StaticReflect::Reflector<FieldType>::template GetField<Is>(field), allocator), 0))... };
				(void)dummy;
			}
		}

		// Helper: expand reflectable fields into set_field_from_json calls (works around MSVC template-lambda parsing)
		template<typename T, std::size_t... Is>
		void set_reflectable_fields_impl(const rapidjson::Value& json, T& value, std::index_sequence<Is...>)
		{
			if constexpr (sizeof...(Is) > 0)
			{
				int dummy[] = {
					((json.HasMember(StaticReflect::Reflector<T>::template FieldName<Is>().data())
						? (set_field_from_json(StaticReflect::Reflector<T>::template GetField<Is>(value), json[StaticReflect::Reflector<T>::template FieldName<Is>().data()]), 0)
						: 0), 0)...
				};
				(void)dummy;
			}
		}

		template<RapidJsonValueConvertible FieldType>
		void add_member(rapidjson::Value& parent, std::string_view name, const FieldType& field, rapidjson::Document::AllocatorType& allocator)
		{
			rapidjson::Value field_json(rapidjson::kObjectType);
			field.to_json(field_json, allocator);
			parent.AddMember(rapidjson::StringRef(name.data()), field_json, allocator);
		}

		template<typename FieldType>
			requires std::is_enum_v<FieldType>
		void add_member(rapidjson::Value& parent, std::string_view name, const FieldType& field, rapidjson::Document::AllocatorType& allocator)
		{
			const std::string enum_str{ EnumName(field).data(), EnumName(field).size() };
			parent.AddMember(rapidjson::StringRef(name.data()), rapidjson::Value(enum_str.c_str(), allocator), allocator);
		}

		template<Aggregte FieldType>
			requires(!Reflectable<FieldType>)
		void add_member(rapidjson::Value& parent, std::string_view name, const FieldType& field, rapidjson::Document::AllocatorType& allocator)
		{
			rapidjson::Value field_json(rapidjson::kObjectType);
			to_json_impl(field, field_json, allocator);
			parent.AddMember(rapidjson::StringRef(name.data()), field_json, allocator);
		}

		// Reflectable field serialization (uses StaticReflect::Reflector)
		template<Reflectable FieldType>
		void add_member(rapidjson::Value& parent, std::string_view name, const FieldType& field, rapidjson::Document::AllocatorType& allocator)
		{
			rapidjson::Value field_json(rapidjson::kObjectType);
			// dispatch by index sequence over reflect fields
			constexpr size_t N = StaticReflect::Reflector<FieldType>::FieldCount();
			add_reflectable_members_impl<FieldType>(field_json, field, allocator, std::make_index_sequence<N>{});
			parent.AddMember(rapidjson::StringRef(name.data()), field_json, allocator);
		}
		template<typename FieldType>
			requires std::is_same_v<FieldType, std::string>
		void add_member(rapidjson::Value& parent, std::string_view name, const FieldType& field, rapidjson::Document::AllocatorType& allocator)
		{
			parent.AddMember(rapidjson::StringRef(name.data()), rapidjson::Value(field.c_str(), allocator), allocator);
		}

		template<typename FieldType>
			requires std::is_arithmetic_v<FieldType>
		void add_member(rapidjson::Value& parent, std::string_view name, const FieldType& field, rapidjson::Document::AllocatorType& allocator)
		{
			parent.AddMember(rapidjson::StringRef(name.data()), field, allocator);
		}

		template<std::ranges::range Range>
			requires (!std::is_same_v<Range, std::string>)
		void add_member(rapidjson::Value& parent, std::string_view name, const Range& rng, rapidjson::Document::AllocatorType& allocator)
		{
			rapidjson::Value array_json(rapidjson::kArrayType);
			for (const auto& item : rng)
			{
				using ItemType = std::remove_cvref_t<decltype(item)>;
				if constexpr (RapidJsonValueConvertible<ItemType>)
				{
					rapidjson::Value item_json(rapidjson::kObjectType);
					item.to_json(item_json, allocator);
					array_json.PushBack(item_json, allocator);
				}
				else if constexpr (Reflectable<ItemType>)
				{
					rapidjson::Value item_json(rapidjson::kObjectType);
					to_json_impl(item, item_json, allocator);
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
				else if constexpr (OptionalLike<ItemType>)
				{
					if (item.has_value())
					{
						using ValueType = std::remove_cvref_t<typename ItemType::value_type>;
						if constexpr (RapidJsonValueConvertible<ValueType>)
						{
							rapidjson::Value item_json(rapidjson::kObjectType);
							item->to_json(item_json, allocator);
							array_json.PushBack(item_json, allocator);
						}
						else if constexpr (Reflectable<ValueType>)
						{
							rapidjson::Value item_json(rapidjson::kObjectType);
							to_json_impl(*item, item_json, allocator);
							array_json.PushBack(item_json, allocator);
						}
						else if constexpr (Aggregte<ValueType>)
						{
							rapidjson::Value item_json(rapidjson::kObjectType);
							to_json_impl(*item, item_json, allocator);
							array_json.PushBack(item_json, allocator);
						}
						else if constexpr (std::is_same_v<ValueType, std::string>)
						{
							array_json.PushBack(rapidjson::Value(item->c_str(), allocator), allocator);
						}
						else if constexpr (std::is_arithmetic_v<ValueType>)
						{
							array_json.PushBack(*item, allocator);
						}
						else if constexpr (std::is_enum_v<ValueType>)
						{
							const std::string enum_str{ EnumName(*item).data(), EnumName(*item).size() };
							array_json.PushBack(rapidjson::Value(enum_str.c_str(), allocator), allocator);
						}
						else
						{
							throw RuntimeError("Unsupported item type in to_json_impl for range field with optional");
						}
					}
					else
					{
						array_json.PushBack(rapidjson::Value(rapidjson::kNullType), allocator);
					}
				}
				else
				{
					throw RuntimeError("Unsupported item type in to_json_impl for range field");
				}
			}
			parent.AddMember(rapidjson::StringRef(name.data()), array_json, allocator);
		}

		template<OptionalLike Opt>
		void add_member(rapidjson::Value& parent, std::string_view name, const Opt& opt, rapidjson::Document::AllocatorType& allocator)
		{
			using ValueType = std::remove_cvref_t<typename Opt::value_type>;
			if (opt.has_value())
			{
				if constexpr (RapidJsonValueConvertible<ValueType>)
				{
					rapidjson::Value field_json(rapidjson::kObjectType);
					opt->to_json(field_json, allocator);
					parent.AddMember(rapidjson::StringRef(name.data()), field_json, allocator);
				}
				else if constexpr (Aggregte<ValueType>)
				{
					rapidjson::Value field_json(rapidjson::kObjectType);
					to_json_impl(*opt, field_json, allocator);
					parent.AddMember(rapidjson::StringRef(name.data()), field_json, allocator);
				}
				else if constexpr (std::is_same_v<ValueType, std::string>)
				{
					parent.AddMember(rapidjson::StringRef(name.data()), rapidjson::Value(opt->c_str(), allocator), allocator);
				}
				else if constexpr (std::is_arithmetic_v<ValueType>)
				{
					parent.AddMember(rapidjson::StringRef(name.data()), *opt, allocator);
				}
				else if constexpr (std::is_enum_v<ValueType>)
				{
					const std::string enum_str{ EnumName(*opt).data(), EnumName(*opt).size() };
					parent.AddMember(rapidjson::StringRef(name.data()), rapidjson::Value(enum_str.c_str(), allocator), allocator);
				}
				else
				{
					throw RuntimeError("Unsupported field type in to_json_impl for optional field");
				}
			}
			else
			{
				parent.AddMember(rapidjson::StringRef(name.data()), rapidjson::Value(rapidjson::kNullType), allocator);
			}
		}

		// fallback for unsupported field types
		template<typename FieldType>
		void add_member(rapidjson::Value&, std::string_view, const FieldType&, rapidjson::Document::AllocatorType&)
		{
			throw RuntimeError("Unsupported field type in to_json_impl");
		}

		template<Aggregte T>
			requires(!Reflectable<T>)
		void to_json_impl(const T& value, rapidjson::Value& json, rapidjson::Document::AllocatorType& allocator)
		{
			constexpr std::array<std::string_view, boost::pfr::tuple_size_v<T>> field_names = boost::pfr::names_as_array<T>();
			boost::pfr::for_each_field(value, [&](const auto& field, size_t idx) {
				add_member(json, field_names[idx], field, allocator);
				});
		}

		// Reflectable to_json_impl using StaticReflect::Reflector<T>
		template<Reflectable T>
		void to_json_impl(const T& value, rapidjson::Value& json, rapidjson::Document::AllocatorType& allocator)
		{
			constexpr size_t N = StaticReflect::Reflector<T>::FieldCount();
			// expand over index sequence using helper (avoids template-lambda parsing issues)
			add_reflectable_members_impl<T>(json, value, allocator, std::make_index_sequence<N>{});
		}
		// -------------------- from_json helpers --------------------
		template<RapidJsonValueConvertible FieldType>
		void set_field_from_json(FieldType& field, const rapidjson::Value& field_json)
		{
			field = FieldType::from_json(field_json);
		}

		template<typename FieldType>
			requires std::is_enum_v<FieldType>
		void set_field_from_json(FieldType& field, const rapidjson::Value& field_json)
		{
			if (field_json.IsString())
			{
				field = EnumFromName<FieldType>(field_json.GetString());
			}
		}

		template<Aggregte FieldType>
			requires(!Reflectable<FieldType>)
		void set_field_from_json(FieldType& field, const rapidjson::Value& field_json)
		{
			from_json_impl(field_json, field);
		}

		// Reflectable field deserialization
		template<Reflectable FieldType>
		void set_field_from_json(FieldType& field, const rapidjson::Value& field_json)
		{
			from_json_impl(field_json, field);
		}
		template<typename FieldType>
			requires std::is_same_v<FieldType, std::string>
		void set_field_from_json(FieldType& field, const rapidjson::Value& field_json)
		{
			if (field_json.IsString()) field = field_json.GetString();
		}

		template<typename FieldType>
			requires std::is_arithmetic_v<FieldType>
		void set_field_from_json(FieldType& field, const rapidjson::Value& field_json)
		{
			if (field_json.IsNumber()) field = field_json.template Get<FieldType>();
		}

		template<std::ranges::range Range>
			requires (!std::is_same_v<Range, std::string>)
		void set_field_from_json(Range& field, const rapidjson::Value& array_json)
		{
			using ItemType = std::remove_cvref_t<typename Range::value_type>;
			field.clear();
			for (const auto& item_json : array_json.GetArray())
			{
				if constexpr (RapidJsonValueConvertible<ItemType>)
				{
					ItemType item = ItemType::from_json(item_json);
					field.push_back(std::move(item));
				}
				else if constexpr (Reflectable<ItemType>)
				{
					ItemType item;
					from_json_impl(item_json, item);
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
					if (item_json.IsString()) field.push_back(item_json.GetString());
				}
				else if constexpr (std::is_arithmetic_v<ItemType>)
				{
					if (item_json.IsNumber()) field.push_back(item_json.template Get<ItemType>());
				}
				else
				{
					throw RuntimeError("Unsupported item type in from_json_impl for range field");
				}
			}
		}

		template<OptionalLike Opt>
		void set_field_from_json(Opt& opt, const rapidjson::Value& field_json)
		{
			using ValueType = std::remove_cvref_t<typename Opt::value_type>;
			if (field_json.IsNull())
			{
				opt.reset();
				return;
			}
			if constexpr (RapidJsonValueConvertible<ValueType>)
			{
				opt = ValueType::from_json(field_json);
			}
			else if constexpr (Aggregte<ValueType>)
			{
				ValueType v;
				from_json_impl(field_json, v);
				opt = std::move(v);
			}
			else if constexpr (std::is_same_v<ValueType, std::string>)
			{
				if (field_json.IsString()) opt = field_json.GetString();
			}
			else if constexpr (std::is_arithmetic_v<ValueType>)
			{
				if (field_json.IsNumber()) opt = field_json.template Get<ValueType>();
			}
			else if constexpr (std::is_enum_v<ValueType>)
			{
				if (field_json.IsString()) opt = EnumFromName<ValueType>(field_json.GetString());
			}
			else
			{
				throw RuntimeError("Unsupported field type in from_json_impl for optional field");
			}
		}

		// fallback
		template<typename FieldType>
		void set_field_from_json(FieldType&, const rapidjson::Value&)
		{
			throw RuntimeError("Unsupported field type in from_json_impl");
		}

		template<Aggregte T>
			requires(!Reflectable<T>)
		void from_json_impl(const rapidjson::Value& json, T& value)
		{
			constexpr std::array<std::string_view, boost::pfr::tuple_size_v<T>> field_names = boost::pfr::names_as_array<T>();
			boost::pfr::for_each_field(value, [&](auto& field, size_t idx) {
				auto name = field_names[idx];
				if (!json.HasMember(name.data())) return;
				const auto& field_json = json[name.data()];
				set_field_from_json(field, field_json);
				});
		}

		// Reflectable from_json_impl using StaticReflect::Reflector<T>
		template<Reflectable T>
		void from_json_impl(const rapidjson::Value& json, T& value)
		{
			constexpr size_t N = StaticReflect::Reflector<T>::FieldCount();
			set_reflectable_fields_impl<T>(json, value, std::make_index_sequence<N>{});
		}
	} // namespace detail

	template<Aggregte T>
		requires(!Reflectable<T>)
	rapidjson::Document to_json(const T& value)
	{
		rapidjson::Document doc;
		doc.SetObject();
		detail::to_json_impl(value, doc, doc.GetAllocator());
		return doc;
	}

	// Reflectable public to_json overloads
	template<Reflectable T>
	rapidjson::Document to_json(const T& value)
	{
		rapidjson::Document doc;
		doc.SetObject();
		detail::to_json_impl(value, doc, doc.GetAllocator());
		return doc;
	}

	template<Reflectable T>
	rapidjson::Document to_json(const T& value, rapidjson::Document::AllocatorType& allocator)
	{
		rapidjson::Document doc(&allocator);
		doc.SetObject();
		detail::to_json_impl(value, doc, doc.GetAllocator());
		return doc;
	}

	template<Aggregte T>
		requires(!Reflectable<T>)
	rapidjson::Document to_json(const T& value, rapidjson::Document::AllocatorType& allocator)
	{
		rapidjson::Document doc(&allocator);
		doc.SetObject();
		detail::to_json_impl(value, doc, doc.GetAllocator());
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
		requires(!Reflectable<T>)
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

	// Reflectable public from_json overloads
	template<Reflectable T>
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
		requires(!Reflectable<T>)
	T from_json(const rapidjson::Document& doc)
	{
		if (!doc.IsObject())
		{
			throw InvalidArgumentError("JSON document is not an object");
		}
		return from_json<T>(static_cast<const rapidjson::Value&>(doc));
	}

	template<Reflectable T>
	T from_json(const rapidjson::Document& doc)
	{
		if (!doc.IsObject())
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

	template<typename T>
	std::ostream& write_json(std::ostream& os, const T& value)
	{
		rapidjson::Document doc = to_json(value);
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
		os << buffer.GetString();
		return os;
	}

	template<typename T>
	std::ostream& write_pretty_json(std::ostream& os, const T& value)
	{
		rapidjson::Document doc = to_json(value);
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
		os << buffer.GetString();
		return os;
	}

	template<typename T>
	std::string write_json(const T& value)
	{
		rapidjson::Document doc = to_json(value);
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
		return buffer.GetString();
	}

	template<typename T>
	std::string write_pretty_json(const T& value)
	{
		rapidjson::Document doc = to_json(value);
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
		return buffer.GetString();
	}

	template<typename T>
	void write_json(std::string_view path, const T& value)
	{
		std::ofstream ofs(path.data());
		if (!ofs)
		{
			throw IOError("Failed to open file for writing: " + std::string(path));
		}
		write_json(ofs, value);
		ofs.close();
	}

	template<typename T>
	void write_pretty_json(std::string_view path, const T& value)
	{
		std::ofstream ofs(path.data());
		if (!ofs)
		{
			throw IOError("Failed to open file for writing: " + std::string(path));
		}
		write_pretty_json(ofs, value);
		ofs.close();
	}

	template<typename T>
	T read_json(std::istream& is)
	{
		rapidjson::Document doc;
		std::string json_str((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
		doc.Parse(json_str.c_str());
		if (doc.HasParseError())
		{
			throw RuntimeError("JSON parse error: " + std::to_string(doc.GetParseError()));
		}
		return from_json<T>(doc);
	}

	template<typename T>
	T read_json_from_file(std::string_view path)
	{
		std::ifstream ifs(path.data());
		if (!ifs)
		{
			throw IOError("Failed to open file for reading: " + std::string(path));
		}
		T value = read_json<T>(ifs);
		ifs.close();
		return value;
	}

	template<typename T>
	T read_json_from_string(const std::string& json_str)
	{
		rapidjson::Document doc;
		doc.Parse(json_str.c_str());
		if (doc.HasParseError())
		{
			throw RuntimeError("JSON parse error: " + std::to_string(doc.GetParseError()));
		}
		return from_json<T>(doc);
	}
} // namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_STRUCT_JSON_HPP