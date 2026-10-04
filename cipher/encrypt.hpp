/** @file encrypt.hpp
 * @brief Symmetric (AES/3DES), RSA and keypair helpers for data encryption in the Cipher module.
 * @author HsBa
 */
#pragma once

#include <string>
#include <vector>

namespace HsBa::Slicer::Cipher
{
inline constexpr size_t AES_KEY_SIZE = 32;        ///< AES key size in bytes (AES-256).
inline constexpr size_t AES_IV_SIZE = 16;         ///< AES IV/block size in bytes.
inline constexpr size_t DES3_KEY_SIZE = 24;       ///< 3DES key size in bytes.
inline constexpr size_t DES3_IV_SIZE = 8;         ///< 3DES IV/block size in bytes.
inline constexpr size_t RSA_DEFAULT_BITS = 2048;  ///< Default RSA key length in bits.

/**
 * @class Encrypt
 * @brief Stateless password/PEM-based encryption helpers (AES-256, 3DES, RSA).
 *
 * Password-based AES/3DES derive their key from the password; block modes use CBC/ECB as named.
 * RSA uses PEM-encoded keys with OAEP padding.
 */
class Encrypt
{
public:
    /// Encrypt plaintext with a password using AES-256-CBC; returns raw cipher bytes.
    static std::vector<unsigned char> aes256_cbc_encrypt(const std::vector<unsigned char>& plaintext,
                                                         std::string_view password);

    /// Decrypt AES-256-CBC cipher bytes with a password; returns plaintext bytes.
    static std::vector<unsigned char> aes256_cbc_decrypt(const std::vector<unsigned char>& cipher,
                                                         std::string_view password);

    /// Encrypt with AES-256 ECB (no IV) using a password-derived key.
    static std::vector<unsigned char> aes256_ecb_encrypt(const std::vector<unsigned char>& plaintext,
                                                         std::string_view password);
    /// Decrypt AES-256 ECB (no IV) using a password-derived key.
    static std::vector<unsigned char> aes256_ecb_decrypt(const std::vector<unsigned char>& cipher,
                                                         std::string_view password);

    /// Encrypt with AES-256 CBC using an explicit IV (IV must be 16 bytes).
    static std::vector<unsigned char> aes256_cbc_encrypt_with_iv(const std::vector<unsigned char>& plaintext,
                                                                 std::string_view password,
                                                                 const std::vector<unsigned char>& iv);
    /// Decrypt AES-256 CBC using an explicit IV (IV must be 16 bytes).
    static std::vector<unsigned char> aes256_cbc_decrypt_with_iv(const std::vector<unsigned char>& cipher,
                                                                 std::string_view password,
                                                                 const std::vector<unsigned char>& iv);

    /// Encrypt with 3DES (DES-EDE3) ECB using a password-derived 24-byte key.
    static std::vector<unsigned char> des3_ecb_encrypt(const std::vector<unsigned char>& plaintext,
                                                       std::string_view password);
    /// Decrypt 3DES (DES-EDE3) ECB using a password-derived 24-byte key.
    static std::vector<unsigned char> des3_ecb_decrypt(const std::vector<unsigned char>& cipher,
                                                       std::string_view password);
    /// Encrypt with 3DES CBC using an explicit IV (IV must be 8 bytes).
    static std::vector<unsigned char> des3_cbc_encrypt_with_iv(const std::vector<unsigned char>& plaintext,
                                                               std::string_view password,
                                                               const std::vector<unsigned char>& iv);
    /// Decrypt 3DES CBC using an explicit IV (IV must be 8 bytes).
    static std::vector<unsigned char> des3_cbc_decrypt_with_iv(const std::vector<unsigned char>& cipher,
                                                               std::string_view password,
                                                               const std::vector<unsigned char>& iv);

    /// RSA-encrypt plaintext with a public key PEM using OAEP padding.
    static std::vector<unsigned char> rsa_public_encrypt_pem(std::string_view public_pem,
                                                             const std::vector<unsigned char>& plaintext);
    /// RSA-decrypt cipher bytes with a private key PEM using OAEP padding.
    static std::vector<unsigned char> rsa_private_decrypt_pem(std::string_view private_pem,
                                                              const std::vector<unsigned char>& cipher);

    /// Generate an RSA keypair; returns {public_pem, private_pem}.
    static std::pair<std::string, std::string> rsa_generate_keypair_pem(int bits = RSA_DEFAULT_BITS);
};

}  // namespace HsBa::Slicer::Cipher
