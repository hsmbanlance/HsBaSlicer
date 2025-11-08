#pragma once

#include <string>
#include <vector>

namespace HsBa::Slicer::Cipher {

	class Encrypt 
	{
	public:
		// Encrypt plaintext with password using AES-256-CBC. Returns raw cipher bytes.
		static std::vector<unsigned char> aes256_cbc_encrypt(const std::vector<unsigned char>& plaintext, std::string_view password);

		// Decrypt raw cipher bytes with password. Returns plaintext bytes.
		static std::vector<unsigned char> aes256_cbc_decrypt(const std::vector<unsigned char>& cipher, std::string_view password);

		// AES-256 ECB (no IV) using password-derived key
		static std::vector<unsigned char> aes256_ecb_encrypt(const std::vector<unsigned char>& plaintext, std::string_view password);
		static std::vector<unsigned char> aes256_ecb_decrypt(const std::vector<unsigned char>& cipher, std::string_view password);

		// AES-256 CBC with explicit IV (IV should be 16 bytes)
		static std::vector<unsigned char> aes256_cbc_encrypt_with_iv(const std::vector<unsigned char>& plaintext, std::string_view password, const std::vector<unsigned char>& iv);
		static std::vector<unsigned char> aes256_cbc_decrypt_with_iv(const std::vector<unsigned char>& cipher, std::string_view password, const std::vector<unsigned char>& iv);

		// 3DES (DES-EDE3) ECB/CBC. For CBC, IV should be 8 bytes. Key derived from password (24 bytes).
		static std::vector<unsigned char> des3_ecb_encrypt(const std::vector<unsigned char>& plaintext, std::string_view password);
		static std::vector<unsigned char> des3_ecb_decrypt(const std::vector<unsigned char>& cipher, std::string_view password);
		static std::vector<unsigned char> des3_cbc_encrypt_with_iv(const std::vector<unsigned char>& plaintext, std::string_view password, const std::vector<unsigned char>& iv);
		static std::vector<unsigned char> des3_cbc_decrypt_with_iv(const std::vector<unsigned char>& cipher, std::string_view password, const std::vector<unsigned char>& iv);

		// RSA: public key PEM encrypt (OAEP), private key PEM decrypt (OAEP)
		static std::vector<unsigned char> rsa_public_encrypt_pem(std::string_view public_pem, const std::vector<unsigned char>& plaintext);
		static std::vector<unsigned char> rsa_private_decrypt_pem(std::string_view private_pem, const std::vector<unsigned char>& cipher);

		// RSA keypair generation (returns {public_pem, private_pem})
		static std::pair<std::string, std::string> rsa_generate_keypair_pem(int bits = 2048);
	};

} // namespace namespace HsBa::Slicer::Cipher
