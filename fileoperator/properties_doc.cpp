#include "properties_doc.hpp"

#include <rapidjson/ostreamwrapper.h>

#include "base/encoding_convert.hpp"

namespace HsBa::Slicer::Config
{
	bool PropertiesDoc::FromJson(std::string_view path)
	{
		std::string path_loc = utf8_to_local(std::string{ path });
		doc_.Parse(path_loc.c_str());
		return !doc_.IsNull();
	}
	bool PropertiesDoc::Write(std::string_view path)
	{
		if (!doc_.IsNull())
		{
			std::string path_loc = utf8_to_local(std::string{ path });
			std::ofstream ofs(path_loc, std::ios_base::out);
			rapidjson::OStreamWrapper osw(ofs);
			rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
			return doc_.Accept(writer);
		}
		return false;
	}
	std::optional<int> PropertiesDoc::GetOptionalInt(std::string_view key) const noexcept
	{
		if (doc_.HasMember(key.data()))
		{
			const auto& json_item = doc_[key.data()];
			if (json_item.IsInt())
			{
				return json_item.GetInt();
			}
			return std::nullopt;
		}
		return std::nullopt;
	}
	std::optional<double> PropertiesDoc::GetOptionalDouble(std::string_view key) const noexcept
	{
		if (doc_.HasMember(key.data()))
		{
			const auto& json_item = doc_[key.data()];
			if (json_item.IsDouble())
			{
				return json_item.GetDouble();
			}
			return std::nullopt;
		}
		return std::nullopt;
	}
	std::optional<bool> PropertiesDoc::GetOptionBool(std::string_view key) const noexcept
	{
		if (doc_.HasMember(key.data()))
		{
			const auto& json_item = doc_[key.data()];
			if (json_item.IsBool())
			{
				return json_item.GetBool();
			}
			return std::nullopt;
		}
		return std::nullopt;
	}
	std::optional<std::string> PropertiesDoc::GetOptionString(std::string_view key) const noexcept
	{
		if (doc_.HasMember(key.data()))
		{
			const auto& json_item = doc_[key.data()];
			if (json_item.IsString())
			{
				return json_item.GetString();
			}
			return std::nullopt;
		}
		return std::nullopt;
	}
	std::optional<std::list<int>> PropertiesDoc::GetOptionalIntArr(std::string_view key) const noexcept
	{
		if (doc_.HasMember(key.data()))
		{
			const auto& json_item = doc_[key.data()];
			if (json_item.IsArray())
			{
				std::list<int> res;
				for (const auto& i : json_item.GetArray())
				{
					if (i.IsInt())
					{
						res.emplace_back(i.GetInt());
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
	std::optional<std::list<double>> PropertiesDoc::GetOptionalDoubleArr(std::string_view key) const noexcept
	{
		if (doc_.HasMember(key.data()))
		{
			const auto& json_item = doc_[key.data()];
			if (json_item.IsArray())
			{
				std::list<double> res;
				for (const auto& i : json_item.GetArray())
				{
					if (i.IsDouble())
					{
						res.emplace_back(i.GetDouble());
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
	std::optional<std::list<bool>> PropertiesDoc::GetOptionalBoolArr(std::string_view key) const noexcept
	{
		if (doc_.HasMember(key.data()))
		{
			const auto& json_item = doc_[key.data()];
			if (json_item.IsArray())
			{
				std::list<bool> res;
				for (const auto& i : json_item.GetArray())
				{
					if (i.IsBool())
					{
						res.emplace_back(i.GetBool());
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
	std::optional<std::list<std::string>> PropertiesDoc::GetOptionalStrArr(std::string_view key) const noexcept
	{
		if (doc_.HasMember(key.data()))
		{
			const auto& json_item = doc_[key.data()];
			if (json_item.IsArray())
			{
				std::list<std::string> res;
				for (const auto& i : json_item.GetArray())
				{
					if (i.IsString())
					{
						res.emplace_back(i.GetString());
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

	void PropertiesDoc::AddInt(std::string_view name, const int v)
	{
		rapidjson::Value value;
		value.SetInt(v);
		rapidjson::Value name_v;
		name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
		doc_.AddMember(name_v, value, doc_.GetAllocator());
	}
	void PropertiesDoc::AddDouble(std::string_view name, const double v)
	{
		rapidjson::Value value;
		value.SetDouble(v);
		rapidjson::Value name_v;
		name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
		doc_.AddMember(name_v, value, doc_.GetAllocator());
	}
	void PropertiesDoc::AddBool(std::string_view name, const bool v)
	{
		rapidjson::Value value;
		value.SetBool(v);
		rapidjson::Value name_v;
		name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
		doc_.AddMember(name_v, value, doc_.GetAllocator());
	}
	void PropertiesDoc::AddString(std::string_view name, const std::string& v)
	{
		rapidjson::Value value;
		value.SetString(v.c_str(), static_cast<rapidjson::SizeType>(v.size()));
		rapidjson::Value name_v;
		name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
		doc_.AddMember(name_v, value, doc_.GetAllocator());
	}
	void PropertiesDoc::AddIntArr(std::string_view name, const std::list<int>& arr)
	{
		rapidjson::Value name_v;
		name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
		auto& allocator = doc_.GetAllocator();
		rapidjson::Value value;
		for (const auto& i : arr)
		{
			rapidjson::Value i_v;
			i_v.SetInt(i);
			value.PushBack(i_v, allocator);
		}
		doc_.AddMember(name_v, value, allocator);
	}
	void PropertiesDoc::AddDoubleArr(std::string_view name, const std::list<double>& arr)
	{
		rapidjson::Value name_v;
		name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
		auto& allocator = doc_.GetAllocator();
		rapidjson::Value value;
		for (const auto& i : arr)
		{
			rapidjson::Value i_v;
			i_v.SetDouble(i);
			value.PushBack(i_v, allocator);
		}
		doc_.AddMember(name_v, value, allocator);
	}
	void PropertiesDoc::AddBoolArr(std::string_view name, const std::list<bool>& arr)
	{
		rapidjson::Value name_v;
		name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
		auto& allocator = doc_.GetAllocator();
		rapidjson::Value value;
		for (const auto& i : arr)
		{
			rapidjson::Value i_v;
			i_v.SetBool(i);
			value.PushBack(i_v, allocator);
		}
		doc_.AddMember(name_v, value, allocator);
	}
	void PropertiesDoc::AddStringArr(std::string_view name, const std::list<std::string>& arr)
	{
		rapidjson::Value name_v;
		name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
		auto& allocator = doc_.GetAllocator();
		rapidjson::Value value;
		for (const auto& i : arr)
		{
			rapidjson::Value i_v;
			i_v.SetString(i.c_str(), static_cast<rapidjson::SizeType>(i.size()));
			value.PushBack(i_v, allocator);
		}
		doc_.AddMember(name_v, value, allocator);
	}
	const rapidjson::Value& PropertiesDoc::GetValue(std::string_view key) const
	{
		const auto& json_item = doc_[key.data()];
		return json_item;
	}
	rapidjson::Value& PropertiesDoc::GetValue(std::string_view key)
	{
		auto& json_item = doc_[key.data()];
		return json_item;
	}
	void PropertiesDoc::AddValue(std::string_view name,rapidjson::Value& value)
	{
		rapidjson::Value name_v;
		name_v.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()));
		doc_.AddMember(name_v, value, doc_.GetAllocator());
	}
}// namespace HsBa::Slicer::Config