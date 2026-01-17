#include "encrypt.hpp"

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <stdexcept>
#include <cstring>

#include "base/error.hpp"
#include "hasher.hpp"

namespace HsBa::Slicer::Cipher
{

	namespace
	{
		// Derive AES_KEY_SIZE-byte key and AES_IV_SIZE-byte iv from a password using a simple SHA256-based scheme.
		void derive_key_iv(std::string_view password, unsigned char key[AES_KEY_SIZE], unsigned char iv[AES_IV_SIZE]) 
		{

			auto hash = Hasher::sha256_hex(password);

			// key: first 32 bytes (SHA256 gives 32)
			memcpy(key, hash.c_str(), AES_KEY_SIZE);

			memcpy(iv, hash.c_str(), AES_IV_SIZE);
		}

		// Derive 24-byte key and 8-byte iv for 3DES
		void derive_3des_key_iv(std::string_view password, unsigned char key[24], unsigned char iv[8]) 
		{
			auto hash = Hasher::sha256_hex(password);

			// Use first DES3_KEY_SIZE bytes for 3DES key (SHA256 gives 32 bytes)
			memcpy(key, hash.c_str(), DES3_KEY_SIZE);

			memcpy(iv, hash.c_str(), DES3_IV_SIZE);
		}

		std::string openssl_err() 
		{
			unsigned long e = ERR_get_error();
			if (e == 0) return std::string();
			char buf[256];
			ERR_error_string_n(e, buf, sizeof(buf));
			return std::string(buf);
		}
	} // namespace

	std::vector<unsigned char> Encrypt::aes256_cbc_encrypt(const std::vector<unsigned char>& plaintext, std::string_view password) 
	{
		unsigned char key[AES_KEY_SIZE];
		unsigned char iv[AES_IV_SIZE];
		derive_key_iv(password, key, iv);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx) throw RuntimeError("EVP_CIPHER_CTX_new failed");

		if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptInit_ex failed");
		}

		std::vector<unsigned char> out;
		out.resize(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
		int out_len1 = 0;
		if (EVP_EncryptUpdate(ctx, out.data(), &out_len1, plaintext.data(), static_cast<int>(plaintext.size())) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptUpdate failed");
		}

		int out_len2 = 0;
		if (EVP_EncryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptFinal_ex failed");
		}

		out.resize(out_len1 + out_len2);
		EVP_CIPHER_CTX_free(ctx);
		return out;
	}

	std::vector<unsigned char> Encrypt::aes256_cbc_decrypt(const std::vector<unsigned char>& cipher, std::string_view password) 
	{
		unsigned char key[AES_KEY_SIZE];
		unsigned char iv[AES_IV_SIZE];
		derive_key_iv(password, key, iv);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx)
			throw RuntimeError("EVP_CIPHER_CTX_new failed");

		if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptInit_ex failed");
		}

		std::vector<unsigned char> out;
		out.resize(cipher.size());
		int out_len1 = 0;
		if (EVP_DecryptUpdate(ctx, out.data(), &out_len1, cipher.data(), static_cast<int>(cipher.size())) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptUpdate failed");
		}

		int out_len2 = 0;
		if (EVP_DecryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptFinal_ex failed - likely bad password or corrupted data");
		}

		out.resize(out_len1 + out_len2);
		EVP_CIPHER_CTX_free(ctx);
		return out;
	}

	std::vector<unsigned char> Encrypt::aes256_ecb_encrypt(const std::vector<unsigned char>& plaintext, std::string_view password) 
	{
		unsigned char key[AES_KEY_SIZE];
		unsigned char iv[AES_IV_SIZE];
		derive_key_iv(password, key, iv); // iv unused for ECB

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx)
			throw RuntimeError("EVP_CIPHER_CTX_new failed");

		if (EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptInit_ex failed");
		}

		std::vector<unsigned char> out;
		out.resize(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_ecb()));
		int out_len1 = 0;
		if (EVP_EncryptUpdate(ctx, out.data(), &out_len1, plaintext.data(), static_cast<int>(plaintext.size())) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptUpdate failed");
		}

		int out_len2 = 0;
		if (EVP_EncryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1)
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptFinal_ex failed");
		}

		out.resize(out_len1 + out_len2);
		EVP_CIPHER_CTX_free(ctx);
		return out;
	}

	std::vector<unsigned char> Encrypt::aes256_ecb_decrypt(const std::vector<unsigned char>& cipher, std::string_view password) 
	{
		unsigned char key[AES_KEY_SIZE];
		unsigned char iv[AES_IV_SIZE];
		derive_key_iv(password, key, iv);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx)
			throw RuntimeError("EVP_CIPHER_CTX_new failed");

		if (EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptInit_ex failed");
		}

		std::vector<unsigned char> out;
		out.resize(cipher.size());
		int out_len1 = 0;
		if (EVP_DecryptUpdate(ctx, out.data(), &out_len1, cipher.data(), static_cast<int>(cipher.size())) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptUpdate failed");
		}

		int out_len2 = 0;
		if (EVP_DecryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptFinal_ex failed - likely bad password or corrupted data");
		}

		out.resize(out_len1 + out_len2);
		EVP_CIPHER_CTX_free(ctx);
		return out;
	}

	std::vector<unsigned char> Encrypt::aes256_cbc_encrypt_with_iv(const std::vector<unsigned char>& plaintext, std::string_view password, const std::vector<unsigned char>& iv_in) 
	{
		if (iv_in.size() != AES_IV_SIZE) throw std::invalid_argument("IV must be " + std::to_string(AES_IV_SIZE) + " bytes for AES-256-CBC");
		unsigned char key[AES_KEY_SIZE];
		unsigned char iv[AES_IV_SIZE];
		derive_key_iv(password, key, iv); // we'll ignore derived iv and use iv_in
		memcpy(iv, iv_in.data(), AES_IV_SIZE);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx)
			throw RuntimeError("EVP_CIPHER_CTX_new failed");

		if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptInit_ex failed");
		}

		std::vector<unsigned char> out;
		out.resize(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
		int out_len1 = 0;
		if (EVP_EncryptUpdate(ctx, out.data(), &out_len1, plaintext.data(), static_cast<int>(plaintext.size())) != 1)
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptUpdate failed");
		}

		int out_len2 = 0;
		if (EVP_EncryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1)
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptFinal_ex failed");
		}

		out.resize(out_len1 + out_len2);
		EVP_CIPHER_CTX_free(ctx);
		return out;
	}

	std::vector<unsigned char> Encrypt::aes256_cbc_decrypt_with_iv(const std::vector<unsigned char>& cipher, std::string_view password, const std::vector<unsigned char>& iv_in) 
	{
		if (iv_in.size() != AES_IV_SIZE)
			throw std::invalid_argument("IV must be " + std::to_string(AES_IV_SIZE) + " bytes for AES-256-CBC");
		unsigned char key[AES_KEY_SIZE];
		unsigned char iv[AES_IV_SIZE];
		derive_key_iv(password, key, iv);
		memcpy(iv, iv_in.data(), AES_IV_SIZE);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx)
			throw RuntimeError("EVP_CIPHER_CTX_new failed");

		if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1)
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptInit_ex failed");
		}

		std::vector<unsigned char> out;
		out.resize(cipher.size());
		int out_len1 = 0;
		if (EVP_DecryptUpdate(ctx, out.data(), &out_len1, cipher.data(), static_cast<int>(cipher.size())) != 1)
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptUpdate failed");
		}

		int out_len2 = 0;
		if (EVP_DecryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1)
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptFinal_ex failed - likely bad password or corrupted data");
		}

		out.resize(out_len1 + out_len2);
		EVP_CIPHER_CTX_free(ctx);
		return out;
	}

	std::vector<unsigned char> Encrypt::des3_ecb_encrypt(const std::vector<unsigned char>& plaintext, std::string_view password) 
	{
		unsigned char key[DES3_KEY_SIZE];
		unsigned char iv[DES3_IV_SIZE];
		derive_3des_key_iv(password, key, iv);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx)
			throw RuntimeError("EVP_CIPHER_CTX_new failed");

		if (EVP_EncryptInit_ex(ctx, EVP_des_ede3_ecb(), nullptr, key, nullptr) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptInit_ex failed");
		}

		std::vector<unsigned char> out;
		out.resize(plaintext.size() + EVP_CIPHER_block_size(EVP_des_ede3_ecb()));
		int out_len1 = 0;
		if (EVP_EncryptUpdate(ctx, out.data(), &out_len1, plaintext.data(), static_cast<int>(plaintext.size())) != 1)
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptUpdate failed");
		}

		int out_len2 = 0;
		if (EVP_EncryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1)
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptFinal_ex failed");
		}

		out.resize(out_len1 + out_len2);
		EVP_CIPHER_CTX_free(ctx);
		return out;
	}

	std::vector<unsigned char> Encrypt::des3_ecb_decrypt(const std::vector<unsigned char>& cipher, std::string_view password)
	{
		unsigned char key[DES3_KEY_SIZE];
		unsigned char iv[DES3_IV_SIZE];
		derive_3des_key_iv(password, key, iv);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx)
			throw RuntimeError("EVP_CIPHER_CTX_new failed");

		if (EVP_DecryptInit_ex(ctx, EVP_des_ede3_ecb(), nullptr, key, nullptr) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptInit_ex failed");
		}

		std::vector<unsigned char> out;
		out.resize(cipher.size());
		int out_len1 = 0;
		if (EVP_DecryptUpdate(ctx, out.data(), &out_len1, cipher.data(), static_cast<int>(cipher.size())) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptUpdate failed");
		}

		int out_len2 = 0;
		if (EVP_DecryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1)
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptFinal_ex failed - likely bad password or corrupted data");
		}

		out.resize(out_len1 + out_len2);
		EVP_CIPHER_CTX_free(ctx);
		return out;
	}

	std::vector<unsigned char> Encrypt::des3_cbc_encrypt_with_iv(const std::vector<unsigned char>& plaintext, std::string_view password, const std::vector<unsigned char>& iv_in) 
	{
		if (iv_in.size() != DES3_IV_SIZE) throw std::invalid_argument("IV must be " + std::to_string(DES3_IV_SIZE) + " bytes for 3DES-CBC");
		unsigned char key[DES3_KEY_SIZE];
		unsigned char iv[DES3_IV_SIZE];
		derive_3des_key_iv(password, key, iv);
		memcpy(iv, iv_in.data(), DES3_IV_SIZE);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx)
			throw RuntimeError("EVP_CIPHER_CTX_new failed");

		if (EVP_EncryptInit_ex(ctx, EVP_des_ede3_cbc(), nullptr, key, iv) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptInit_ex failed");
		}

		std::vector<unsigned char> out;
		out.resize(plaintext.size() + EVP_CIPHER_block_size(EVP_des_ede3_cbc()));
		int out_len1 = 0;
		if (EVP_EncryptUpdate(ctx, out.data(), &out_len1, plaintext.data(), static_cast<int>(plaintext.size())) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptUpdate failed");
		}

		int out_len2 = 0;
		if (EVP_EncryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_EncryptFinal_ex failed");
		}

		out.resize(out_len1 + out_len2);
		EVP_CIPHER_CTX_free(ctx);
		return out;
	}

	std::vector<unsigned char> Encrypt::des3_cbc_decrypt_with_iv(const std::vector<unsigned char>& cipher, std::string_view password, const std::vector<unsigned char>& iv_in) 
	{
		if (iv_in.size() != 8) throw std::invalid_argument("IV must be 8 bytes for 3DES-CBC");
		unsigned char key[24];
		unsigned char iv[8];
		derive_3des_key_iv(password, key, iv);
		memcpy(iv, iv_in.data(), 8);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx)
			throw RuntimeError("EVP_CIPHER_CTX_new failed");

		if (EVP_DecryptInit_ex(ctx, EVP_des_ede3_cbc(), nullptr, key, iv) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptInit_ex failed");
		}

		std::vector<unsigned char> out;
		out.resize(cipher.size());
		int out_len1 = 0;
		if (EVP_DecryptUpdate(ctx, out.data(), &out_len1, cipher.data(), static_cast<int>(cipher.size())) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptUpdate failed");
		}

		int out_len2 = 0;
		if (EVP_DecryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1) 
		{
			EVP_CIPHER_CTX_free(ctx);
			throw RuntimeError("EVP_DecryptFinal_ex failed - likely bad password or corrupted data");
		}

		out.resize(out_len1 + out_len2);
		EVP_CIPHER_CTX_free(ctx);
		return out;
	}

	std::vector<unsigned char> Encrypt::rsa_public_encrypt_pem(std::string_view public_pem, const std::vector<unsigned char>& plaintext) 
	{
		BIO* bio = BIO_new_mem_buf(public_pem.data(), static_cast<int>(public_pem.size()));
		if (!bio)
			throw RuntimeError("BIO_new_mem_buf failed");
		EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
		BIO_free(bio);
		if (!pkey)
			throw RuntimeError(std::string("PEM_read_bio_PUBKEY failed: ") + openssl_err());

		EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
		if (!ctx)
		{
			EVP_PKEY_free(pkey);
			throw RuntimeError("EVP_PKEY_CTX_new failed");
		}
		if (EVP_PKEY_encrypt_init(ctx) <= 0)
		{
			EVP_PKEY_free(pkey);
			EVP_PKEY_CTX_free(ctx);
			throw RuntimeError("EVP_PKEY_encrypt_init failed");
		}
		if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) 
		{
			EVP_PKEY_free(pkey);
			EVP_PKEY_CTX_free(ctx);
			throw RuntimeError("EVP_PKEY_CTX_set_rsa_padding failed"); 
		}

		size_t outlen = 0;
		if (EVP_PKEY_encrypt(ctx, nullptr, &outlen, plaintext.data(), plaintext.size()) <= 0) 
		{
			EVP_PKEY_free(pkey);
			EVP_PKEY_CTX_free(ctx);
			throw RuntimeError("EVP_PKEY_encrypt size query failed"); 
		}
		std::vector<unsigned char> out(outlen);
		if (EVP_PKEY_encrypt(ctx, out.data(), &outlen, plaintext.data(), plaintext.size()) <= 0) 
		{
			EVP_PKEY_free(pkey);
			EVP_PKEY_CTX_free(ctx);
			throw RuntimeError(std::string("EVP_PKEY_encrypt failed: ") + openssl_err()); 
		}
		out.resize(outlen);
		EVP_PKEY_free(pkey);
		EVP_PKEY_CTX_free(ctx);
		return out;
	}

	std::pair<std::string, std::string> Encrypt::rsa_generate_keypair_pem(int bits) {
		EVP_PKEY* pkey = nullptr;
		EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
		if (!pctx) throw RuntimeError("EVP_PKEY_CTX_new_id failed");
		if (EVP_PKEY_keygen_init(pctx) <= 0) { EVP_PKEY_CTX_free(pctx); throw RuntimeError("EVP_PKEY_keygen_init failed"); }
		if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, bits) <= 0) { EVP_PKEY_CTX_free(pctx); throw RuntimeError("EVP_PKEY_CTX_set_rsa_keygen_bits failed"); }
		if (EVP_PKEY_keygen(pctx, &pkey) <= 0) { EVP_PKEY_CTX_free(pctx); throw RuntimeError("EVP_PKEY_keygen failed"); }
		EVP_PKEY_CTX_free(pctx);

		BIO* bio_priv = BIO_new(BIO_s_mem());
		BIO* bio_pub = BIO_new(BIO_s_mem());
		if (!bio_priv || !bio_pub) { if (bio_priv) BIO_free(bio_priv); if (bio_pub) BIO_free(bio_pub); EVP_PKEY_free(pkey); throw RuntimeError("BIO_new failed"); }

		if (!PEM_write_bio_PrivateKey(bio_priv, pkey, nullptr, nullptr, 0, nullptr, nullptr)) { BIO_free(bio_priv); BIO_free(bio_pub); EVP_PKEY_free(pkey); throw RuntimeError("PEM_write_bio_PrivateKey failed"); }
		if (!PEM_write_bio_PUBKEY(bio_pub, pkey)) { BIO_free(bio_priv); BIO_free(bio_pub); EVP_PKEY_free(pkey); throw RuntimeError("PEM_write_bio_PUBKEY failed"); }

		char* priv_data = nullptr;
		long priv_len = BIO_get_mem_data(bio_priv, &priv_data);
		std::string priv_pem(priv_data, priv_len);

		char* pub_data = nullptr;
		long pub_len = BIO_get_mem_data(bio_pub, &pub_data);
		std::string pub_pem(pub_data, pub_len);

		BIO_free(bio_priv);
		BIO_free(bio_pub);
		EVP_PKEY_free(pkey);
		return { pub_pem, priv_pem };
	}

	std::vector<unsigned char> Encrypt::rsa_private_decrypt_pem(std::string_view private_pem, const std::vector<unsigned char>& cipher) 
	{
		BIO* bio = BIO_new_mem_buf(private_pem.data(), static_cast<int>(private_pem.size()));
		if (!bio)
			throw RuntimeError("BIO_new_mem_buf failed");
		EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
		BIO_free(bio);
		if (!pkey)
			throw RuntimeError(std::string("PEM_read_bio_PrivateKey failed: ") + openssl_err());

		EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
		if (!ctx)
		{
			EVP_PKEY_free(pkey);
			throw RuntimeError("EVP_PKEY_CTX_new failed");
		}
		if (EVP_PKEY_decrypt_init(ctx) <= 0)
		{
			EVP_PKEY_free(pkey);
			EVP_PKEY_CTX_free(ctx);
			throw RuntimeError("EVP_PKEY_decrypt_init failed");
		}
		if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) 
		{
			EVP_PKEY_free(pkey);
			EVP_PKEY_CTX_free(ctx);
			throw RuntimeError("EVP_PKEY_CTX_set_rsa_padding failed"); 
		}

		size_t outlen = 0;
		if (EVP_PKEY_decrypt(ctx, nullptr, &outlen, cipher.data(), cipher.size()) <= 0) 
		{
			EVP_PKEY_free(pkey);
			EVP_PKEY_CTX_free(ctx);
			throw RuntimeError("EVP_PKEY_decrypt size query failed"); 
		}
		std::vector<unsigned char> out(outlen);
		if (EVP_PKEY_decrypt(ctx, out.data(), &outlen, cipher.data(), cipher.size()) <= 0) 
		{
			EVP_PKEY_free(pkey);
			EVP_PKEY_CTX_free(ctx);
			throw RuntimeError(std::string("EVP_PKEY_decrypt failed: ") + openssl_err());
		}
		out.resize(outlen);
		EVP_PKEY_free(pkey);
		EVP_PKEY_CTX_free(ctx);
		return out;
	}
} // namespace HsBa::Slicer::Cipher