#include "bit7z_zipper.hpp"

#include <format>

#ifdef USE_BIT7Z
#include <bit7z/bitfileextractor.hpp>
#include <bit7z/bitfilecompressor.hpp>
#include <bit7z/bitexception.hpp>
#endif // USE_BIT7Z

#include "base/error.hpp"
#include "base/encoding_convert.hpp"

namespace HsBa::Slicer
{
#ifdef USE_BIT7Z
	void Bit7zExtract(const std::string& archive, const std::string& outdir, const std::string& password,
		const std::string& dll_path)
	{
		bit7z::Bit7zLibrary lib{ dll_path };
		bit7z::BitFileExtractor ex{ lib, bit7z::BitFormat::SevenZip };
		if (!password.empty())
		{
			ex.setPassword(password);
		}
		ex.extract(archive, outdir);
	}
	void Bit7zExtract(const std::string& archive,
		std::map<std::string, std::vector<bit7z::byte_t>>& bufs,
		const std::string& password, const std::string& dll_path)
	{
		bit7z::Bit7zLibrary lib{ dll_path };
		bit7z::BitFileExtractor ex{ lib, bit7z::BitFormat::SevenZip };
		if (!password.empty())
		{
			ex.setPassword(password);
		}
		ex.extract(archive, bufs);
	}

	void Bit7zZipper::AddByteFile(std::string_view name, const std::vector<bit7z::byte_t>& data)
	{
		auto [add, add_res] = byteFilesWaitCompress_.emplace(name, data);
		if (!add_res)
		{
			throw InvalidArgumentError("Duplicate name files");
		}
	}

	void Bit7zZipper::AddByteFile(std::string_view name, const std::string& data)
	{
		std::vector<bit7z::byte_t> bytes(data.size());
		for (size_t i = 0; i != data.size(); ++i)
		{
			bytes[i] = static_cast<bit7z::byte_t>(data[i]);
		}
		AddByteFile(name, bytes);
	}

	void Bit7zZipper::AddFile(std::string_view name, std::string_view path)
	{
		auto [add, add_res] = byteFilesWaitCompress_.emplace(name, std::string(path));
		if (!add_res)
		{
			throw InvalidArgumentError("Duplicate name files");
		}
	}

	void Bit7zZipper::AddByteFileIgnoreDuplicate(std::string_view name, const std::vector<bit7z::byte_t>& data)
	{
		if (byteFilesWaitCompress_.count(std::string{ name }))
		{
			byteFilesWaitCompress_.emplace((std::string(name) + duplicate_addition), data);
		}
		else
		{
			byteFilesWaitCompress_.emplace(name, Bytes{ data });
		}
	}

	void Bit7zZipper::AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data)
	{
		std::vector<bit7z::byte_t> bytes(data.size());
		for (size_t i = 0; i != data.size(); ++i)
		{
			bytes[i] = static_cast<bit7z::byte_t>(data[i]);
		}
		AddByteFile(name, bytes);
	}

	void Bit7zZipper::AddFileIgnoreDuplicate(std::string_view name, std::string_view path)
	{
		if (byteFilesWaitCompress_.count(std::string{ name }))
		{
			byteFilesWaitCompress_.emplace((std::string(name) + duplicate_addition), std::string(path));
		}
		else
		{
			byteFilesWaitCompress_.emplace(name, std::string(path));
		}
	}

	void Bit7zZipper::Save(std::string_view filePath)
	{
		std::string path = std::filesystem::path(filePath).make_preferred().string();
		path = utf8_to_local(path);
		try
		{
			bit7z::Bit7zLibrary lib{ dll_path_ };
			switch (format_)
			{
			case HsBa::Slicer::ZipperFormat::Zip:
			{
				bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::Zip);
				SaveAllFile(compress, path);
				break;
			}
			case HsBa::Slicer::ZipperFormat::SevenZip:
			{
				bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::SevenZip);
				SaveAllFile(compress, path);
				break;
			}
			case HsBa::Slicer::ZipperFormat::XZ:
			{
				bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::Xz);
				SaveAllFile(compress, path);
				break;
			}
			case HsBa::Slicer::ZipperFormat::BZIP2:
			{
				bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::BZip2);
				SaveAllFile(compress, path);
				break;
			}
			case HsBa::Slicer::ZipperFormat::GZIP:
			{
				bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::GZip);
				SaveAllFile(compress, path);
				break;
			}
			case HsBa::Slicer::ZipperFormat::TAR:
			{
				bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::Tar);
				SaveAllFile(compress, path);
				break;
			}
			default:
				throw NotSupportedError("Unsupported format");
				break;
			}
		}
		catch (const bit7z::BitException& e)
		{
			throw IOError(std::format("Failed to save zipper file, see {}", e.what()));
		}
	}

	void Bit7zZipper::SaveAllFile(bit7z::BitArchiveWriter& compress,const std::string& path)
	{
		size_t fileCount = byteFilesWaitCompress_.size();
		size_t currentFileIndex = 0;
		compress.setPassword(password_);
		compress.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
		for (const auto& [name, bytes] : byteFilesWaitCompress_)
		{
			std::visit([&name, &compress, this](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<std::string, T>)
				{
					compress.addFile(arg, name);
				}
				else if constexpr (std::is_same_v<Bytes, T>)
				{
					compress.addFile(arg, name);
				}
				},
				bytes
			);
			double progress = static_cast<double>(currentFileIndex) / fileCount;
			RaiseEvent(progress, name);
		}
		compress.compressTo(path);
	}

#endif // USE_BIT7Z
}// namespace HsBa::Slicer