#pragma once
#ifndef HSBA_SLICER_UNZIPPER_HPP
#define HSBA_SLICER_UNZIPPER_HPP

#include <unordered_map>
#include <memory>

#include <miniz.h>

#include "IUnzipper.hpp"
#include "base/delegate.hpp"

namespace HsBa::Slicer
{
	class Unzipper :public IUnzipper<Unzipper>
		,public std::enable_shared_from_this<Unzipper>,
		public Utils::EventSource<Unzipper,void,std::string_view,std::string_view>
	{
	public:
		static std::shared_ptr<Unzipper> Create()
		{
			return std::shared_ptr<Unzipper>(new Unzipper());
		}
		~Unzipper();
		friend class IUnzipper<Unzipper>;
		inline static void SetMaxMemSize(size_t size = 1024 * 1024 * 1024)
		{
			max_mem_size_ = size;
		}
	private:
		Unzipper() = default;
		void ReadFromFileImpl(std::string_view path,bool reopen);
		std::shared_ptr<UnzipperStream> GetStreamImpl(std::string_view part_file);
		Unzipper(const Unzipper&) = delete;
		Unzipper& operator=(const Unzipper&) = delete;
		Unzipper(Unzipper&&) = delete;
		Unzipper& operator=(Unzipper&&) = delete;
		inline static size_t max_mem_size_ = 1024 * 1024 * 1024; // 1GB
		inline static constexpr size_t user_buff_size = 4096;
		mz_zip_archive zip_archive_{};
		std::unordered_map<std::string, UnzipperStream::BufferOrFile> memory_cache_;
		std::string archiver_path_;
		std::string cache_dir_;
		bool is_open_{ false };
		bool use_cache_dir_{ false };
		std::shared_ptr<UnzipperStream> ReadFileTobuff(int file_index, size_t uncompsize,const std::string& part_name);
		std::shared_ptr<UnzipperStream> ReadFileToFile(int file_index, const std::string& part_name);
		void CreateBuffDir();
	};
}// namespace HsBa::Slicer

#endif // !HSBA_SLICER_UNZIPPER_HPP