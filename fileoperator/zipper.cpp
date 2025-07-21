#include "zipper.hpp"

#include <type_traits>
#include <filesystem>

#include "base/error.hpp"
#include "base/encoding_convert.hpp"

namespace HsBa::Slicer
{
	Zipper::Zipper(MinizCompression compression)
	{
		if (compression == MinizCompression::Undefine || compression == MinizCompression::Unknown)
		{
			throw NotSupportedError("Unknow or Undefine miniz compression");
		}
		switch (compression) {
		case MinizCompression::No:
			compression_ = MZ_NO_COMPRESSION;
			break;
		case MinizCompression::Fast:
			compression_ = MZ_BEST_SPEED;
			break;
		case MinizCompression::Tight:
			compression_ = MZ_BEST_COMPRESSION;
			break;
		}
	}

	void Zipper::AddByteFile(std::string_view name, const std::string& data)
	{
		std::string ansi_name = utf8_to_local(std::string{ name });
		auto [add, add_res] = byteFilesWaitCompress_.emplace(ansi_name, Bytes{ data });
		if (!add_res)
		{
			throw InvalidArgumentError("Duplicate name files");
		}
	}
	void Zipper::AddFile(std::string_view name, std::string_view path)
	{
		std::string ansi_name = utf8_to_local(std::string{ name });
		auto [add, add_res] = byteFilesWaitCompress_.emplace(ansi_name, std::string(path));
		if (!add_res)
		{
			throw InvalidArgumentError("Duplicate name files");
		}
	}

	void Zipper::AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data)
	{
		std::string ansi_name = utf8_to_local(std::string{ name });
		if (byteFilesWaitCompress_.count(std::string{ ansi_name }))
		{
			byteFilesWaitCompress_.emplace((std::string(ansi_name) + duplicate_addition), Bytes{ data });
		}
		else
		{
			byteFilesWaitCompress_.emplace(ansi_name, Bytes{ data });
		}
	}
	void Zipper::AddFileIgnoreDuplicate(std::string_view name, std::string_view path)
	{
		std::string ansi_name = utf8_to_local(std::string{ name });
		if (byteFilesWaitCompress_.count(std::string{ ansi_name }))
		{
			byteFilesWaitCompress_.emplace((std::string(ansi_name) + duplicate_addition), std::string(path));
		}
		else
		{
			byteFilesWaitCompress_.emplace(ansi_name, std::string(path));
		}
	}
	void Zipper::Save(std::string_view filePath)
	{
		std::string path = std::filesystem::path(filePath).make_preferred().string();
		path = utf8_to_local(path);
		mz_zip_archive archiver{};
		mz_zip_zero_struct(&archiver);
		mz_bool status = mz_zip_writer_init_file(&archiver, path.c_str(), 0);
		mz_bool add_files_status = AddAllToZip(archiver);
		status = mz_zip_writer_finalize_archive(&archiver);
		if (status <= MZ_OK && add_files_status <= MZ_OK)
		{
			throw IOError("Failed to save zip file");
		}
	}


	mz_bool Zipper::AddAllToZip(mz_zip_archive& archiver)
	{
		mz_bool status = MZ_OK;
		for (const auto& [name, bytes] : byteFilesWaitCompress_)
		{
			status = std::visit([&archiver, &name, this](auto&& arg)
				{
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<std::string, T>)
					{
						return ZipAddFile(archiver, name, arg);
					}
					else if constexpr (std::is_same_v<Bytes, T>)
					{
						return ZipAddMember(archiver, name, arg);
					}
				}, bytes);
			if (status <= MZ_OK)
			{
				return status;
			}
		}
		return status;
	}

	mz_bool Zipper::ZipAddFile(mz_zip_archive& archiver, const std::string& name, const std::string& path) const
	{
		return mz_zip_writer_add_file(&archiver, name.c_str(), path.c_str(), NULL, 0, compression_);
	}

	mz_bool Zipper::ZipAddMember(mz_zip_archive& archiver, const std::string& name, const Bytes& bytes) const
	{
		return mz_zip_writer_add_mem(&archiver, name.c_str(), bytes.data.data(), bytes.data.size(), compression_);
	}

}// namespace HsBa::Slicer

