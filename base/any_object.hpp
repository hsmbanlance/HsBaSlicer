#pragma once
#ifndef HSBA_SLICER_ANY_OBJECT_HPP

#include <cstdint>
#include <span>
#include <tuple>
#include <unordered_map>
#include <functional>
#include <type_traits>
#include <map>
#include <optional>

#include "concepts.hpp"
#include "error.hpp"

namespace HsBa::Slicer::Utils
{
	class AnyObject;
	struct TypeInfo
	{
		std::string_view Name;
		void (*destroy)(void*);
		void* (*copy)(const void*);
		void* (*move)(void*);

		using Field = std::pair<TypeInfo*, size_t>;
		using Method = AnyObject(*)(void*, std::span<AnyObject>);
		std::unordered_map<std::string_view, Field> fields;
		std::unordered_map<std::string_view, Method> methods;
	};

	template<typename T>
	TypeInfo* GetTypeInfo();

	class AnyObject
	{
	public:
		AnyObject() : type_info(nullptr), data(nullptr), flag(0) {}
		AnyObject(TypeInfo* type_info, void* data) :
			type_info(type_info), data(data), flag(0) {}
		AnyObject(const AnyObject& other);
		AnyObject(AnyObject&& other) noexcept;
		~AnyObject();
		AnyObject& operator=(const AnyObject& other);
		AnyObject& operator=(AnyObject&& other) noexcept;
		template<typename T, typename = std::enable_if_t<!std::is_same_v<std::remove_cvref_t<T>, AnyObject>>>
		AnyObject(T&& value);
		template<typename T>
		T& cast()
		{
			if (!type_info || type_info != GetTypeInfo<std::remove_cvref_t<T>>())
			{
				throw RuntimeError("Bad AnyObject cast: type mismatch");
			}
			return *static_cast<T*>(data);
		}
		template<typename T>
		T cast_new()
		{
			if (!type_info || type_info != GetTypeInfo<std::remove_cvref_t<T>>())
			{
				throw RuntimeError("Bad AnyObject cast: type mismatch");
			}
			return *static_cast<T*>(data);
		}
		TypeInfo* get_type_info() const { return type_info; }
		AnyObject Invoke(std::string_view method_name, std::span<AnyObject> args);
		void ForeachField(const std::function<void(std::string_view, AnyObject)>& callback);
	private:
		TypeInfo* type_info;
		void* data;
		uint8_t flag;
	};

	template<typename T>
	struct member_fn_traits;
	template<typename R, typename C, typename... Args>
	struct member_fn_traits<R(C::*)(Args...)>
	{
		using ReturnType = R;
		using ClassType = C;
		using ArgTypes = std::tuple<Args...>;
	};
	template<typename R, typename C, typename... Args>
	struct member_fn_traits<R(C::*)(Args...) const>
	{
		using ReturnType = R;
		using ClassType = C;
		using ArgTypes = std::tuple<Args...>;
	};

	template<auto ptr>
	auto* type_ensure()
	{
		using traits = member_fn_traits<decltype(ptr)>;
		using ClassType = typename traits::ClassType;
		using ResultType = typename traits::ReturnType;
		using ArgTypes = typename traits::ArgTypes;
		return +[](void* obj, std::span<AnyObject> args) -> AnyObject {
			auto self = static_cast<ClassType*>(obj);
			return [=]<std::size_t... Is>(std::index_sequence<Is...>) {
				if constexpr (std::is_void_v<ResultType>)
				{
					(self->*ptr)(args[Is].template cast<std::remove_cvref_t<std::tuple_element_t<Is, ArgTypes>>>()...);
					return AnyObject{};
				}
				else
				{
					return AnyObject{ (self->*ptr)(args[Is].template cast<std::remove_cvref_t<std::tuple_element_t<Is, ArgTypes>>>()...) };
				}
			}(std::make_index_sequence<std::tuple_size_v<ArgTypes>>{});
		};
	}

	/**
	 * @brief Return type info for type T, this is a singleton per type T
	 * @tparam T type
	 * @return TypeInfo* pointer to the type info
	 */
	template<typename T>
	TypeInfo* GetTypeInfo()
	{
		static TypeInfo info;
		info.Name = typeid(T).name();
		info.destroy = [](void* data) { delete static_cast<T*>(data); };
		info.copy = [](const void* data) -> void* { return new T(*static_cast<const T*>(data)); };
		info.move = [](void* data) -> void* { return new T(std::move(*static_cast<T*>(data))); };
		return &info;
	}

	template<typename T, typename>
	AnyObject::AnyObject(T&& value)
	{
		using Decayed = std::remove_cvref_t<T>;
		type_info = GetTypeInfo<Decayed>();
		data = new Decayed(std::forward<T>(value));
		flag = 0b1;
	}

	    template<>
        inline TypeInfo* GetTypeInfo<std::string>()
        {
            static TypeInfo info;
            info.Name = "std::string";
            info.destroy = [](void* data) { delete static_cast<std::string*>(data); };
            info.copy = [](const void* data) -> void* { return new std::string(*static_cast<const std::string*>(data)); };
            info.move = [](void* data) -> void* { return new std::string(std::move(*static_cast<std::string*>(data))); };
            info.methods["size"] = +[](void* obj, std::span<AnyObject>) -> AnyObject {
                auto str = static_cast<std::string*>(obj);
                return AnyObject{ str->size() };
            };
            info.methods["c_str"] = +[](void* obj, std::span<AnyObject>) -> AnyObject {
                auto str = static_cast<std::string*>(obj);
                return AnyObject{ str->c_str() };
            };
            info.methods["at"] = +[](void* obj, std::span<AnyObject> args) -> AnyObject {
                auto str = static_cast<std::string*>(obj);
                if (args.size() != 1 || !args[0].get_type_info() || args[0].get_type_info()->Name != "size_t")
                {
                    throw RuntimeError("std::string::at expects one size_t argument");
                }
                size_t index = args[0].cast<size_t>();
                if (index >= str->size())
                {
                    throw RuntimeError("std::string::at index out of range");
                }
                return AnyObject{ (*str)[index] };
            };
            return &info;
        }

        template<>
        inline TypeInfo* GetTypeInfo<std::string_view>()
        {
            static TypeInfo info;
            info.Name = "std::string_view";
            info.destroy = [](void* data) { /* do nothing */ };
            info.copy = [](const void* data) -> void* { return new std::string_view(*static_cast<const std::string_view*>(data)); };
            info.move = [](void* data) -> void* { return new std::string_view(std::move(*static_cast<std::string_view*>(data))); };
            info.methods["size"] = +[](void* obj, std::span<AnyObject>) -> AnyObject {
                auto str = static_cast<std::string_view*>(obj);
                return AnyObject{ str->size() };
            };
            info.methods["data"] = +[](void* obj, std::span<AnyObject>) -> AnyObject {
                auto str = static_cast<std::string_view*>(obj);
                return AnyObject{ str->data() };
            };
            info.methods["at"] = +[](void* obj, std::span<AnyObject> args) -> AnyObject {
                auto str = static_cast<std::string_view*>(obj);
                if (args.size() != 1 || !args[0].get_type_info() || args[0].get_type_info()->Name != "size_t")
                {
                    throw RuntimeError("std::string_view::at expects one size_t argument");
                }
                size_t index = args[0].cast<size_t>();
                if (index >= str->size())
                {
                    throw RuntimeError("std::string_view::at index out of range");
                }
                return AnyObject{ (*str)[index] };
            };
            return &info;
        }
 
} // namespace HsBa::Slicer::Utils

#endif // HSBA_SLICER_ANY_OBJECT_HPP