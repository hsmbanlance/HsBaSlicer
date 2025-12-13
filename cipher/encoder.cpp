#include "encoder.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#include <stdexcept>
#include <cctype>

#include "base/error.hpp"

namespace HsBa::Slicer::Cipher
{
	namespace
	{
		unsigned char hex_val(char c) 
		{
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			throw InvalidArgumentError("Invalid hex char");
		}
	}

	std::string Encoder::base64_encode(const std::vector<unsigned char>& data) 
	{
		BIO* bio = BIO_new(BIO_s_mem());
		BIO* b64 = BIO_new(BIO_f_base64());
		if (!bio || !b64)
		{
			BIO_free(bio);
			BIO_free(b64);
			throw RuntimeError("Failed to create BIO");
		}
		// Do not use newlines
		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
		bio = BIO_push(b64, bio);

		if (BIO_write(bio, data.data(), static_cast<int>(data.size())) <= 0) 
		{
			BIO_free_all(bio);
			throw RuntimeError("BIO_write failed");
		}
		if (BIO_flush(bio) != 1) 
		{
			BIO_free_all(bio);
			throw RuntimeError("BIO_flush failed");
		}

		BUF_MEM* mem = nullptr;
		BIO_get_mem_ptr(bio, &mem);
		std::string out;
		if (mem && mem->length > 0) out.assign(mem->data, mem->length);

		BIO_free_all(bio);
		return out;
	}

	std::vector<unsigned char> Encoder::base64_decode(std::string_view b64) 
	{
		BIO* bio = BIO_new_mem_buf(b64.data(), static_cast<int>(b64.size()));
		BIO* b64f = BIO_new(BIO_f_base64());
		if (!bio || !b64f)
		{
			BIO_free(bio);
			BIO_free(b64f);
			throw RuntimeError("Failed to create BIO");
		}
		BIO_set_flags(b64f, BIO_FLAGS_BASE64_NO_NL);
		bio = BIO_push(b64f, bio);

		std::vector<unsigned char> out;
		out.resize(b64.size());
		int len = BIO_read(bio, out.data(), static_cast<int>(out.size()));
		if (len < 0) {
			BIO_free_all(bio);
			throw RuntimeError("BIO_read failed");
		}
		out.resize(len);
		BIO_free_all(bio);
		return out;
	}

	std::string Encoder::hex_encode(const std::vector<unsigned char>& data) 
	{
		static const char* hex_chars = "0123456789abcdef";
		std::string out;
		out.reserve(data.size() * 2);
		for (unsigned char c : data) {
			out.push_back(hex_chars[(c >> 4) & 0xF]);
			out.push_back(hex_chars[c & 0xF]);
		}
		return out;
	}

	std::vector<unsigned char> Encoder::hex_decode(std::string_view hex) 
	{
		if (hex.size() % 2 != 0) throw std::invalid_argument("hex string length must be even");
		std::vector<unsigned char> out;
		out.reserve(hex.size() / 2);
		for (size_t i = 0; i < hex.size(); i += 2) {
			unsigned char hi = hex_val(hex[i]);
			unsigned char lo = hex_val(hex[i + 1]);
			out.push_back((unsigned char)((hi << 4) | lo));
		}
		return out;
	}
} // namespace HsBa::Slicer::Cipher