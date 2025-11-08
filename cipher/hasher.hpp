#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace HsBa::Slicer::Cipher {

	class Hasher {
	public:
		// Compute MD5, SHA1, SHA256 of data and return hex string
		static std::string md5_hex(const std::vector<unsigned char>& data);
		static std::string sha1_hex(const std::vector<unsigned char>& data);
		static std::string sha256_hex(const std::vector<unsigned char>& data);
		inline static std::string md5_hex(std::string_view data) {
			return md5_hex(std::vector<unsigned char>(data.begin(), data.end()));
		}
		inline static std::string sha1_hex(const std::string_view data) {
			return sha1_hex(std::vector<unsigned char>(data.begin(), data.end()));
		}
		inline static std::string sha256_hex(const std::string_view data) {
			return sha256_hex(std::vector<unsigned char>(data.begin(), data.end()));
		}
	};

} // namespace HsBa::Slicer::Cipher
