#pragma once
#ifndef HSBA_SLICER_ZIPPER_HPP
#define HSBA_SLICER_ZIPPER_HPP

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <variant>

#include <miniz.h>

#include "IZipper.hpp"
#include "base/delegate.hpp"

namespace HsBa::Slicer
{
	enum class MinizCompression
	{
		Undefine,
		No,
		Fast,
		Tight,
		Unknown
	};

	//Zipper use miniz
	class Zipper final : public IZipper, public Utils::EventSource<Zipper, void, double, std::string_view>
	{
	public:
		Zipper() = default;
		Zipper(MinizCompression compression);
		~Zipper() = default;
		Zipper(const Zipper&) = delete;
		Zipper& operator=(const Zipper&) = delete;
		Zipper(Zipper&&) noexcept = delete;
		Zipper& operator=(Zipper&&) noexcept = delete;
		void AddByteFile(std::string_view name, const std::string& data) override;
		void AddFile(std::string_view name, std::string_view path) override;
		//To add duplicate file, filename add "_duplicate"
		void AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data) override;
		void AddFileIgnoreDuplicate(std::string_view name, std::string_view path) override;
		void Save(std::string_view filePath) override;
	private:
		struct Bytes
		{
			std::string data;
		};
		using BytesFileName = std::variant<Bytes, std::string>;
		using ByteFiles = std::unordered_map<std::string, BytesFileName>;
		ByteFiles byteFilesWaitCompress_;
		mz_uint compression_ = MZ_DEFAULT_COMPRESSION;
		const std::string duplicate_addition = "_duplicate";
		mz_bool AddAllToZip(/*in*/mz_zip_archive& archiver);
		mz_bool ZipAddFile(/*ref*/mz_zip_archive& archiver, const std::string& name, const std::string& path) const;
		mz_bool ZipAddMember(/*ref*/mz_zip_archive& archiver, const std::string& name, const Bytes& bytes) const;
	};

	void MiniZExtractFile(std::string_view archive_path, std::string_view output_path);

	std::unordered_map<std::string, std::string> MiniZExtractFileToBuffer(
		std::string_view archive_path);
}// namespace HsBa::Slicer

#endif // !HSBA_SLICER_ZIPPER_HPP