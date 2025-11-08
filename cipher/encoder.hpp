#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace HsBa::Slicer::Cipher {

	class Encoder {
	public:
		// Base64
		static std::string base64_encode(const std::vector<unsigned char>& data);
		inline static std::string base64_encode(std::string_view data) {
			return base64_encode(std::vector<unsigned char>(data.begin(), data.end()));
		}
		static std::vector<unsigned char> base64_decode(std::string_view b64);
		inline static std::string base64_decode_to_string(std::string_view b64) {
			auto vec = base64_decode(b64);
			return std::string(vec.begin(), vec.end());
		}

		// Hex
		static std::string hex_encode(const std::vector<unsigned char>& data);
		inline static std::string hex_encode(std::string_view data) {
			return hex_encode(std::vector<unsigned char>(data.begin(), data.end()));
		}
		static std::vector<unsigned char> hex_decode(std::string_view hex);
		inline static std::string hex_decode_to_string(std::string_view hex) {
			auto vec = hex_decode(hex);
			return std::string(vec.begin(), vec.end());
		}
	};

} // namespace HsBa::Slicer::Cipher
