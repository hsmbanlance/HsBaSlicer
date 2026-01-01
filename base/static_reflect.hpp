#pragma once

#ifndef HSBA_UTILS_STATIC_REFLECT
#define HSBA_UTILS_STATIC_REFLECT

#include <tuple>
#include <any>

#include <boost/preprocessor.hpp>

#include "template_helper.hpp"
#include "concepts.hpp"

/**
 * @brief Introspect class static reflection
 */
namespace HsBa::Slicer::Utils::StaticReflect
{
	/**
	 * @brief field info
	 * @tparam ClassT class type
	 * @tparam FieldT field type
	 * @tparam FieldName f	ield name
	 * @tparam FieldPtr field ptr
	 */
	template<typename ClassT, typename FieldT, TemplateString FieldName, FieldT ClassT::* FieldPtr>
	struct FieldInfo
	{
		/**
		 * @brief class type
		 */
		using ClassType = ClassT;
		/**
		 * @brief field type
		 */
		using FieldType = FieldT;
		/**
		 * @brief field name
		 */
		static constexpr auto Name = FieldName;
		/**
		 * @brief field ptr
		 */
		static constexpr auto Pointer = FieldPtr;
		static FieldT& Get(ClassT& obj) noexcept
		{
			return obj.*FieldPtr;
		}
		static const FieldT& Get(const ClassT& obj) noexcept
		{
			return obj.*FieldPtr;
		}
		constexpr std::string_view GetName() const noexcept
		{
			return FieldName.ToStringView();
		}
	};

	/**
	 * @brief method info
	 * @tparam ClassT class type
	 * @tparam FuncT function type
	 * @tparam MethodName method name
	 * @tparam MethodPtr method ptr
	 */
	template<typename ClassT, typename FuncT, TemplateString MethodName, FuncT ClassT::* MethodPtr>
	struct MethodInfo
	{
		/**
		 * @brief class type
		 */
		using ClassType = ClassT;
		/**
		 * @brief func type
		 */
		using FunctionType = FuncT;
		/**
		 * @brief method name
		 */
		static constexpr auto Name = MethodName;
		/**
		 * @brief method ptr
		 */
		static constexpr auto Pointer = MethodPtr;
		template<typename... Args>
		static decltype(auto) Invoke(ClassT& obj, Args&&... args)
		{
			return (obj.*MethodPtr)(std::forward<Args>(args)...);
		}
		template<typename... Args>
		static decltype(auto) Invoke(const ClassT& obj, Args&&... args)
		{
			return (obj.*MethodPtr)(std::forward<Args>(args)...);
		}
		constexpr std::string_view GetName() const noexcept
		{
			return MethodName.ToStringView();
		}
	};

	/**
	 * @brief class reflectable concept, using this static reflect
	 */
	template<typename T>
	concept Reflectable = requires
	{
		typename T::FieldList;
		typename T::MethodList;
		{ T::ClassName };
	};

	/**
	 * @brief reflector for reflectable class
	 * @tparam class type
	 */
	template<Reflectable T>
	class Reflector
	{
	public:
		using Type = T;
		static constexpr std::string_view ClassName() noexcept
		{
			return T::ClassName.ToStringView();
		}
		static constexpr size_t FieldCount() noexcept
		{
			return std::tuple_size_v<typename T::FieldList>;
		}
		template<size_t Index>
		static constexpr auto GetFieldInfo() noexcept
		{
			static_assert(Index < FieldCount(), "Field index out of range");
			return std::get<Index>(typename T::FieldList{});
		}
		template<size_t Index>
		static constexpr auto GetMethodInfo() noexcept
		{
			static_assert(Index < MethodCount(), "Method index out of range");
			return std::get<Index>(typename T::MethodList{});
		}
		static constexpr size_t MethodCount() noexcept
		{
			return std::tuple_size_v<typename T::MethodList>;
		}
		template<size_t Index>
		static constexpr std::string_view MethodName() noexcept
		{
			static_assert(Index < MethodCount(), "Method index out of range");
			return GetMethodInfo<Index>().Name.ToStringView();
		}
		template<size_t Index>
		static constexpr std::string_view FieldName() noexcept
		{
			static_assert(Index < FieldCount(), "Field index out of range");
			return GetFieldInfo<Index>().Name.ToStringView();
		}
		template<size_t Index>
		static constexpr auto& GetField(T& obj) noexcept
		{
			static_assert(Index < FieldCount(), "Field index out of range");
			return GetFieldInfo<Index>().Get(obj);
		}
		template<size_t Index>
		static constexpr auto& GetField(const T& obj) noexcept
		{
			static_assert(Index < FieldCount(), "Field index out of range");
			return GetFieldInfo<Index>().Get(obj);
		}
		static constexpr size_t FindFieldIndexByName(std::string_view name) noexcept
		{
			for (size_t i = 0; i < FieldCount(); ++i)
			{
				if (GetFieldInfo<i>().Name.ToStringView() == name)
				{
					return i;
				}
			}
			return static_cast<size_t>(-1);
		}
		static constexpr size_t FindMethodIndexByName(std::string_view name) noexcept
		{
			for (size_t i = 0; i < MethodCount(); ++i)
			{
				if (GetMethodInfo<i>().Name.ToStringView() == name)
				{
					return i;
				}
			}
			return static_cast<size_t>(-1);
		}
		template<TemplateString FuncName, typename ...Args>
		static decltype(auto) InvokeMemberFunction(T& obj, Args&&... args)
		{
			return InvokeMemberFunctionImpl<0, FuncName>(obj, args...);
		}
	private:
		template<size_t Index, TemplateString FuncName,typename ...Args>
		static auto InvokeMemberFunctionImpl(T& obj, Args&&... args)
		{
			if constexpr (Index < MethodCount())
			{
				if constexpr (GetMethodInfo<Index>().Name == FuncName)
				{
					return GetMethodInfo<Index>().Invoke(obj, std::forward<Args>(args)...);
				}
				else
				{
					return InvokeMemberFunctionImpl<Index + 1, FuncName>(obj, std::forward<Args>(args)...);
				}
			}
			else
			{
				static_assert(Index < MethodCount(), "Method not found");
			}
		}
	};

} // namespace HsBa::Utils::Slicer::StaticReflect

#endif // HSBA_UTILS_STATIC_REFLECT
