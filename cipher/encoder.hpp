/** @file encoder.hpp
 * @brief Base64 and hexadecimal encoding/decoding helpers.
 * @author HsBa
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace HsBa::Slicer::Cipher
{

/**
 * @class Encoder
 * @brief Stateless Base64 and hexadecimal encoding/decoding helpers.
 */
class Encoder
{
public:
    /// Encode raw bytes to a Base64 string.
    static std::string base64_encode(const std::vector<unsigned char>& data);
    /// Encode a string view to a Base64 string.
    inline static std::string base64_encode(std::string_view data)
    {
        return base64_encode(std::vector<unsigned char>(data.begin(), data.end()));
    }
    /// Decode a Base64 string into raw bytes.
    static std::vector<unsigned char> base64_decode(std::string_view b64);
    /// Decode a Base64 string into a byte string.
    inline static std::string base64_decode_to_string(std::string_view b64)
    {
        auto vec = base64_decode(b64);
        return std::string(vec.begin(), vec.end());
    }

    /// Encode raw bytes to a hexadecimal string.
    static std::string hex_encode(const std::vector<unsigned char>& data);
    /// Encode a string view to a hexadecimal string.
    inline static std::string hex_encode(std::string_view data)
    {
        return hex_encode(std::vector<unsigned char>(data.begin(), data.end()));
    }
    /// Decode a hexadecimal string into raw bytes.
    static std::vector<unsigned char> hex_decode(std::string_view hex);
    /// Decode a hexadecimal string into a byte string.
    inline static std::string hex_decode_to_string(std::string_view hex)
    {
        auto vec = hex_decode(hex);
        return std::string(vec.begin(), vec.end());
    }
};

}  // namespace HsBa::Slicer::Cipher
