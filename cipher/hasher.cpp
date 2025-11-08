#include "hasher.hpp"

#include <openssl/evp.h>

#include <stdexcept>
#include <vector>
#include <sstream>
#include <iomanip>

#include "base/error.hpp"

namespace HsBa::Slicer::Cipher
{
	namespace 
	{
		std::string to_hex(const unsigned char* data, size_t len) 
		{
			static const char* hex_chars = "0123456789abcdef";
			std::string out;
			out.reserve(len * 2);
			for (size_t i = 0; i < len; ++i) 
			{
				unsigned char c = data[i];
				out.push_back(hex_chars[(c >> 4) & 0xF]);
				out.push_back(hex_chars[c & 0xF]);
			}
			return out;
		}

		std::string digest_hex(const std::vector<unsigned char>& data, const EVP_MD* md) 
		{
			EVP_MD_CTX* ctx = EVP_MD_CTX_new();
			if (!ctx) 
				throw RuntimeError("EVP_MD_CTX_new failed");
			if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) 
			{
				EVP_MD_CTX_free(ctx);
				throw RuntimeError("EVP_DigestInit_ex failed");
			}
			if (EVP_DigestUpdate(ctx, data.data(), data.size()) != 1) 
			{
				EVP_MD_CTX_free(ctx);
				throw RuntimeError("EVP_DigestUpdate failed");
			}
			unsigned char out[EVP_MAX_MD_SIZE];
			unsigned int outlen = 0;
			if (EVP_DigestFinal_ex(ctx, out, &outlen) != 1) 
			{
				EVP_MD_CTX_free(ctx);
				throw RuntimeError("EVP_DigestFinal_ex failed");
			}
			EVP_MD_CTX_free(ctx);
			return to_hex(out, outlen);
		}
	}

	std::string Hasher::md5_hex(const std::vector<unsigned char>& data) 
	{
		return digest_hex(data, EVP_md5());
	}

	std::string Hasher::sha1_hex(const std::vector<unsigned char>& data) 
	{
		return digest_hex(data, EVP_sha1());
	}

	std::string Hasher::sha256_hex(const std::vector<unsigned char>& data) 
	{
		return digest_hex(data, EVP_sha256());
	}
} // namespace HsBa::Slicer::Cipher