#pragma once
#ifndef HSBA_SLICER_IZIPPER_HPP
#define HSBA_SLICER_IZIPPER_HPP

#include <string>
#include <string_view>

namespace HsBa::Slicer
{
	class IZipper
	{
	public:
		virtual ~IZipper() {}
		virtual void AddByteFile(std::string_view name, const std::string& data) = 0;
		virtual void AddFile(std::string_view name, std::string_view path) = 0;
		virtual void AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data) = 0;
		virtual void AddFileIgnoreDuplicate(std::string_view name, std::string_view path) = 0;
		virtual void Save(std::string_view filePath) = 0;
		IZipper() = default;
		IZipper(const IZipper&) = delete;
		IZipper& operator=(const IZipper&) = delete;
	private:

	};
}// namespace HsBa::Slicer
#endif // !HSBA_SLICER_IZIPPER_HPP
