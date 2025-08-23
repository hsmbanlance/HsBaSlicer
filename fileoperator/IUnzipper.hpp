#pragma once
#ifndef HSBA_SLICER_IUNZIPPER_HPP
#define HSBA_SLICER_IUNZIPPER_HPP

#include <string>
#include <string_view>
#include <istream>
#include <variant>
#include <fstream>
#include <sstream>
#include <memory>
#include <any>

#include "base/error.hpp"

namespace HsBa::Slicer
{
	class UnzipperStream : public std::istream
	{
	public:
		UnzipperStream() : std::istream{ nullptr }
		{
			rdbuf(nullptr);
		}
		UnzipperStream(std::string_view fileName, std::ios_base::openmode openmode):
			std::istream{nullptr}, stream_{std::ifstream(fileName.data(),openmode)}
		{ 
			if(std::get_if<std::ifstream>(&stream_)->fail())
			{
				throw IOError("Failed to open file");
			}
			rdbuf(std::get<std::ifstream>(stream_).rdbuf());
		}
		UnzipperStream(std::string_view data):
			std::istream{ nullptr }, stream_{ std::istringstream{std::string{data}} }
		{
			rdbuf(std::get<std::istringstream>(stream_).rdbuf());
		}
		UnzipperStream(const UnzipperStream&) = delete;
		UnzipperStream& operator=(const UnzipperStream&) = delete;
		UnzipperStream(UnzipperStream&& o) noexcept :std::istream{nullptr}
		{
			stream_ = std::move(o.stream_);
			rdbuf(o.rdbuf());
			o.rdbuf(nullptr);
		}
		UnzipperStream& operator=(UnzipperStream&& o) noexcept
		{
			if (this != &o)
			{
				stream_ = std::move(o.stream_);
				rdbuf(o.rdbuf());
				o.rdbuf(nullptr);
			}
			return *this;
		}
		~UnzipperStream()
		{
			std::visit(CloseStream{}, stream_);
		}
		struct Buffer {
			Buffer() = default;
			Buffer(size_t size)
			{
				data = std::make_shared<char[]>(size);
				this -> size = size;
			}
			std::shared_ptr<char[]> data = nullptr;
			size_t size{ 0 };
		};

		using BufferOrFile = std::variant<Buffer, std::string>;

		inline static std::shared_ptr<UnzipperStream> MakeUnzipperStream(const BufferOrFile& data)
		{
			auto res = std::visit(MakeOperator{}, data);
			return res;
		}
		template<typename T>
		void SetFrom(std::shared_ptr<T> ptr)
		{
			unzipper_ = ptr;
		}
	private:
		std::variant<std::ifstream, std::istringstream> stream_;
		struct CloseStream
		{
			void operator()(std::ifstream& ifs) const
			{
				if (ifs.is_open())
				{
					ifs.close();
				}
			}
			void operator()(std::istringstream&){}
		};
		struct MakeOperator 
		{
			std::shared_ptr<UnzipperStream> operator()(const Buffer& buff)
			{
				return std::make_shared<UnzipperStream>(std::string{ buff.data.get(),buff.size });
			}
			std::shared_ptr<UnzipperStream> operator()(const std::string& str)
			{
				return std::make_shared<UnzipperStream>(str, std::ios_base::binary | std::ios_base::in);
			}
		};
		std::any unzipper_;
	};

	template<typename Derived>
	class IUnzipper 
	{
	public:
		void ReadFromFile(std::string_view path,bool reopen = false)
		{
			static_cast<Derived*>(this)->ReadFromFileImpl(path,reopen);
		}
		std::shared_ptr<UnzipperStream> GetStream(std::string_view part_file)
		{
			return static_cast<Derived*>(this)->GetStreamImpl(part_file);
		}
		IUnzipper(const IUnzipper&) = delete;
		IUnzipper& operator=(const IUnzipper) = delete;
		IUnzipper(IUnzipper&&) = delete;
		IUnzipper& operator=(IUnzipper&&) = delete;
	protected:
		IUnzipper() = default;
		~IUnzipper() = default;
	};

} // namespace HsBa::Slicer

#endif // !HSBA_SLICER_IUNZIPPER_HPP