#include "bit7z_unzipper.hpp"

#include <boost/uuid.hpp>

namespace HsBa::Slicer
{
#ifdef USE_BIT7Z
	Bit7ZUnzipper::~Bit7ZUnzipper()
	{
		if (is_open_)
		{
			is_open_ = false;
		}
		if (use_cache_dir_)
		{
			if (std::filesystem::exists(cache_dir_))
			{
				std::filesystem::remove_all(cache_dir_);
			}
		}
	}
	void Bit7ZUnzipper::ReadFromFileImpl(std::string_view path, bool reopen)
	{
		if (is_open_)
		{
			if (path == archiver_path_ && !reopen)
			{
				return;
			}
			is_open_ = false;
		}
		bit7z::Bit7zLibrary lib{ dll_path_ };
		archiver_ = std::make_unique<bit7z::BitArchiveReader>
			(lib, archiver_path_, bit7z::ArchiveStartOffset::FileStart, bit7z::BitFormat::Auto, password_);
		archiver_path_ = path;
		is_open_ = true;
		if (use_cache_dir_)
		{
			if (std::filesystem::exists(cache_dir_))
			{
				std::filesystem::remove_all(cache_dir_);
			}
		}
		use_cache_dir_ = false;
		memory_cache_.clear();
	}
	void Bit7ZUnzipper::CreateBuffDir()
	{
		if (!is_open_ || use_cache_dir_)
		{
			return;
		}
		boost::uuids::name_generator_latest gen{ boost::uuids::ns::url() };
		auto uuid = gen(archiver_path_.c_str());
		std::filesystem::path cur_path = std::filesystem::current_path();
		std::filesystem::path cache_path = cur_path / boost::uuids::to_string(uuid);
		if (std::filesystem::exists(cache_path))
		{
			std::filesystem::remove_all(cache_path);
		}
		std::filesystem::create_directories(cache_path);
		cache_dir_ = cache_path.string();
		use_cache_dir_ = true;
	}

	std::shared_ptr<UnzipperStream> Bit7ZUnzipper::GetStreamImpl(std::string_view part_file)
	{
		if (!is_open_)
		{
			throw IOError(std::format("Zip file {} is not opened.", archiver_path_));
		}
		RaiseEvent(archiver_path_, part_file);
		// 使用保存的已缓存
		if (memory_cache_.find(std::string{ part_file }) != memory_cache_.end())
		{
			const auto& cache = memory_cache_.at(std::string{ part_file });
			auto stream = UnzipperStream::MakeUnzipperStream(cache);
			stream->SetFrom(shared_from_this());
			return stream;
		}
		const auto it = archiver_->find(bit7z::tstring(part_file));
		if (it == archiver_->end())
		{
			throw IOError("File not found in zip: " + std::string(part_file));
		}
		size_t uncomp_size = it->size();
		if (uncomp_size == 0)
		{
			auto stream = std::make_shared<UnzipperStream>("");
			stream->SetFrom(shared_from_this());
			return stream;
		}
		if (uncomp_size <= max_mem_size_)
		{
			return ReadFileTobuff(it, uncomp_size, std::string{ part_file });
		}
		return ReadFileToFile(it, std::string{ part_file });
	}

	std::shared_ptr<UnzipperStream> Bit7ZUnzipper::ReadFileTobuff(It it, size_t uncompsize, const std::string& part_name)
	{
		UnzipperStream::Buffer buff(it->size());
		archiver_->extractTo(reinterpret_cast<bit7z::byte_t*>(buff.data.get()), it->size(), it->index());
		auto stream = UnzipperStream::MakeUnzipperStream(buff);
		stream->SetFrom(shared_from_this());
		memory_cache_[std::string{ part_name }] = buff;
		return stream;
	}

	std::shared_ptr<UnzipperStream> Bit7ZUnzipper::ReadFileToFile(It it, const std::string& part_name)
	{
		if (!use_cache_dir_)
		{
			CreateBuffDir();
		}
		boost::uuids::name_generator_latest gen{ boost::uuids::ns::url() };
		auto uuid = gen(part_name.c_str());
		std::filesystem::path cur_path = std::filesystem::path{ cache_dir_ } / boost::uuids::to_string(uuid);
		if (std::filesystem::exists(cur_path))
		{
			std::filesystem::remove_all(cur_path);
		}
		std::ofstream ofs(cur_path,std::ios_base::out | std::ios_base::binary);
		archiver_->extractTo(ofs, it->index());
		ofs.close();
		auto stream = UnzipperStream::MakeUnzipperStream(cur_path.string());
		stream->SetFrom(shared_from_this());
		memory_cache_[std::string{ part_name }] = cur_path.string();
		return stream;
	}
#endif // USE_BIT7Z
} // namespace HsBa::Slicer