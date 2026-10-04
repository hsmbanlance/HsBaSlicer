/** @file hasher.hpp
 * @brief MD5/SHA1/SHA256 hashing helpers returning hexadecimal digests.
 * @author HsBa
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace HsBa::Slicer::Cipher
{

/**
 * @class Hasher
 * @brief Stateless cryptographic hash helpers (MD5, SHA1, SHA256) returning hex digests.
 */
class Hasher
{
public:
    static std::string md5_hex(const std::vector<unsigned char>& data);      ///< MD5 hex digest of the buffer.
    static std::string sha1_hex(const std::vector<unsigned char>& data);     ///< SHA1 hex digest of the buffer.
    static std::string sha256_hex(const std::vector<unsigned char>& data);   ///< SHA256 hex digest of the buffer.
    /// Compute the MD5 hex digest of a string view.
    inline static std::string md5_hex(std::string_view data)
    {
        return md5_hex(std::vector<unsigned char>(data.begin(), data.end()));
    }
    /// Compute the SHA1 hex digest of a string view.
    inline static std::string sha1_hex(const std::string_view data)
    {
        return sha1_hex(std::vector<unsigned char>(data.begin(), data.end()));
    }
    /// Compute the SHA256 hex digest of a string view.
    inline static std::string sha256_hex(const std::string_view data)
    {
        return sha256_hex(std::vector<unsigned char>(data.begin(), data.end()));
    }
};

}  // namespace HsBa::Slicer::Cipher
