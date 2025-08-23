﻿#pragma once
#ifndef HSBA_SLICER_BIT7Z_UNZIPPER_HPP
#define HSBA_SLICER_BIT7Z_UNZIPPER_HPP

#include <unordered_map>
#include <memory>

#ifdef USE_BIT7Z
#include <bit7z/bit7z.hpp>
#include "bit7z_def.hpp"
#endif // USE_BIT7Z

#include "IUnzipper.hpp"
#include "base/delegate.hpp"

namespace HsBa::Slicer
{
#ifdef USE_BIT7Z

	class Bit7ZUnzipper :public IUnzipper<Bit7ZUnzipper>
		,public std::enable_shared_from_this<Bit7ZUnzipper>,
		public Utils::EventSource<Bit7ZUnzipper,void,std::string_view,std::string_view>
	{
	public:
		static std::shared_ptr<Bit7ZUnzipper> Create(const std::string& dll_path)
		{
			return std::shared_ptr<Bit7ZUnzipper>(new Bit7ZUnzipper(dll_path));
		}
		void SetPassword(std::string_view password) 
		{
			password_ = password;
			if (archiver_) 
			{
				archiver_->setPassword(password_);
			}
		}
		inline static void SetMaxMemSize(size_t size = 1024 * 1024 * 1024)
		{
			max_mem_size_ = size;
		}
		~Bit7ZUnzipper();
		friend class IUnzipper<Bit7ZUnzipper>;
	private:
		Bit7ZUnzipper(const std::string& dll_path = HSBA_7Z_DLL) :
			dll_path_{ dll_path } {
		}
		void ReadFromFileImpl(std::string_view path, bool reopen);
		std::shared_ptr<UnzipperStream> GetStreamImpl(std::string_view part_file);
		Bit7ZUnzipper(const Bit7ZUnzipper&) = delete;
		Bit7ZUnzipper& operator=(const Bit7ZUnzipper&) = delete;
		Bit7ZUnzipper(Bit7ZUnzipper&&) = delete;
		Bit7ZUnzipper& operator=(Bit7ZUnzipper&&) = delete;
		inline static size_t max_mem_size_ = 1024 * 1024 * 1024; // 1GB
		inline static constexpr size_t user_buff_size = 4096;
		std::string dll_path_;
		std::unique_ptr<bit7z::BitArchiveReader> archiver_;
		std::unordered_map<std::string, UnzipperStream::BufferOrFile> memory_cache_;
		std::string archiver_path_;
		std::string cache_dir_;
		std::string password_;
		bool is_open_{ false };
		bool use_cache_dir_{ false };
		using It = bit7z::BitInputArchive::ConstIterator;
		std::shared_ptr<UnzipperStream> ReadFileTobuff(It it, size_t uncompsize, const std::string& part_name);
		std::shared_ptr<UnzipperStream> ReadFileToFile(It it, const std::string& part_name);
		void CreateBuffDir();
	};
#endif // USE_BIT7Z
}// namespace HsBa::Slicer

#endif // !HSBA_SLICER_BIT7Z_UNZIPPER_HPP